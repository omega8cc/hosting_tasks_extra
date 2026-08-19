<?php

/**
 * @file
 * Backend half of the migration source wiring.
 *
 * Two jobs, both inside the same provision-verify run so they succeed or fail
 * together:
 *   1. emit $databases['migrate'] into the generated settings.php;
 *   2. grant (or revoke) SELECT on the source database for this site's user.
 *
 * SECURITY MODEL. The frontend bounds which NODE may be named. It cannot bound
 * which DATABASE that node names: platforms live in a tenant-writable tree and
 * site import builds nodes from whatever settings file a platform contains, so
 * a tenant can own a node whose recorded database is any database on the box.
 * Everything that actually bounds the database is here, and every check is
 * answered by MySQL rather than by a file anyone can author:
 *
 *   (a) the source's own DB user must already hold a grant on the source
 *       database -- true for every genuine site, false for a manufactured one;
 *   (b) that user must not hold a global ON *.* grant -- otherwise naming the
 *       account superuser as the source user would satisfy (a) for anything;
 *   (c) the source must not be the front end's own database.
 *
 * Underneath sits the privilege boundary itself: issuing a grant needs
 * master_db, which is readable only by the account user that runs Aegir tasks,
 * not by the tenant's shell or web identity (proven on ng019, 2026-08-19). That
 * boundary is why a tenant cannot simply invoke this code themselves -- but it
 * is deliberately NOT the only defence, because it came within one failed
 * password rotation of not holding.
 */

/**
 * Implements hook_drush_init().
 */
function migrate_source_drush_init() {
  migrate_source_provision_register_autoload();
}

/**
 * Register our directory as a place to find provision classes.
 */
function migrate_source_provision_register_autoload() {
  static $loaded = FALSE;
  if (!$loaded) {
    $loaded = TRUE;
    $list = drush_commandfile_list();
    $provision_dir = dirname($list['provision']);
    if (is_readable($provision_dir . '/provision.inc')) {
      include_once($provision_dir . '/provision.inc');
      include_once($provision_dir . '/provision.service.inc');
      if (function_exists('provision_autoload_register_prefix')) {
        provision_autoload_register_prefix('Provision_', dirname(__FILE__));
      }
    }
  }
}

/**
 * Expose the service type this extension defines to provision.
 */
function migrate_source_provision_services() {
  migrate_source_provision_register_autoload();
  return array('migrate_source' => NULL);
}

/**
 * A safe SQL identifier.
 *
 * The db service's %s placeholder is PDO::quote() with the outer quotes
 * stripped: fine inside a string literal, NOT safe inside backticks, which is
 * where database and user names go. Provision only ever mints [A-Za-z0-9_]
 * names, so anything else is not ours to touch.
 */
function _migrate_source_valid_ident($name) {
  return (is_string($name) && $name !== '' && preg_match('/^[A-Za-z0-9_]+$/', $name) === 1);
}

/**
 * A safe grant host.
 */
function _migrate_source_valid_host($host) {
  return (is_string($host) && $host !== '' && preg_match('/^[A-Za-z0-9_.:%-]+$/', $host) === 1);
}

/**
 * The source database this site should read, or '' when there is none.
 */
function _migrate_source_wanted_db() {
  $db = d()->migrate_source_db;
  if (empty($db) || $db == 'null') {
    return '';
  }
  if (!_migrate_source_valid_ident($db)) {
    drush_log(dt('Migration source: refusing a source database name that is not a plain identifier.'), 'warning');
    return '';
  }
  return $db;
}

/**
 * Is this site Drupal 8 or newer?
 *
 * Backend twin of the frontend gate. Required independently: anything that
 * reaches the backend with the context set would otherwise emit the block on a
 * Drupal 7 site, whose fallback hash salt BOA derives from serialize($databases)
 * AFTER this block lands -- rotating the salt and killing every outstanding
 * one-time login link. Fails closed on an unknown version.
 */
function _migrate_source_target_is_d8plus() {
  $version = drush_drupal_major_version();
  return (!empty($version) && $version >= 8);
}

/**
 * Implements hook_provision_drupal_config().
 *
 * Lands in settings.php after $databases['default']['default'] is populated, so
 * the migrate connection is composed from the site's OWN credentials with only
 * the database name swapped. No new secret is written: copying the source's
 * user and password here would hand this site a credential for the source.
 *
 * The block is inert without the grant -- D8+ connects lazily -- so a verify
 * that emits it and then fails leaves a site that still serves.
 */
function migrate_source_provision_drupal_config($uri, $data) {
  if (d()->type != 'site') {
    return '';
  }
  $db = _migrate_source_wanted_db();
  if ($db === '') {
    return '';
  }
  if (!_migrate_source_target_is_d8plus()) {
    drush_log(dt('Migration source: not emitting the migrate connection on a pre-Drupal-8 site.'), 'warning');
    return '';
  }

  $lines = array();
  $lines[] = "# Migration source: read-only access to another of this client's sites.";
  $lines[] = "if (isset(\$databases['default']['default'])) {";
  $lines[] = "  \$databases['migrate']['default'] = \$databases['default']['default'];";
  $lines[] = "  \$databases['migrate']['default']['database'] = '" . $db . "';";
  $lines[] = "  \$databases['migrate']['default']['prefix'] = '';";
  $lines[] = "}";
  $lines[] = "";

  return implode("\n", $lines);
}

/**
 * Rows of SHOW GRANTS for one account, or FALSE when the query itself failed.
 *
 * A failed query and an empty result mean different things: empty is "no
 * grants", failed is "we do not know". Callers must fail closed on FALSE.
 */
function _migrate_source_show_grants($user, $host) {
  if (!_migrate_source_valid_ident($user) || !_migrate_source_valid_host($host)) {
    return FALSE;
  }
  $result = d()->service('db')->query("SHOW GRANTS FOR `%s`@`%s`", $user, $host);
  if ($result === FALSE) {
    return FALSE;
  }
  $rows = array();
  foreach ($result as $row) {
    $values = array_values((array) $row);
    if (isset($values[0])) {
      $rows[] = $values[0];
    }
  }
  return $rows;
}

/**
 * Does this set of grant statements include one on `$db`.*?
 *
 * Matches the ON clause only, never the TO clause: Percona 5.7 renders the
 * grantee 'u'@'h' and 8.x renders it `u`@`h`, so a whole-statement match is
 * non-idempotent on one of the two. Provision's own revoke() matches the same
 * way.
 */
function _migrate_source_grants_include_db($rows, $db) {
  if (!is_array($rows)) {
    return FALSE;
  }
  $pattern = '/^GRANT .+ ON `' . preg_quote($db, '/') . '`\.\*/';
  foreach ($rows as $statement) {
    if (preg_match($pattern, $statement) === 1) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * Does this set of grant statements include a global ON *.* grant?
 */
function _migrate_source_grants_include_global($rows) {
  if (!is_array($rows)) {
    return TRUE;
  }
  foreach ($rows as $statement) {
    if (preg_match('/^GRANT .+ ON \*\.\* /', $statement) === 1) {
      // USAGE ON *.* is the empty grant every account carries; it is not power.
      if (preg_match('/^GRANT USAGE ON \*\.\* /', $statement) !== 1) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

/**
 * The hosts this site's own DB user actually holds grants on.
 *
 * Discovered rather than assumed: grant() adds a 127.0.0.1 account on top of
 * whatever host was requested, so the site's real host set does not equal
 * grant_host_list(). Loopback only -- a wildcard or remote entry left behind by
 * older tooling must not be widened into network-reachable access.
 */
function _migrate_source_target_hosts($user) {
  $hosts = array();
  if (!_migrate_source_valid_ident($user)) {
    return $hosts;
  }
  $candidates = array('localhost', '127.0.0.1');
  foreach ($candidates as $host) {
    $rows = _migrate_source_show_grants($user, $host);
    if (is_array($rows) && !empty($rows)) {
      $hosts[] = $host;
    }
  }
  return $hosts;
}

/**
 * The front end's own database name, or '' if it cannot be determined.
 */
function _migrate_source_frontend_db() {
  $hostmaster = d('@hostmaster', FALSE, FALSE);
  if (!empty($hostmaster) && !empty($hostmaster->db_name)) {
    return $hostmaster->db_name;
  }
  return '';
}

/**
 * Check the three invariants that bound WHICH database may be granted.
 *
 * @return
 *   TRUE only when all three hold. Anything unknown is a refusal.
 */
function _migrate_source_source_is_grantable($db) {
  // (c) never the front end.
  $frontend_db = _migrate_source_frontend_db();
  if ($frontend_db !== '' && $frontend_db === $db) {
    drush_log(dt('Migration source: refusing the Aegir front-end database as a source.'), 'error');
    return FALSE;
  }

  // Provision names a site's DB user after its database by default; the alter
  // hook can change that, so try the default and accept the first identity that
  // actually exists in MySQL.
  $source_user = $db;
  if (!_migrate_source_valid_ident($source_user)) {
    return FALSE;
  }

  $found = FALSE;
  $candidates = array('localhost', '127.0.0.1');
  foreach ($candidates as $host) {
    $rows = _migrate_source_show_grants($source_user, $host);
    if ($rows === FALSE || empty($rows)) {
      continue;
    }
    $found = TRUE;
    // (b) the source's user must not be a global superuser.
    if (_migrate_source_grants_include_global($rows)) {
      drush_log(dt('Migration source: refusing a source whose database user holds global privileges.'), 'error');
      return FALSE;
    }
    // (a) the source's user must already own the source database.
    if (!_migrate_source_grants_include_db($rows, $db)) {
      drush_log(dt('Migration source: refusing a source database its own site does not own.'), 'error');
      return FALSE;
    }
  }

  if (!$found) {
    drush_log(dt('Migration source: could not confirm the source database belongs to one of your sites.'), 'error');
    return FALSE;
  }

  return TRUE;
}

/**
 * Revoke every migration-source grant this site's user holds except $keep_db.
 *
 * Driven from MySQL's own state, so it also clears grants left by a previous
 * source, by a rotated user, or by an interrupted run.
 */
function _migrate_source_revoke_others($user, $hosts, $own_db, $keep_db) {
  foreach ($hosts as $host) {
    $rows = _migrate_source_show_grants($user, $host);
    if ($rows === FALSE) {
      // Unknown state: do not guess. The sweep will reconcile later.
      continue;
    }
    foreach ($rows as $statement) {
      $matches = array();
      if (preg_match('/^GRANT SELECT ON `([A-Za-z0-9_]+)`\.\* /', $statement, $matches) !== 1) {
        continue;
      }
      $granted_db = $matches[1];
      if ($granted_db === $own_db || $granted_db === $keep_db) {
        continue;
      }
      d()->service('db')->query("REVOKE SELECT ON `%s`.* FROM `%s`@`%s`", $granted_db, $user, $host);
      drush_log(dt('Migration source: revoked read access to @db.', array('@db' => $granted_db)), 'ok');
    }
  }
}

/**
 * Implements drush_hook_post_provision_verify().
 *
 * Converges the grant with what the context asks for. Runs in the same
 * provision-verify as the settings write, so the two cannot diverge.
 */
function drush_migrate_source_post_provision_verify() {
  if (d()->type != 'site') {
    return;
  }

  $own_db = d()->db_name;
  $user = d()->db_user;
  if (!_migrate_source_valid_ident($user) || !_migrate_source_valid_ident($own_db)) {
    return;
  }

  $hosts = _migrate_source_target_hosts($user);
  if (empty($hosts)) {
    drush_log(dt('Migration source: no local grant hosts found for this site, skipping.'), 'warning');
    return;
  }

  $wanted = _migrate_source_wanted_db();

  // Never grant onto ourselves, and never emit a self-referential wiring.
  if ($wanted === $own_db) {
    $wanted = '';
  }
  if ($wanted !== '' && !_migrate_source_target_is_d8plus()) {
    $wanted = '';
  }
  if ($wanted !== '' && !_migrate_source_source_is_grantable($wanted)) {
    $wanted = '';
  }

  // Clear anything that is no longer wanted first, then add what is.
  _migrate_source_revoke_others($user, $hosts, $own_db, $wanted);

  if ($wanted === '') {
    return;
  }

  foreach ($hosts as $host) {
    $rows = _migrate_source_show_grants($user, $host);
    if ($rows === FALSE) {
      continue;
    }
    if (_migrate_source_grants_include_db($rows, $wanted)) {
      continue;
    }
    $ok = d()->service('db')->query("GRANT SELECT ON `%s`.* TO `%s`@`%s`", $wanted, $user, $host);
    if ($ok === FALSE) {
      drush_log(dt('Migration source: failed to grant read access to @db.', array('@db' => $wanted)), 'error');
    }
    else {
      drush_log(dt('Migration source: granted read access to @db.', array('@db' => $wanted)), 'ok');
    }
  }
}

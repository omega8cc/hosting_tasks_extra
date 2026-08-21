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
 * BE PRECISE ABOUT WHAT THAT PROVES. Together these establish that the source is
 * a GENUINE, non-superuser site database on this server -- they do NOT establish
 * that it belongs to the requesting account, because MySQL holds no ownership
 * fact to ask. Ownership is enforced on the frontend, where the association is
 * chosen and re-validated (client, db_server, node access), and it is reconciled
 * afterwards by the sweep, which revokes any grant this instance does not
 * intend. Do not write, here or in a log line, that these checks confirm the
 * source belongs to the caller. If a future change weakens the frontend or
 * unschedules the sweep, this is the boundary that does NOT catch it.
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
 * NEVER pass an escaped database name through this service's %s placeholder.
 *
 * provision's query() renders %s as substr(PDO::quote($v), 1, -1), and PDO's
 * MySQL quoting escapes the backslash -- so the `foo\_bar` this file produces
 * arrives at the server as `foo\\_bar`, which is a grant on a name containing a
 * literal backslash and grants access to NOTHING. Measured on ng019 2026-08-21:
 * the resulting grant let the user read neither foo_bar nor its wildcard
 * neighbour. The escaped identifier is therefore interpolated directly, which is
 * safe only because _migrate_source_valid_ident() has already constrained the
 * name to ^[A-Za-z0-9_]+$ -- keep that check in front of every such call.
 */

/**
 * Escape a database name for the ON clause of a GRANT or REVOKE.
 *
 * MySQL reads _ and % in that position as LIKE wildcards, so an unescaped name
 * grants on a PATTERN rather than on one database: a grant on `site_0` also
 * covers `siteX0` for any X. BOA mints exactly such names -- provision's
 * suggest_db_name() appends _<n> whenever a 16-character truncation collides --
 * so this escape is what keeps a grant to the one database it names. Verified
 * live on Percona 5.7.44 and 8.4.10 (2026-08-20): a grant on `wcq_a`.* read
 * wcqxa on both.
 *
 * Both spellings must keep matching FOREVER: grants issued before this escaping
 * existed are stored unescaped and stay that way until revoked.
 * _migrate_source_unescape_db() is the other half, and the sweep carries its own
 * copy of the same idea in _hosting_migrate_source_db_spellings().
 */
function _migrate_source_escape_db($db) {
  return str_replace(array('_', '%'), array('\_', '\%'), $db);
}

/**
 * Strip GRANT escaping from a database name read back out of MySQL.
 */
function _migrate_source_unescape_db($db) {
  return str_replace(array('\_', '\%'), array('_', '%'), $db);
}

/**
 * Does this set of grant statements include one on `$db`.*?
 *
 * Matches the ON clause only, never the TO clause: Percona 5.7 renders the
 * grantee 'u'@'h' and 8.x renders it `u`@`h`, so a whole-statement match is
 * non-idempotent on one of the two. Provision's own revoke() matches the same
 * way.
 *
 * Accepts the escaped and unescaped spelling of the name, because a grant issued
 * before the escaping existed is still a grant.
 */
function _migrate_source_grants_include_db($rows, $db) {
  if (!is_array($rows)) {
    return FALSE;
  }
  $patterns = array('/^GRANT .+ ON `' . preg_quote($db, '/') . '`\.\*/');
  $escaped = _migrate_source_escape_db($db);
  if ($escaped !== $db) {
    $patterns[] = '/^GRANT .+ ON `' . preg_quote($escaped, '/') . '`\.\*/';
  }
  foreach ($rows as $statement) {
    foreach ($patterns as $pattern) {
      if (preg_match($pattern, $statement) === 1) {
        return TRUE;
      }
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
 *
 * The hostmaster alias carries site_path but NOT db_name (verified on a live
 * box), so the name is read out of the front end's own drushrc.php. That file
 * is never included -- it runs code -- only regex-scanned, the way provision
 * reads a sibling site's settings elsewhere.
 *
 * Reaching it at all requires the hostmaster alias, which is owner-read-only to
 * the account user that runs Aegir tasks, so this resolves for the legitimate
 * runner and returns '' for anyone else. An empty return is NOT fatal: on a
 * real box the front end's database user holds global privileges, so invariant
 * (b) refuses it independently of this check. Verified on ng019 2026-08-19 --
 * the front-end user carries GRANT ... ON *.* WITH GRANT OPTION.
 */
function _migrate_source_frontend_db() {
  $hostmaster = d('@hostmaster', FALSE, FALSE);
  if (empty($hostmaster) || empty($hostmaster->site_path)) {
    return '';
  }
  $drushrc = $hostmaster->site_path . '/drushrc.php';
  if (!is_readable($drushrc)) {
    return '';
  }
  $contents = @file_get_contents($drushrc);
  if ($contents === FALSE || $contents === '') {
    return '';
  }
  $matches = array();
  // Anchored at line start so a commented copy cannot shadow the assignment.
  if (preg_match('/^\$options\[\'db_name\'\]\s*=\s*\'([A-Za-z0-9_]+)\'/m', $contents, $matches) === 1) {
    return $matches[1];
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
  // (c) never the front end. Note this is not the only thing standing between a
  // tenant and the front-end database: its user holds global privileges, so (b)
  // refuses it too. Both are kept because neither resolves in every situation.
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
    drush_log(dt('Migration source: @db does not look like a live site database on this server (no site user of its own holds a grant on it), so it was not wired up.', array('@db' => $db)), 'error');
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
      // Backslash is in the class because an escaped grant reads back escaped.
      if (preg_match('/^GRANT SELECT ON `([A-Za-z0-9_%\\\\]+)`\.\* /', $statement, $matches) !== 1) {
        continue;
      }
      // Compare on the plain name, but REVOKE with the spelling MySQL stored --
      // revoking a differently-spelled name is simply a grant that does not exist.
      $granted_raw = $matches[1];
      $granted_db = _migrate_source_unescape_db($granted_raw);
      if ($granted_db === $own_db || $granted_db === $keep_db) {
        continue;
      }
      if (!_migrate_source_valid_ident($granted_db)) {
        continue;
      }
      // Interpolated for the same reason as the GRANT above: %s would
      // double-escape the backslash and revoke a grant that does not exist.
      d()->service('db')->query("REVOKE SELECT ON `" . $granted_raw . "`.* FROM `%s`@`%s`", $user, $host);
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

  // The site's own credentials come from the site OPTION context (its
  // drushrc.php), not from the context object: the site alias deliberately
  // carries no database credentials, so d()->db_name is empty here. This is the
  // same accessor provision's own fetch_site_credentials() uses.
  $own_db = drush_get_option('db_name', '', 'site');
  $user = drush_get_option('db_user', '', 'site');
  if (!_migrate_source_valid_ident($user) || !_migrate_source_valid_ident($own_db)) {
    drush_log(dt('Migration source: could not read this site\'s own database credentials, skipping.'), 'warning');
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

  // Clear anything that is no longer wanted first, then add what is. $wanted is
  // threaded through as keep_db, and that is the ONLY reason revoke-then-grant
  // is idempotent rather than a revoke followed by a re-grant on every run. It
  // holds only because every gate that can zero $wanted runs ABOVE this line --
  // a gate added below it would revoke and immediately re-grant, once per verify.
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
    // Escaped: an unescaped _ would make this a grant on a PATTERN of database
    // names rather than on the one named. See _migrate_source_escape_db(). The
    // name is interpolated, NOT passed as %s -- %s would double-escape it into a
    // grant on nothing at all.
    $ok = d()->service('db')->query("GRANT SELECT ON `" . _migrate_source_escape_db($wanted) . "`.* TO `%s`@`%s`", $user, $host);
    if ($ok === FALSE) {
      drush_log(dt('Migration source: failed to grant read access to @db.', array('@db' => $wanted)), 'error');
    }
    else {
      drush_log(dt('Migration source: granted read access to @db.', array('@db' => $wanted)), 'ok');
    }
  }
}

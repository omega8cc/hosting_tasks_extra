<?php

/**
 * @file
 * Frontend-side backend plumbing for the migration source association.
 */

/**
 * Implements hook_hosting_site_context_options().
 *
 * Only verify and install carry provision_save for sites, and the
 * migrate_source task rides provision-verify, so this fires exactly when the
 * emitted settings block is regenerated.
 *
 * On the migrate_source run itself the association has not been persisted yet
 * (that happens in the post-task hook), so the task's own argument wins; every
 * later verify uses the stored association.
 */
function hosting_migrate_source_hosting_site_context_options(&$task) {
  $source_nid = 0;

  if (isset($task->task_type) && $task->task_type == 'migrate_source'
    && isset($task->task_args['source'])) {
    $source_nid = intval($task->task_args['source']);
  }
  elseif (isset($task->ref->migrate_source)) {
    $source_nid = intval($task->ref->migrate_source);
  }

  // Re-validate on every push: the association is long-lived and the client's
  // sites change under it. A source that is no longer eligible is cleared
  // rather than carried. Structural check only -- this runs inside a drush task
  // with no interactive user, where node_access() would evaluate against the
  // anonymous user and clear every association.
  if ($source_nid && !hosting_migrate_source_source_eligible($task->ref, $source_nid)) {
    // Say so. Clearing in silence is indistinguishable from having nothing to
    // do, which is exactly how a grant that vanished mid-migration became
    // invisible once already.
    drush_log(dt('Migration source: source site @nid is no longer eligible for @target; clearing the association for this run.', array(
      '@nid' => $source_nid,
      '@target' => !empty($task->ref->title) ? $task->ref->title : $task->ref->nid,
    )), 'warning');
    $source_nid = 0;
  }

  $db_name = '';
  $uri = '';
  if ($source_nid) {
    $source = node_load($source_nid);
    // hosting_site.db_name is the account-owned record of the source database,
    // refreshed from the backend on every verify/install/import. It is NOT
    // authoritative on its own -- the backend re-checks it against MySQL's own
    // privilege tables before granting anything.
    if (!empty($source->db_name)) {
      $db_name = $source->db_name;
      $uri = !empty($source->hosting_name) ? $source->hosting_name : $source->title;
    }
  }

  // 'null' is the house idiom for "unset this in the backend context"; an empty
  // string would be retained.
  $task->context_options['migrate_source_db'] = ($db_name === '') ? 'null' : $db_name;
  $task->context_options['migrate_source_uri'] = ($uri === '') ? 'null' : $uri;
}

/**
 * Implements hook_drush_command().
 */
function hosting_migrate_source_drush_command() {
  $items = array();
  $items['hosting-migrate-source-sweep'] = array(
    'description' => 'Reconcile migration-source grants against what this Aegir instance actually intends.',
    'callback' => 'hosting_migrate_source_sweep',
    'options' => array(
      'dry-run' => 'Report what would change without changing anything.',
      'max-revokes' => 'Refuse to revoke more than this many grants in one pass (default 25). A guard against acting on corrupt or empty intent.',
      'force' => 'Ignore the max-revokes guard.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'aliases' => array('hosting-mss'),
  );
  return $items;
}

/**
 * Reconcile grants with intent.
 *
 * WHY THIS EXISTS. The verify path converges a site whenever it runs, but four
 * things it cannot see leave grants behind:
 *   - a deleted source's database is dropped while the grant survives, and
 *     database names are global and derived from a 16-character truncation of
 *     the URI, so the name can later be taken by a site in ANOTHER account;
 *   - deleting a site revokes only its own database and skips DROP USER when
 *     any other grant remains, so the user outlives the site still holding one;
 *   - restore and deploy rotate the site's database user, orphaning the grant
 *     the superseded user held;
 *   - a crash between issuing a grant and recording it leaves a grant no
 *     intent-driven pass can find.
 *
 * WHAT BOUNDS IT. This runs with the account user's connection, which holds
 * ALL ON *.* WITH GRANT OPTION -- it could revoke anything on the box. So it
 * never enumerates users: it enumerates GRANTS ON THE DATABASES OF THIS
 * INSTANCE'S OWN SITES, and considers nothing else. Another account's databases
 * are never read and never touched.
 *
 * WHAT IT WILL NOT DO. It never issues a GRANT. A missing grant queues a verify
 * instead, so the backend remains the single place a grant can be created, with
 * its invariant checks intact.
 */
/**
 * Both spellings a database name can have in MySQL's privilege tables.
 *
 * GRANT reads _ and % in a database name as LIKE wildcards, so the backend
 * escapes them when it issues one -- but every grant issued before that change
 * is stored unescaped, and both must keep matching here forever. A sweep that
 * knew only one spelling would silently ignore half the grants it exists to
 * find, which is worse than not sweeping at all.
 *
 * Callers pass names already validated as ^[A-Za-z0-9_]+$, so there is no
 * backslash of their own to escape first.
 */
function _hosting_migrate_source_db_spellings($db) {
  $escaped = str_replace(array('_', '%'), array('\_', '\%'), $db);
  return ($escaped === $db) ? array($db) : array($db, $escaped);
}

function hosting_migrate_source_sweep() {
  $dry = drush_get_option('dry-run', FALSE);
  $max = (int) drush_get_option('max-revokes', 25);
  $force = drush_get_option('force', FALSE);

  // --- our own databases -------------------------------------------------
  // Only sites of THIS frontend, and never the front end's own database.
  $hostmaster_nid = hosting_migrate_source_hostmaster_nid();
  if (empty($hostmaster_nid)) {
    return drush_set_error('MIGRATE_SOURCE_NO_FRONTEND', dt('Cannot identify the Aegir front-end site; refusing to sweep.'));
  }

  $ours = array();
  $result = db_query("SELECT n.nid, n.title, s.db_name FROM {node} n INNER JOIN {hosting_site} s ON n.vid = s.vid WHERE n.type = 'site' AND s.status <> :deleted AND n.nid <> :hm AND s.db_name <> ''", array(':deleted' => HOSTING_SITE_DELETED, ':hm' => $hostmaster_nid));
  foreach ($result as $row) {
    if (preg_match('/^[A-Za-z0-9_]+$/', $row->db_name) !== 1) {
      continue;
    }
    $ours[$row->db_name] = $row->nid;
  }
  if (empty($ours)) {
    drush_log(dt('Migration source sweep: this instance has no site databases to examine.'), 'ok');
    return;
  }

  // --- what we intend ----------------------------------------------------
  // Join on the CURRENT revision: the association table is keyed on vid. The
  // client and db_server equality carry the rest of the intent: a source moved
  // to another client, or onto another database server, stops being intended
  // the moment it moves, and this pass is then what takes the grant back.
  $intended = array();
  $result = db_query("SELECT m.nid AS target_nid, m.source_nid, ts.db_name AS target_db, ss.db_name AS source_db
    FROM {hosting_migrate_source} m
    INNER JOIN {node} n ON n.nid = m.nid AND n.vid = m.vid
    INNER JOIN {hosting_site} ts ON ts.vid = n.vid
    INNER JOIN {node} sn ON sn.nid = m.source_nid
    INNER JOIN {hosting_site} ss ON ss.vid = sn.vid
    WHERE m.source_nid <> 0 AND ts.status <> :deleted AND ss.status <> :deleted
      AND ts.client = ss.client AND ts.db_server = ss.db_server", array(':deleted' => HOSTING_SITE_DELETED));
  foreach ($result as $row) {
    if ($row->target_db === '' || $row->source_db === '') {
      continue;
    }
    // key: "<grantee user>|<source database>"
    $intended[$row->target_db . '|' . $row->source_db] = $row->target_nid;
  }

  // --- what actually exists ----------------------------------------------
  $revokes = array();
  foreach (array_keys($ours) as $db) {
    // Db comes back as STORED, which may be the escaped spelling; a REVOKE has
    // to name the grant exactly as it was issued or MySQL reports no such grant.
    $rows = db_query("SELECT User AS u, Host AS h, Db AS d FROM mysql.db WHERE Db IN (:dbs) AND Select_priv = 'Y'", array(':dbs' => _hosting_migrate_source_db_spellings($db)));
    foreach ($rows as $g) {
      // The database's own site user is always legitimate.
      if ($g->u === $db) {
        continue;
      }
      if (preg_match('/^[A-Za-z0-9_]+$/', $g->u) !== 1) {
        drush_log(dt('Migration source sweep: skipping grantee @u on @db, not a plain identifier.', array('@u' => $g->u, '@db' => $db)), 'warning');
        continue;
      }
      if (preg_match('/^[A-Za-z0-9_%\\\\]+$/', $g->d) !== 1) {
        drush_log(dt('Migration source sweep: skipping a grant on @db, stored under an unrecognised name.', array('@db' => $db)), 'warning');
        continue;
      }
      if (isset($intended[$g->u . '|' . $db])) {
        continue;
      }
      $revokes[] = array('user' => $g->u, 'host' => $g->h, 'db' => $g->d, 'label' => $db);
    }
  }

  // --- guard against acting on corrupt or empty intent --------------------
  $failed = 0;
  if (count($revokes) > $max && !$force) {
    return drush_set_error('MIGRATE_SOURCE_TOO_MANY', dt('Migration source sweep: @n grants look unjustified, which is more than the @max guard allows. Refusing to act -- check the association table before re-running with --force.', array('@n' => count($revokes), '@max' => $max)));
  }

  foreach ($revokes as $r) {
    if (preg_match('/^[A-Za-z0-9_.:%-]+$/', $r['host']) !== 1) {
      drush_log(dt('Migration source sweep: skipping an unrecognised host for @u.', array('@u' => $r['user'])), 'warning');
      continue;
    }
    if ($dry) {
      drush_log(dt('Migration source sweep [dry-run]: would revoke @g read access to @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
      continue;
    }
    // One REVOKE that fails must not take the pass down with it. D7 db_query()
    // rethrows PDOException, and a grant revoked concurrently between the
    // enumeration above and this line -- the target's own verify does exactly
    // that, and BOA boxes run verify storms -- would otherwise skip every later
    // revoke AND the whole missing-grant pass below. Exception, never Throwable:
    // Throwable is PHP 7.0+ and would fatal on the 5.6 floor.
    try {
      db_query('REVOKE SELECT ON `' . $r['db'] . '`.* FROM `' . $r['user'] . '`@`' . $r['host'] . '`');
      drush_log(dt('Migration source sweep: revoked @g read access to @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
    }
    catch (Exception $e) {
      $failed++;
      drush_log(dt('Migration source sweep: could not revoke @g on @db (@msg); continuing.', array(
        '@g' => $r['user'] . '@' . $r['host'],
        '@db' => $r['label'],
        '@msg' => $e->getMessage(),
      )), 'warning');
    }
  }

  // --- the other direction: intended but missing --------------------------
  // Never granted here. A verify re-runs the backend converge, which owns the
  // only path that may create a grant.
  $queued = 0;
  foreach ($intended as $key => $target_nid) {
    $parts = explode('|', $key);
    $target_db = $parts[0];
    $source_db = $parts[1];
    $found = db_query("SELECT COUNT(*) FROM mysql.db WHERE Db IN (:dbs) AND User = :u AND Select_priv = 'Y'", array(':dbs' => _hosting_migrate_source_db_spellings($source_db), ':u' => $target_db))->fetchField();
    if ($found > 0) {
      continue;
    }
    if ($dry) {
      drush_log(dt('Migration source sweep [dry-run]: would queue a verify for site @nid to restore its missing grant.', array('@nid' => $target_nid)), 'ok');
      continue;
    }
    if (hosting_task_outstanding($target_nid, 'delete') || hosting_task_outstanding($target_nid, 'verify')) {
      continue;
    }
    hosting_add_task($target_nid, 'verify');
    $queued++;
    drush_log(dt('Migration source sweep: queued a verify for site @nid to restore its missing grant.', array('@nid' => $target_nid)), 'ok');
  }

  drush_log(dt('Migration source sweep: @o databases examined, @i associations intended, @r revoked, @f revokes failed, @q verifies queued.', array(
    '@o' => count($ours),
    '@i' => count($intended),
    '@r' => $dry ? 0 : (count($revokes) - $failed),
    '@f' => $failed,
    '@q' => $queued,
  )), 'ok');
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_migrate_source_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    if (isset($context->migrate_source_db) && !empty($context->migrate_source_db)) {
      $node->migrate_source_db = $context->migrate_source_db;
    }
  }
}

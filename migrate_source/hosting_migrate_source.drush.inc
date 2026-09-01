<?php

/**
 * Consecutive sweeps that may re-queue a verify for the same missing grant
 * before the association is left to a human. Prevents an intent the backend
 * will never satisfy from generating a provision-verify on every pass forever.
 */
define('HOSTING_MIGRATE_SOURCE_MAX_RETRIES', 3);

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
    drush_log(dt('Migration source: source site @nid is no longer eligible for @target; clearing the association.', array(
      '@nid' => $source_nid,
      '@target' => !empty($task->ref->title) ? $task->ref->title : $task->ref->nid,
    )), 'warning');
    // PERSIST it. Clearing only "for this run" left the stored row still naming
    // the ineligible source, so the sweep went on believing the association was
    // intended and re-queued a verify to restore a grant this push had just
    // declined to ask for -- every pass, forever. Writing the row makes the
    // record agree with what was actually pushed.
    if (function_exists('hosting_migrate_source_write') && !empty($task->ref->vid)) {
      hosting_migrate_source_write($task->ref->vid, $task->ref->nid, 0);
    }
    // The row write alone does not survive a verify: hosting_site's own
    // post-task hook node_save()s this very ref, and hook_node_update()
    // re-writes the association FROM THE NODE OBJECT -- which still carries
    // the value it was loaded with, before this clear. Observed live on a
    // client-move: the clear warned and wrote 0, and the completion save
    // restored the old source within the same task -- leaving a record that
    // would silently resurrect the association (and its grant) if the source
    // ever moved back. Clearing the in-memory field makes the copy-back
    // carry the same 0.
    $task->ref->migrate_source = 0;
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

  // The latch the backend's revoke pass reads. Emitted ONLY when a source is
  // actually being pushed, and never unset -- retention is the whole point,
  // since provision-save keeps an option it is not told about, so the alias
  // carries the latch through the very push that CLEARS the association, which
  // is the push whose revoke has to run. Nothing clears it: a grant issued once
  // can still be outstanding.
  if ($db_name !== '') {
    $task->context_options['migrate_source_wired'] = '1';
  }
}

/**
 * Implements drush_hook_pre_hosting_task().
 *
 * Record a migrate target's about-to-be-superseded database name BEFORE the
 * task dispatches. It cannot be recorded afterwards: hosting_migrate's own
 * post-task hook refreshes the frontend record to the NEW name before this
 * module's hooks run (module order), so a post-task recording captures the
 * live name instead of the superseded one -- observed live on a rename. If
 * the migrate then fails, a still-live name sits on the record; that is the
 * safe direction, since a proven user outranks the record and the entry
 * expires.
 */
function drush_hosting_migrate_source_pre_hosting_task() {
  $task = &drush_get_context('HOSTING_TASK');
  if (!isset($task->ref) || !isset($task->ref->type) || $task->ref->type != 'site') {
    return;
  }
  if (!isset($task->task_type) || $task->task_type != 'migrate') {
    return;
  }
  if (!empty($task->ref->db_name) && function_exists('hosting_migrate_source_record_former_name')) {
    hosting_migrate_source_record_former_name($task->ref->db_name);
  }
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
      // NOT --force: the queue command this sweep is armed on
      // (hosting-migrate_source) documents --force as its queue-lock bypass,
      // and options are global to a drush invocation -- so an operator
      // unsticking the queue would have disarmed the max-revokes guard.
      'force-revokes' => 'Ignore the max-revokes guard.',
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
 * never enumerates globally: one direction reads the grants HELD BY THIS
 * INSTANCE'S OWN PROVEN USERS, the other direction the grants ON ITS OWN
 * PROVEN DATABASES. Both are bounded by this instance's own records as well as
 * by proof: the first may revoke only on a database this instance records for a
 * site of its own, live or former -- a grant one of our users holds on anything
 * else is reported and left -- and the second only from a grantee those records
 * name as a former site database. Another account's databases and users are
 * never read and never touched.
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
/**
 * The plain name behind a stored spelling (the escape is MySQL's, not ours).
 */
function _hosting_migrate_source_unescape_db($db) {
  return str_replace(array('\_', '\%'), array('_', '%'), $db);
}

function _hosting_migrate_source_db_spellings($db) {
  $escaped = str_replace(array('_', '%'), array('\_', '\%'), $db);
  return ($escaped === $db) ? array($db) : array($db, $escaped);
}

/**
 * TRUE when a drushrc db_host/db_port pair names this box's own MySQL server.
 * Mirror of the backend helper; PHP 5.6-safe.
 */
function _hosting_migrate_source_local_db_endpoint($host, $port) {
  $h = strtolower(trim((string) $host));
  if ($h !== '' && $h !== 'localhost' && $h !== '127.0.0.1' && $h !== '::1') {
    return FALSE;
  }
  if ($port !== '' && preg_match('/^[0-9]{1,5}$/', (string) $port) !== 1) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Prove a site's real database identity, or FALSE.
 *
 * Returns array('user' => ..., 'db' => ...) only when the credentials that site
 * records for itself actually OPEN the database it claims. Memoised per pass.
 *
 * WHY THIS EXISTS. The sweep needs to know which MySQL users belong to this
 * instance before it revokes anything, and the frontend has no db_user column,
 * so the only available answers are hosting_site.db_name (a cache the module
 * itself documents as going stale, and one a tenant can influence through an
 * imported platform) or the site's own drushrc (which lives in a tenant-writable
 * tree). Trusting either turned the destructive pass into something a tenant
 * could aim: a manufactured or stale db_name pointed it at another account's
 * database, and a manufactured db_user pointed it at another account's user.
 *
 * Testing the pair removes the choice. A forged password does not connect, and
 * real credentials aimed at somebody else's database do not either, so what
 * comes back is a fact rather than a claim. Same reasoning, and the same
 * safety, as invariant (d) on the grant side.
 */

/**
 * The database a site claims for itself when its probe could NOT be completed.
 *
 * Two failures reach the same FALSE and mean opposite things. An access-family
 * refusal IS an answer: the credentials that site records do not open the
 * database it claims, so the pair is not ours. A connect timeout, a 1040, a
 * server restart mid-pass answer nothing at all -- and a caller that revokes on
 * the strength of the second takes a live association's grant away on a blip.
 * Set by the probe, read by the one caller that can act destructively on the
 * difference.
 *
 * Returns '' when the answer was a real one, or when there was no probe.
 */
function _hosting_migrate_source_probe_unknown($nid, $db = NULL) {
  static $unknown = array();
  if ($db !== NULL) {
    $unknown[$nid] = $db;
  }
  return isset($unknown[$nid]) ? $unknown[$nid] : '';
}

function _hosting_migrate_source_prove_site($nid) {
  static $proven = array();
  if (isset($proven[$nid])) {
    return $proven[$nid];
  }
  $proven[$nid] = FALSE;

  $site = node_load($nid);
  if (empty($site) || $site->type != 'site' || empty($site->platform)) {
    return FALSE;
  }
  $platform = node_load($site->platform);
  if (empty($platform) || empty($platform->publish_path)) {
    return FALSE;
  }
  $uri = !empty($site->hosting_name) ? $site->hosting_name : $site->title;
  if ($uri === '' || strpos($uri, '/') !== FALSE || strpos($uri, '..') !== FALSE) {
    return FALSE;
  }
  $file = $platform->publish_path . '/sites/' . $uri . '/drushrc.php';
  if (!is_readable($file)) {
    return FALSE;
  }
  $contents = @file_get_contents($file);
  if ($contents === FALSE || $contents === '') {
    return FALSE;
  }

  $get = '_hosting_migrate_source_opt';
  $db = $get($contents, 'db_name');
  $user = $get($contents, 'db_user');
  $pass = $get($contents, 'db_passwd');
  $host = $get($contents, 'db_host');
  $port = $get($contents, 'db_port');
  if ($db === '' || $user === '' || $pass === ''
    || preg_match('/^[A-Za-z0-9_]+$/', $db) !== 1
    || preg_match('/^[A-Za-z0-9_]+$/', $user) !== 1) {
    return FALSE;
  }
  if ($host === '') {
    $host = 'localhost';
  }
  // Loopback only: the pair is taken as proof that $user belongs to THIS
  // instance on THIS server, which a tenant-reachable foreign server would
  // fake for any name. Same rule as the backend possession probe.
  if (!_hosting_migrate_source_local_db_endpoint($host, $port)) {
    return FALSE;
  }
  $dsn = 'mysql:host=' . $host . ';dbname=' . $db;
  if ($port !== '') {
    $dsn = 'mysql:host=' . $host . ';port=' . $port . ';dbname=' . $db;
  }
  try {
    $probe = new PDO($dsn, $user, $pass, array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_TIMEOUT => 5));
    $probe->query('SELECT 1');
    $probe = NULL;
  }
  catch (Exception $e) {
    // Never logged verbatim: the message can echo credentials.
    //
    // Classify it, though. 1044/1045/1049 -- no access to that database, bad
    // credentials, no such database -- are the server ANSWERING that this pair
    // is not ours. Anything else (the 5s connect timeout above, a 1040, a
    // socket that is not there, a non-PDO failure) is the server not being
    // reachable to ask. Remember which, so a caller that would revoke on the
    // strength of this FALSE can tell the two apart.
    $code = ($e instanceof PDOException && isset($e->errorInfo[1])) ? (int) $e->errorInfo[1] : 0;
    if ($code !== 1044 && $code !== 1045 && $code !== 1049) {
      _hosting_migrate_source_probe_unknown($nid, $db);
    }
    return FALSE;
  }
  $proven[$nid] = array('user' => $user, 'db' => $db);
  return $proven[$nid];
}

/**
 * One $options[...] scalar out of drushrc.php contents. Never includes the file.
 */
function _hosting_migrate_source_opt($contents, $key) {
  $matches = array();
  $pattern = '/^\$options\[\047' . preg_quote($key, '/') . '\047\]\s*=\s*\047([^\047]*)\047/m';
  if (preg_match($pattern, $contents, $matches) === 1) {
    return $matches[1];
  }
  return '';
}

function hosting_migrate_source_sweep() {
  $dry = drush_get_option('dry-run', FALSE);
  $max = (int) drush_get_option('max-revokes', 25);
  $force = drush_get_option('force-revokes', FALSE);

  // --- our own databases -------------------------------------------------
  // Only sites of THIS frontend, and never the front end's own database.
  $hostmaster_nid = hosting_migrate_source_hostmaster_nid();
  if (empty($hostmaster_nid)) {
    return drush_set_error('MIGRATE_SOURCE_NO_FRONTEND', dt('Cannot identify the Aegir front-end site; refusing to sweep.'));
  }

  // Every drop is counted: a site this loop cannot key never reaches the
  // proving pass, so its user's grants go unexamined, and a blind spot that
  // does not show up in any number is one nobody sizes.
  $ours = array();
  $cand_empty = 0;
  $cand_invalid = 0;
  $cand_dup = 0;
  $result = db_query("SELECT n.nid, n.title, s.db_name FROM {node} n INNER JOIN {hosting_site} s ON n.vid = s.vid WHERE n.type = 'site' AND s.status <> :deleted AND n.nid <> :hm", array(':deleted' => HOSTING_SITE_DELETED, ':hm' => $hostmaster_nid));
  foreach ($result as $row) {
    if ((string) $row->db_name === '') {
      // Routine for a site not yet installed; counted, not warned per site.
      $cand_empty++;
      continue;
    }
    if (preg_match('/^[A-Za-z0-9_]+$/', $row->db_name) !== 1) {
      $cand_invalid++;
      drush_log(dt('Migration source sweep: site @nid records a database name that is not a plain identifier; it was not examined.', array('@nid' => $row->nid)), 'warning');
      continue;
    }
    if (isset($ours[$row->db_name])) {
      $cand_dup++;
      drush_log(dt('Migration source sweep: sites @first and @nid both record database @db; only @first was examined.', array(
        '@first' => $ours[$row->db_name],
        '@nid' => $row->nid,
        '@db' => $row->db_name,
      )), 'warning');
      continue;
    }
    $ours[$row->db_name] = $row->nid;
  }
  if (empty($ours)) {
    drush_log(dt('Migration source sweep: this instance has no examinable site databases (@e without a recorded name, @v invalid, @d duplicates).', array(
      '@e' => $cand_empty,
      '@v' => $cand_invalid,
      '@d' => $cand_dup,
    )), 'ok');
    return;
  }

  // Every identity proven anywhere in this pass, keyed for the reclaim pass
  // below: a proven user's grants are reconciled by the user-side enumeration,
  // so the database-side pass must never touch them a second time.
  $proven_users = array();
  $proven_dbs = array();

  // --- what we intend ----------------------------------------------------
  // Join on the CURRENT revision: the association table is keyed on vid. The
  // client and db_server equality carry the rest of the intent: a source moved
  // to another client, or onto another database server, stops being intended
  // the moment it moves, and this pass is then what takes the grant back.
  $intended = array();
  $intended_meta = array();
  // Pairs whose SOURCE could not be asked on this pass. Not intent -- a stay of
  // execution, so a connection blip does not cost a live association its grant.
  // Only what is shielded NOW is written back, so a source that answers again
  // resets its own count and one that never answers runs out of passes.
  $shield = array();
  $shield_tries = variable_get('hosting_migrate_source_shielded', array());
  $shield_next = array();
  $shielded = 0;
  $result = db_query("SELECT m.nid AS target_nid, m.source_nid, ts.db_name AS target_db, ss.db_name AS source_db
    FROM {hosting_migrate_source} m
    INNER JOIN {node} n ON n.nid = m.nid AND n.vid = m.vid
    INNER JOIN {hosting_site} ts ON ts.vid = n.vid
    INNER JOIN {node} sn ON sn.nid = m.source_nid
    INNER JOIN {hosting_site} ss ON ss.vid = sn.vid
    WHERE m.source_nid <> 0 AND ts.status <> :deleted AND ss.status <> :deleted
      AND ts.client = ss.client AND ts.db_server = ss.db_server", array(':deleted' => HOSTING_SITE_DELETED));
  foreach ($result as $row) {
    // Both ends must PROVE their identity before an association can shield a
    // grant from revocation. Keying this on hosting_site.db_name alone let a
    // manufactured association -- a node whose recorded database is somebody
    // else's -- mark the resulting grant "intended" forever, so the sweep would
    // never take it back. Proving both ends means an association that does not
    // correspond to real, self-owned databases simply is not intended, and the
    // grant is revoked on this pass.
    $t = _hosting_migrate_source_prove_site($row->target_nid);
    $sc = _hosting_migrate_source_prove_site($row->source_nid);
    if ($t === FALSE || $sc === FALSE) {
      // An unprovable TARGET is already safe: the pass below never enumerates
      // its user, so nothing of its is revoked. An unprovable SOURCE is not --
      // the target's user IS enumerated, and its grant on that source database
      // is then revoked for want of an entry here. That is right when the
      // source ANSWERED (the association does not correspond to a real,
      // self-owned database, which is the case this proof exists for) and wrong
      // when it could not be asked at all. Shield that one pair instead, on the
      // database the source's own drushrc names, and only while the count below
      // allows: a source that is permanently unaskable is still reclaimed, a
      // few passes late. Cheaper than revoking a live grant on a blip.
      $src_db = ($t === FALSE) ? '' : _hosting_migrate_source_probe_unknown($row->source_nid);
      if ($src_db !== '' && preg_match('/^[A-Za-z0-9_]+$/', $src_db) === 1) {
        $key = $t['user'] . '|' . $src_db;
        $tries = isset($shield_tries[$key]) ? (int) $shield_tries[$key] : 0;
        if ($tries < HOSTING_MIGRATE_SOURCE_MAX_RETRIES) {
          $shield[$key] = TRUE;
          $shield_next[$key] = $tries + 1;
          $shielded++;
          drush_log(dt('Migration source sweep: source site @s could not be asked whether it still owns @db, so association @t -> @s is neither proven nor disproven; leaving the grant in place this pass (@n of @m).', array(
            '@t' => $row->target_nid,
            '@s' => $row->source_nid,
            '@db' => $src_db,
            '@n' => $tries + 1,
            '@m' => HOSTING_MIGRATE_SOURCE_MAX_RETRIES,
          )), 'warning');
          continue;
        }
      }
      drush_log(dt('Migration source sweep: association @t -> @s could not be proven on both ends; not treating it as intended.', array(
        '@t' => $row->target_nid,
        '@s' => $row->source_nid,
      )), 'warning');
      continue;
    }
    // key: "<proven grantee user>|<proven source database>"
    $intended[$t['user'] . '|' . $sc['db']] = $row->target_nid;
    $intended_meta[$t['user'] . '|' . $sc['db']] = array('target_nid' => $row->target_nid, 'source_db' => $sc['db'], 'target_user' => $t['user']);
    $proven_users[$t['user']] = TRUE;
    $proven_users[$sc['user']] = TRUE;
    $proven_dbs[$t['db']] = $row->target_nid;
    $proven_dbs[$sc['db']] = $row->source_nid;
  }
  if (!$dry) {
    variable_set('hosting_migrate_source_shielded', $shield_next);
  }

  // This instance's own record of the database names it has minted for its own
  // sites and no longer runs. Built here, ahead of BOTH passes that read it --
  // the pass further down documents where its three sources come from. It reads
  // frontend tables only, so the order is free.
  $former = array();
  $result = db_query("SELECT DISTINCT s.db_name FROM {hosting_site} s INNER JOIN {node} n ON n.nid = s.nid WHERE n.type = 'site' AND n.nid <> :hm AND s.db_name <> '' AND (s.status = :deleted OR s.vid <> n.vid)", array(':hm' => $hostmaster_nid, ':deleted' => HOSTING_SITE_DELETED));
  foreach ($result as $row) {
    if (preg_match('/^[A-Za-z0-9_]+$/', $row->db_name) === 1) {
      $former[$row->db_name] = TRUE;
    }
  }
  foreach (array_keys(variable_get('hosting_migrate_source_former_names', array())) as $name) {
    if (preg_match('/^[A-Za-z0-9_]+$/', $name) === 1) {
      $former[$name] = TRUE;
    }
  }

  // Counted by both passes below: a grant this sweep can see but holds no
  // record that would justify touching.
  $left_to_human = 0;

  // --- what actually exists: enumerate from OUR OWN PROVEN USERS ------------
  //
  // This pass used to enumerate grants ON our databases, taken from
  // hosting_site.db_name. That had two separate failures. It could not see the
  // orphan class this sweep exists for at all -- once a source site is deleted
  // its database is dropped, so no row remains to find, while the grant on that
  // freed name survives and can later be inherited by a site in ANOTHER account
  // that takes the name. And it derived "ours" from a value that goes stale and
  // that a tenant can influence through an imported platform, which pointed the
  // destructive half at databases outside this instance.
  //
  // Enumerating from users we have PROVEN belong to this instance fixes both: a
  // grant is visible whether or not its database still exists, and nothing
  // outside this instance is ever read or touched. It also honours the rule
  // this sweep is built on -- it never enumerates users globally, only its own.
  $revokes = array();
  $unproven = 0;
  foreach ($ours as $db_name_hint => $nid) {
    $identity = _hosting_migrate_source_prove_site($nid);
    if ($identity === FALSE) {
      $unproven++;
      continue;
    }
    $user = $identity['user'];
    $own_db = $identity['db'];
    $proven_users[$user] = TRUE;
    $proven_dbs[$own_db] = $nid;
    // SELECT * and loopback hosts only, exactly like the former-user pass
    // below: a row carrying more than SELECT, or held at a remote host, is not
    // a grant this feature ever issued, so it is not ours to touch.
    $rows = db_query("SELECT * FROM mysql.db WHERE User = :u AND Host IN ('localhost', '127.0.0.1') AND Select_priv = 'Y'", array(':u' => $user));
    foreach ($rows as $g) {
      $g = (array) $g;
      $granted = _hosting_migrate_source_unescape_db($g['Db']);
      if ($granted === $own_db) {
        // Its own database. Always legitimate.
        continue;
      }
      $select_only = TRUE;
      foreach ($g as $column => $value) {
        if (substr($column, -5) === '_priv' && $column !== 'Select_priv' && $value === 'Y') {
          $select_only = FALSE;
          break;
        }
      }
      if (!$select_only || preg_match('/^[A-Za-z0-9_]+$/', $granted) !== 1) {
        // A wildcard, a multi-privilege or otherwise unusual grant. Not a shape
        // this feature issues, so not ours to remove -- say so rather than act.
        drush_log(dt('Migration source sweep: leaving @u alone on @db -- not a grant shape this feature issues.', array('@u' => $user, '@db' => $g['Db'])), 'warning');
        continue;
      }
      if (isset($intended[$user . '|' . $granted]) || isset($shield[$user . '|' . $granted])) {
        continue;
      }
      // Bounded by this instance's own records, the way the pass below is. This
      // one enumerates from a proven user of ours, which establishes the
      // GRANTEE -- it establishes nothing about the database granted, so a
      // read-only grant an operator made by hand, on something this instance
      // never minted for a site, was being taken away silently and never given
      // back. Not a scope test on the database: revoking our user's grant on a
      // name that now belongs elsewhere is the orphan class this sweep exists
      // for. Every one of those still comes back -- a crash-before-record grant
      // sits on a live source database, so does a source that moved to another
      // client, and a dropped source's name survives in the former-names
      // record. Only names this instance has never recorded are report-only.
      if (!isset($proven_dbs[$granted]) && !isset($ours[$granted]) && !isset($former[$granted])) {
        $left_to_human++;
        drush_log(dt('Migration source sweep: @u holds a read grant on @db, which is not a database name this instance has ever recorded for a site of its own; leaving it to a human.', array('@u' => $user, '@db' => $g['Db'])), 'warning');
        continue;
      }
      $revokes[] = array('user' => $user, 'host' => $g['Host'], 'db' => $g['Db'], 'label' => $granted);
    }
  }
  if ($unproven > 0) {
    drush_log(dt('Migration source sweep: @n site(s) could not prove their own database identity and were skipped; their grants were neither examined nor touched.', array('@n' => $unproven)), 'warning');
  }

  // --- the orphan classes no current user holds ----------------------------
  //
  // Two leftovers are invisible to the pass above because their grantee is
  // nobody's CURRENT proven user: the user of a deleted target site, which
  // provision keeps alive precisely because the SELECT grant on the source is
  // still on it, and a user that a restore or deploy has superseded. Both hold
  // a grant on a database that still lives here, so they are found from the
  // other direction: enumerate grants ON this instance's proven databases.
  //
  // The destructive half stays bounded the same way as everything else in this
  // pass -- by proof and by this instance's own records, never by how a grant
  // merely looks. A row is revoked only when ALL of these hold:
  //   - the host is loopback, the only place this feature ever grants;
  //   - the privilege set is exactly SELECT, the only shape it ever issues;
  //   - the grantee is not a proven user of this instance (those grants were
  //     reconciled above) and not the database's own same-named user (the
  //     provision convention -- that user is legitimate even while its site
  //     cannot currently prove itself);
  //   - the grantee is on this instance's OWN record of former database
  //     names. That record has three sources: a deleted site's surviving
  //     {hosting_site} row, a superseded revision (for forks that keep them),
  //     and the names hosting_migrate_source_record_former_name() writes at
  //     the moment a restore or migrate rotates them -- necessary because this
  //     fork updates the site's single revision in place, so the rotated-away
  //     name survives in no other record. The record is what keeps an
  //     operator's monitoring or backup user out of reach: such a user is
  //     never a name this instance minted for a site, so the sweep can report
  //     it but structurally cannot revoke it.
  // Anything that fails only the last test is said out loud and left alone.
  // $former and $left_to_human are built above the pass before this one, which
  // reads them too.
  if (!empty($proven_dbs)) {
    $spellings = array();
    foreach (array_keys($proven_dbs) as $db) {
      foreach (_hosting_migrate_source_db_spellings($db) as $spelling) {
        $spellings[] = $spelling;
      }
    }
    // SELECT * so the privilege shape can be checked against every *_priv
    // column this server version actually has.
    $rows = db_query("SELECT * FROM mysql.db WHERE Db IN (:dbs) AND User <> '' AND Host IN ('localhost', '127.0.0.1') AND Select_priv = 'Y'", array(':dbs' => $spellings));
    foreach ($rows as $g) {
      $g = (array) $g;
      $granted = _hosting_migrate_source_unescape_db($g['Db']);
      $grantee = $g['User'];
      if (isset($proven_users[$grantee])) {
        continue;
      }
      if ($grantee === $granted) {
        continue;
      }
      $select_only = TRUE;
      foreach ($g as $column => $value) {
        if (substr($column, -5) === '_priv' && $column !== 'Select_priv' && $value === 'Y') {
          $select_only = FALSE;
          break;
        }
      }
      if (!$select_only || preg_match('/^[A-Za-z0-9_]+$/', $grantee) !== 1) {
        drush_log(dt('Migration source sweep: leaving @u alone on @db -- not a grant shape this feature issues.', array('@u' => $grantee, '@db' => $g['Db'])), 'warning');
        continue;
      }
      if (!isset($former[$grantee])) {
        $left_to_human++;
        drush_log(dt('Migration source sweep: @u holds a read grant on @db but is not on this instance\'s record of its own former databases; leaving it to a human.', array('@u' => $grantee, '@db' => $g['Db'])), 'warning');
        continue;
      }
      $revokes[] = array('user' => $grantee, 'host' => $g['Host'], 'db' => $g['Db'], 'label' => $granted, 'former' => TRUE);
    }
  }

  // --- guard against acting on corrupt or empty intent --------------------
  $failed = 0;
  $reclaimed = 0;
  $host_skipped = 0;
  if (count($revokes) > $max && !$force) {
    return drush_set_error('MIGRATE_SOURCE_TOO_MANY', dt('Migration source sweep: @n grants look unjustified, which is more than the @max guard allows. Refusing to act -- check the association table before re-running with --force-revokes.', array('@n' => count($revokes), '@max' => $max)));
  }

  foreach ($revokes as $r) {
    if (preg_match('/^[A-Za-z0-9_.:%-]+$/', $r['host']) !== 1) {
      // Tallied: the summary derives "revoked" by subtraction, so an
      // uncounted skip would be reported as revoked.
      $host_skipped++;
      drush_log(dt('Migration source sweep: skipping an unrecognised host for @u.', array('@u' => $r['user'])), 'warning');
      continue;
    }
    if ($dry) {
      if (!empty($r['former'])) {
        drush_log(dt('Migration source sweep [dry-run]: would revoke the stale read grant former site user @g holds on @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
      }
      else {
        drush_log(dt('Migration source sweep [dry-run]: would revoke @g read access to @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
      }
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
      if (!empty($r['former'])) {
        $reclaimed++;
        drush_log(dt('Migration source sweep: revoked the stale read grant former site user @g held on @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
      }
      else {
        drush_log(dt('Migration source sweep: revoked @g read access to @db.', array('@g' => $r['user'] . '@' . $r['host'], '@db' => $r['label'])), 'ok');
      }
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
  //
  // BOUNDED. An association the backend will never satisfy -- a target that has
  // moved off a Drupal 8+ platform, a source that cannot prove ownership -- used
  // to re-queue a full provision-verify on every pass, forever, with nothing
  // recording that it never worked. On an hourly schedule that is a permanent
  // load generator nobody attributes to this feature. After a few consecutive
  // failures the association is left alone and said so once.
  $queued = 0;
  $abandoned = 0;
  $attempts = variable_get('hosting_migrate_source_retries', array());
  $seen = array();
  foreach ($intended as $key => $target_nid) {
    $meta = $intended_meta[$key];
    $found = db_query("SELECT COUNT(*) FROM mysql.db WHERE Db IN (:dbs) AND User = :u AND Select_priv = 'Y'", array(
      ':dbs' => _hosting_migrate_source_db_spellings($meta['source_db']),
      ':u' => $meta['target_user'],
    ))->fetchField();
    if ($found > 0) {
      // Converged: forget any history so a later genuine failure starts fresh.
      unset($attempts[$key]);
      continue;
    }
    $seen[$key] = TRUE;
    $tries = isset($attempts[$key]) ? (int) $attempts[$key] : 0;
    if ($tries >= HOSTING_MIGRATE_SOURCE_MAX_RETRIES) {
      $abandoned++;
      if ($tries == HOSTING_MIGRATE_SOURCE_MAX_RETRIES) {
        drush_log(dt('Migration source sweep: site @nid still has no grant on @db after @n attempts; leaving it to a human rather than re-queueing a verify every pass. Clear or re-set the source to try again.', array(
          '@nid' => $target_nid,
          '@db' => $meta['source_db'],
          '@n' => $tries,
        )), 'warning');
        $attempts[$key] = $tries + 1;
      }
      continue;
    }
    if ($dry) {
      drush_log(dt('Migration source sweep [dry-run]: would queue a verify for site @nid to restore its missing grant.', array('@nid' => $target_nid)), 'ok');
      continue;
    }
    if (hosting_task_outstanding($target_nid, 'delete') || hosting_task_outstanding($target_nid, 'verify')) {
      continue;
    }
    $attempts[$key] = $tries + 1;
    hosting_add_task($target_nid, 'verify');
    $queued++;
    drush_log(dt('Migration source sweep: queued a verify for site @nid to restore its missing grant.', array('@nid' => $target_nid)), 'ok');
  }
  // Drop history for associations that no longer exist at all.
  foreach (array_keys($attempts) as $key) {
    if (!isset($seen[$key])) {
      unset($attempts[$key]);
    }
  }
  if (!$dry) {
    variable_set('hosting_migrate_source_retries', $attempts);
  }

  drush_log(dt('Migration source sweep: @o databases examined, @i associations intended, @c shielded (their source could not be asked), @r revoked (@s from former site users), @f revokes failed, @k skipped on an unrecognised host, @q verifies queued, @a abandoned, @h grants left to a human, @e sites without a recorded database name, @v with an invalid one, @d duplicate names skipped.', array(
    '@o' => count($ours),
    '@i' => count($intended),
    '@c' => $shielded,
    '@r' => $dry ? 0 : (count($revokes) - $failed - $host_skipped),
    '@s' => $reclaimed,
    '@f' => $failed,
    '@k' => $host_skipped,
    '@q' => $queued,
    '@a' => $abandoned,
    '@h' => $left_to_human,
    '@e' => $cand_empty,
    '@v' => $cand_invalid,
    '@d' => $cand_dup,
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

<?php

/**
 * @file
 * Provision/Drush hooks for the provision_tasks_extra module.
 *
 * These are the hooks that will be executed by the drush_invoke function.
 */

// The readers of a tenant's tree and what judges a move between two
// codebases: one file, which the panel loads too. Another copy of it may be
// loaded already while the panel moves to a new tree.
if (!function_exists('_provision_tasks_extra_read_plain')) {
  include_once dirname(__FILE__) . '/../hosting_tasks_extra.upgrade.inc';
}

/**
 * Implementation of hook_drush_command().
 */
function provision_tasks_extra_drush_command() {
  $items['provision-flush_cache'] = array(
    'description' => 'Flushes all caches on a site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-rebuild_registry'] = array(
    'description' => 'Rebuilds the registry on a site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-run_cron'] = array(
    'description' => 'Runs cron on a site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-update'] = array(
    'description' => dt('Runs db updates on a site'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
  );
  $items['provision-site_local_updatedb'] = array(
    'description' => dt('Applies a Drupal 10+ site\'s pending database updates with its site-local Drush (no backup, no maintenance mode); what provision-update and provision-sync run on Drupal 10+'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
  );
  $items['provision-update_translations'] = array(
    'description' => dt('Update translations on a site'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['provision-flush_drush_cache'] = array(
    'description' => 'Flushes the Drush cache on the server',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-updates_report'] = array(
    'description' => dt('Reports the available updates of a site. Read-only: it changes nothing in the site\'s code or content, or in its platform.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-drupal-upgrade'] = array(
    'description' => dt('Moves a Drupal 10+ site onto a newer platform, keeping a copy to go back to: a Clone as the way back, then a Migrate that is undone when the database updates do not finish or a page no longer answers.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'upgrade_target' => 'The context name of the platform to move onto.',
      'upgrade_target_nid' => 'The panel id of that platform.',
      'upgrade_source_nid' => 'The panel id of the platform the site runs on.',
      'upgrade_db_nid' => 'The panel id of the site\'s database server.',
      'upgrade_task_nid' => 'The panel id of this task.',
      'upgrade_copy' => 'The name the way-back copy gets.',
      'upgrade_source_title' => 'The name of the current platform, for the log.',
      'upgrade_target_title' => 'The name of the new platform, for the log.',
      'upgrade_copy_secs' => 'How long the site\'s newest Backup or Clone took, in seconds.',
      'upgrade_ceiling' => 'How long one task may run, in seconds.',
      'upgrade_return' => 'Set when the panel offers Go back.',
      'upgrade_tidy' => 'Set when the run may remove what an earlier run of the site left when it was cut off before its move.',
    ),
  );
  $items['provision-drupal-upgrade-return'] = array(
    'description' => dt('Puts back the copy an Upgrade kept: the upgraded site is renamed aside and switched off, then the copy is switched on and takes the site\'s name. Nothing is deleted.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'return_domain' => 'The name the site is served under.',
      'return_copy' => 'The name of the copy the Upgrade kept.',
      'return_aside' => 'The name the upgraded site is kept under.',
      'return_new' => 'The context name of the platform the upgraded site is on.',
      'return_new_nid' => 'The panel id of that platform.',
      'return_old' => 'The context name of the platform the copy is on.',
      'return_old_nid' => 'The panel id of that platform.',
      'return_new_db_nid' => 'The panel id of the upgraded site\'s database server.',
      'return_old_db_nid' => 'The panel id of the copy\'s database server.',
      'return_task_nid' => 'The panel id of this task.',
      'return_new_title' => 'The name of the upgraded site\'s platform, for the log.',
      'return_old_title' => 'The name of the copy\'s platform, for the log.',
      'return_when' => 'When the copy was made, for the log.',
      'return_aside_on' => 'Set when the panel holds the upgraded site as switched on.',
      'return_copy_on' => 'Set when the panel holds the copy as switched on.',
      'return_copy_was_on' => 'Set when the copy was switched on before Go back began.',
      'return_copy_secs' => 'How long the site\'s newest Backup or Clone took, in seconds.',
      'return_ceiling' => 'How long one task may run, in seconds.',
    ),
  );
  $items['provision-platform-successor'] = array(
    'description' => dt('Builds a Composer platform again at a newer version into a new folder, as the new platform\'s own first Verify, and keeps it only when it is a newer version of the same codebase; otherwise deletes it. A security advisory the build adds is named in its result.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'successor_context' => 'The context name of the new platform.',
      'successor_nid' => 'Its panel id.',
      'successor_path' => 'The folder it is built into.',
      'successor_root' => 'Its Platform Path.',
      'successor_source_path' => 'The build folder of the platform it is built from.',
      'successor_major' => 'The Drupal major the chosen version asks for, 0 when it does not say.',
      'successor_task_nid' => 'The panel id of this task.',
      'successor_title' => 'The name of the new platform, for the log.',
      'source_title' => 'The name of the platform it is built from, for the log.',
    ),
  );
  $items['provision-update_uri'] = array(
    'description' => 'Update a site database to reflect a changed URI. This does NOT rename the whole site, use a provision-migrate task for that.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'arguments' => array(
      'old_uri' => 'Previous name of subdirectory within /sites or a site-alias.',
      'uri' => 'New name of subdirectory within /sites or a site-alias.',
    ),
    'examples' => array(
      'drush provision-update_uri www.example.com dev.example.com'
        => 'Update many references to the www.example.com uri in the site database'
    ),
  );
  $items['provision-features_revert_all'] = array(
    'description' => 'Revert all Features modules overrides on this site.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'features' => 'Optional: List of features to revert.  If left empty, all features will be reverted.',
    ),
  );
  $items['provision-features_update_all'] = array(
    'description' => 'Update all Features modules on this site: write overrides to files.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'features' => 'Optional: List of features to update.  If left empty, all features will be updated.',
      'revert' => 'Optional: Run a features-revert-all --force command after updating.',
    ),
  );
  $items['provision-utf8mb4_convert'] = array(
    'description' => 'Converts a Drupal 7 site database from 3-byte utf8 to utf8mb4',
    // SITE phase (not ROOT): the site credentials live in the site-local
    // drushrc.php, which drush only loads into the 'site' option context
    // at this phase — the alias file carries no db_* options.
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
  );

  return $items;
}

/**
 * Implements the provision-updates_report command.
 *
 * Report only: nothing here changes the site's code or content or its platform,
 * it never locks a platform's local Drush, and a report that cannot be made is a
 * note in the task log, never a failed task.
 */
function drush_provision_tasks_extra_provision_updates_report() {
  drush_errors_on();
  // These come first: none has Drupal 8's core file, so each would inherit the
  // Drupal 6 and 7 path below.
  if ((function_exists('provision_platform_is_grav') && provision_platform_is_grav(d()))
    || (function_exists('provision_platform_is_txp') && provision_platform_is_txp(d()))) {
    drush_log(dt('Available updates ###---### not reported for this CMS.'), 'success');
    return;
  }
  // Drush 8 answers "not supported yet by Backdrop" to pm-updatestatus.
  if (function_exists('provision_platform_is_backdrop') && provision_platform_is_backdrop(d())) {
    drush_log(dt('Available updates ###---### not reported here for Backdrop yet: see Reports, Available updates in the site itself.'), 'success');
    return;
  }
  // By a file, not drush_drupal_major_version(), which loads the platform's
  // autoload.php into this process.
  if (is_file(d()->root . '/core/lib/Drupal.php')) {
    _provision_tasks_extra_updates_report_audit();
  }
  else {
    _provision_tasks_extra_updates_report_ups();
  }
}

/**
 * Drupal 6 and 7: Drush 8's own update backend.
 *
 * It asks the release feed directly, so the update module stays as it is, enabled
 * or not. -v keeps Drush's notes of a feed it read from an expired copy, and it
 * also lists the projects that are up to date, which are counted here, not listed.
 */
function _provision_tasks_extra_updates_report_ups() {
  drush_log(dt('Available updates ###---### drush8 pm-updatestatus ###---### start'), 'notice');
  $scratch = _provision_tasks_extra_scratch();
  if ($scratch === FALSE) {
    drush_log(dt('Available updates ###---### unknown: no private folder for the report.'), 'success');
    return;
  }
  $ran = _provision_tasks_extra_exec('drush8 ' . drush_escapeshellarg(d()->name) . ' ups --update-backend=drush --format=json -v', $scratch, '.ups.json', '.ups.err');
  $out = trim((string) @file_get_contents($scratch . '/.ups.json'));
  $err = (string) @file_get_contents($scratch . '/.ups.err');
  _provision_tasks_extra_scratch_remove($scratch);
  $doc = json_decode($out, TRUE);
  // Drush drops a project whose feed it could not read, clears the error and exits
  // 0, so only its own stderr says so. With -v a read feed always gives a row, core's
  // at the least, so no row is never a site with nothing to report.
  $unread = substr_count($err, 'Failed to get available update data');
  $stale = substr_count($err, 'retrieved from an expired cache');
  if ($ran !== TRUE || !is_array($doc) || !$doc) {
    if ($ran === TRUE && $unread > 0) {
      drush_log(dt('Available updates ###---### unknown: the release feed could not be read.'), 'success');
    }
    else {
      drush_log(dt('Available updates ###---### unknown: drush8 pm-updatestatus gave no report for this site.'), 'success');
    }
    return;
  }
  $rows = array();
  $listed = 0;
  $security = 0;
  $newest = 0;
  $d7 = FALSE;
  foreach ($doc as $project) {
    if (!is_array($project) || !isset($project['name'])) {
      continue;
    }
    $d7 = $d7 || (isset($project['api_version']) && $project['api_version'] === '7.x');
    $name = _provision_tasks_extra_audit_word($project['name']);
    $have = _provision_tasks_extra_audit_word(isset($project['existing_version']) ? $project['existing_version'] : '');
    $want = _provision_tasks_extra_audit_word(isset($project['candidate_version']) ? $project['candidate_version'] : '');
    $why = _provision_tasks_extra_audit_text(isset($project['status_msg']) ? $project['status_msg'] : '');
    $status = isset($project['status']) ? (int) $project['status'] : 0;
    // An extended-support core is newer than anything on drupal.org's feed, and
    // the feed then proposes a downgrade. That is no finding.
    if ($name === 'drupal' && $want !== '-' && version_compare($have, $want, '>')) {
      $rows[] = $name . ' ' . $have . ': newer than the release feed knows, an extended-support core takes its fixes with a BOA release';
      continue;
    }
    // 1 is Drush's own verdict, DRUSH_UPDATESTATUS_NOT_SECURE.
    if ($status === 1 || _provision_tasks_extra_release_insecure($project)) {
      $security++;
      $listed++;
      $rows[] = ($have === $want)
        ? $name . ' ' . $have . ': the release feed knows a security problem here, and has no newer stable release to move to'
        : $name . ' ' . $have . ' to ' . $want . ': SECURITY UPDATE available';
      continue;
    }
    // On its newest release: 5 is up to date, 3 is drupal.org's verdict on every
    // project of an ended major version, with nothing newer to move to.
    if ($have === $want && in_array($status, array(3, 5), TRUE)) {
      $newest++;
      continue;
    }
    $listed++;
    $rows[] = $name . ' ' . $have . ' to ' . $want . ': ' . $why;
  }
  if ($rows) {
    _provision_tasks_extra_updates_report_log($rows);
  }
  if ($unread > 0) {
    drush_log(dt('Available updates ###---### @count more could not be checked: the release feed did not answer.', array('@count' => $unread)), 'notice');
  }
  if ($stale > 0) {
    drush_log(dt('Available updates ###---### @count read from an expired copy of the release feed, which did not answer.', array('@count' => $stale)), 'notice');
  }
  if ($d7) {
    drush_log(dt('Available updates ###---### drupal.org has published no Drupal 7 advisories since 5 January 2025, so no security flag means no published advisory, not a safe release.'), 'notice');
  }
  drush_log(dt('Available updates ###---### @count listed, @security of them security updates, @newest more on their newest release.', array(
    '@count' => $listed,
    '@security' => $security,
    '@newest' => $newest,
  )), 'success');
}

/**
 * Whether the release feed knows a security problem in the installed release.
 *
 * Drush's own walk over the releases runs only for a supported project, and
 * drupal.org marks every project of an ended major version unsupported, so it never
 * says so on Drupal 6 or 7. This is the same walk: the release's own Insecure term,
 * or a newer published release of the same major version typed as a security update.
 * A version the feed does not list has no place in that order, so it gets no verdict:
 * a fork of 7.102 is not older than 7.102's security release.
 */
function _provision_tasks_extra_release_insecure($project) {
  if (empty($project['releases']) || !is_array($project['releases']) || !isset($project['existing_version'])) {
    return FALSE;
  }
  $have = $project['existing_version'];
  $dev = isset($project['install_type']) && $project['install_type'] === 'dev';
  if (!$dev && !isset($project['releases'][$have])) {
    return FALSE;
  }
  if (in_array('Insecure', _provision_tasks_extra_release_types(isset($project['releases'][$have]) ? $project['releases'][$have] : array()), TRUE)) {
    return TRUE;
  }
  foreach ($project['releases'] as $version => $release) {
    if ((string) $version === (string) $have) {
      break;
    }
    if (!is_array($release) || (isset($release['status']) && $release['status'] === 'unpublished')) {
      continue;
    }
    $types = _provision_tasks_extra_release_types($release);
    if (in_array('Insecure', $types, TRUE)) {
      continue;
    }
    if (isset($release['version_major'], $project['existing_major']) && (int) $release['version_major'] > (int) $project['existing_major']) {
      continue;
    }
    // A dev snapshot has no release of its own to stop at: compare the dates, with
    // Drush's 100 seconds of leeway between the .info file and the tarball.
    if ($dev && (empty($project['datestamp']) || (isset($release['date']) && (int) $project['datestamp'] + 100 > (int) $release['date']))) {
      continue;
    }
    if (in_array('Security update', $types, TRUE)) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * The feed's "Release type" terms of one release.
 */
function _provision_tasks_extra_release_types($release) {
  if (is_array($release) && isset($release['terms']['Release type']) && is_array($release['terms']['Release type'])) {
    return $release['terms']['Release type'];
  }
  return array();
}

/**
 * Drupal 8 and newer: composer audit of the platform's lock.
 */
function _provision_tasks_extra_updates_report_audit() {
  drush_log(dt('Available updates ###---### composer audit --locked ###---### start'), 'notice');
  $audit = _provision_tasks_extra_lock_advisories(d()->root);
  if ($audit['lines'] === FALSE) {
    drush_log(dt('Available updates ###---### advisories: unknown (@why)', array('@why' => $audit['why'])), 'success');
    return;
  }
  $lines = $audit['lines'];
  drush_log(dt('Available updates ###---### read from @path: the whole platform, not only what this site installed.', array('@path' => _provision_tasks_extra_no_markup($audit['lock']))), 'notice');
  _provision_tasks_extra_updates_report_log($lines);
  drush_log(dt('Available updates ###---### @first', array('@first' => $lines[0])), 'success');
}

/**
 * The advisory report of one codebase's lock.
 *
 * It works on a disposable copy, never in the platform: Composer runs a
 * composer.json's init and pre-command-run scripts even for audit, which would
 * run tenant code as this user, and a platform's config and repositories can
 * hide advisories. The copy's composer.json declares drupal.org's repository,
 * the only way its advisories reach composer audit.
 *
 * @param string $root
 *   A platform's docroot.
 *
 * An answer is kept for the rest of the process, keyed by the lock it read:
 * an Upgrade that builds its target first audits both codebases in the
 * build's gate and asks again in its own plan.
 *
 * @return array
 *   'lines': the report's lines, FALSE when no audit finished. 'why': the
 *   reason when it did not. 'lock': the path of the lock that was read.
 */
function _provision_tasks_extra_lock_advisories($root) {
  static $answers = array();
  $app = _provision_tasks_extra_app_root($root);
  $lock = ($app === '') ? FALSE : _provision_tasks_extra_read_plain($app . '/composer.lock');
  if ($lock === FALSE) {
    return array('lines' => FALSE, 'why' => 'no composer.lock at the platform root', 'lock' => '');
  }
  // Only what the platform carries is audited.
  $dev = '';
  $installed = _provision_tasks_extra_read_plain($app . '/vendor/composer/installed.json');
  if ($installed !== FALSE) {
    $info = json_decode($installed, TRUE);
    if (is_array($info) && array_key_exists('dev', $info) && $info['dev'] === FALSE) {
      $dev = ' --no-dev';
    }
  }
  $key = $app . "\n" . md5($lock) . $dev;
  if (!isset($answers[$key])) {
    $answers[$key] = _provision_tasks_extra_lock_audit($app, $lock, $dev);
  }
  return $answers[$key];
}

/**
 * Runs the audit of _provision_tasks_extra_lock_advisories() on a copy.
 */
function _provision_tasks_extra_lock_audit($app, $lock, $dev) {
  $scratch = _provision_tasks_extra_scratch();
  $copy = json_encode(array(
    'name' => 'boa/advisory-audit',
    'repositories' => array(array('type' => 'composer', 'url' => 'https://packages.drupal.org/8')),
  ));
  if ($scratch === FALSE
    || @file_put_contents($scratch . '/composer.json', $copy) === FALSE
    || @file_put_contents($scratch . '/composer.lock', $lock) === FALSE) {
    if ($scratch !== FALSE) {
      _provision_tasks_extra_scratch_remove($scratch);
    }
    return array('lines' => FALSE, 'why' => 'cannot write the audit copy', 'lock' => $app . '/composer.lock');
  }
  // A home of its own, so no global Composer config (ignore lists,
  // ignore-unreachable) reaches the audit. The exit code is not the verdict:
  // composer audit exits non-zero for findings and abandoned packages alike.
  putenv('COMPOSER_HOME=' . $scratch . '/.home');
  putenv('COMPOSER_CACHE_DIR=' . $scratch . '/.cache');
  putenv('COMPOSER_NO_INTERACTION=1');
  $ran = _provision_tasks_extra_exec('composer audit --locked --format=json --no-plugins --no-scripts' . $dev, $scratch, '.audit.json', '.audit.err');
  putenv('COMPOSER_HOME');
  putenv('COMPOSER_CACHE_DIR');
  putenv('COMPOSER_NO_INTERACTION');
  $audit = (string) @file_get_contents($scratch . '/.audit.json');
  $err = (string) @file_get_contents($scratch . '/.audit.err');
  _provision_tasks_extra_scratch_remove($scratch);
  $lines = ($ran === NULL) ? FALSE : _provision_tasks_extra_audit_lines($audit, $lock, $dev !== '');
  if ($lines === FALSE) {
    $why = ($ran === NULL) ? 'it ran out of time' : _provision_tasks_extra_audit_why($err);
    return array('lines' => FALSE, 'why' => 'composer audit did not finish: ' . $why, 'lock' => $app . '/composer.lock');
  }
  $lines[] = ($dev !== '') ? '# require-dev: not installed in the platform, not audited' : '# require-dev: audited (installed, or no installed.json to tell)';
  return array('lines' => $lines, 'why' => '', 'lock' => $app . '/composer.lock');
}

/**
 * Logs the report's lines as one entry, which the panel folds behind (Expand).
 */
function _provision_tasks_extra_updates_report_log($lines) {
  drush_log(_provision_tasks_extra_no_markup(implode("\n", $lines)), 'notice');
}

/**
 * Text from the tenant's tree, without markup.
 *
 * The panel's log filter keeps links, so what a lock or a path says must not reach
 * whoever reads the log as a working link.
 */
function _provision_tasks_extra_no_markup($text) {
  return str_replace(array('<', '>'), '', $text);
}

/**
 * A private folder for one report, or FALSE.
 *
 * Not drush_tempdir(): its folder takes the task's group-writable umask, and a
 * tenant in that group could plant links under the names the report writes. The
 * report removes it itself, because Drush's own cleanup opens a folder to 0777 first.
 */
function _provision_tasks_extra_scratch() {
  $dir = rtrim(drush_find_tmp(), '/') . '/updates_report_' . str_replace('.', '', uniqid('', TRUE));
  if (!@mkdir($dir, 0700)) {
    return FALSE;
  }
  return $dir;
}

/**
 * Removes a folder _provision_tasks_extra_scratch() made, following no link.
 */
function _provision_tasks_extra_scratch_remove($dir) {
  $items = @scandir($dir);
  if (is_array($items)) {
    foreach ($items as $item) {
      if ($item === '.' || $item === '..') {
        continue;
      }
      $path = $dir . '/' . $item;
      if (is_dir($path) && !is_link($path)) {
        _provision_tasks_extra_scratch_remove($path);
      }
      else {
        @unlink($path);
      }
    }
  }
  @rmdir($dir);
}

/**
 * Runs a command through /bin/sh in $dir, its output in two files there.
 *
 * Not drush_shell_exec(), which appends 2>&1 and lands Drush's and Composer's
 * messages in the JSON each leg reads. The leading word stays first so that websh
 * dispatches it as usual. The instance has one task runner, so a feed that hangs
 * must not hold it. TRUE on exit 0, FALSE on any other exit, NULL out of time.
 */
function _provision_tasks_extra_exec($command, $dir, $out, $err, $limit = 900) {
  $pipes = array();
  $process = @proc_open($command, array(
    0 => array('file', '/dev/null', 'r'),
    1 => array('file', $dir . '/' . $out, 'w'),
    2 => array('file', $dir . '/' . $err, 'w'),
  ), $pipes, $dir);
  if (!is_resource($process)) {
    return FALSE;
  }
  $deadline = time() + $limit;
  while (TRUE) {
    $state = proc_get_status($process);
    if (!$state['running']) {
      proc_close($process);
      return (int) $state['exitcode'] === 0;
    }
    if (time() >= $deadline) {
      proc_terminate($process, 15);
      sleep(1);
      proc_terminate($process, 9);
      proc_close($process);
      return NULL;
    }
    usleep(100000);
  }
}

/**
 * Composer's reason for an unfinished audit, one printable line.
 *
 * Composer prints its error under an "In <File>.php line <N>:" header as a block
 * cut at a fixed width, each chunk framed by two spaces a side.
 */
function _provision_tasks_extra_audit_why($err) {
  $why = '';
  $first = '';
  $in = FALSE;
  foreach (explode("\n", $err) as $line) {
    if ($first === '' && trim($line) !== '') {
      $first = trim($line);
    }
    if (preg_match('/^In .* line [0-9]+:/', $line)) {
      $in = TRUE;
      continue;
    }
    if ($in) {
      if (trim($line) !== '') {
        $why .= substr($line, 2, max(0, strlen($line) - 4));
      }
      elseif ($why !== '') {
        break;
      }
    }
  }
  if ($why === '') {
    $why = ($first !== '') ? $first : 'an advisory source was unreachable';
  }
  return _provision_tasks_extra_no_markup(substr(trim(preg_replace('/[^\x20-\x7e]+/', ' ', $why)), 0, 200));
}

/**
 * Strips a field to printable non-space ASCII: it comes from a remote feed.
 */
function _provision_tasks_extra_audit_word($s) {
  $s = preg_replace('/[^\x21-\x7e]/', '', is_scalar($s) ? (string) $s : '');
  return $s === '' ? '-' : $s;
}

/**
 * Strips a field to one line of printable ASCII: it comes from a remote feed.
 */
function _provision_tasks_extra_audit_text($s) {
  $s = trim(preg_replace('/[^\x20-\x7e]+/', ' ', is_scalar($s) ? (string) $s : ''));
  return $s === '' ? '-' : substr($s, 0, 160);
}

/**
 * Why no drupal.org advisory reaches composer audit for a locked package.
 *
 * Its lock entry carries drupal.org's per-release coverage with any status but
 * "covered" (alpha, beta, RC, dev, not opted in), or it is a Drupal module, theme
 * or profile whose entry has no such record. '' when it is covered.
 */
function _provision_tasks_extra_audit_uncovered($p) {
  $d = isset($p['extra']['drupal']) && is_array($p['extra']['drupal']) ? $p['extra']['drupal'] : array();
  if (isset($d['security-coverage']) && is_array($d['security-coverage'])) {
    $c = $d['security-coverage'];
    $st = isset($c['status']) && is_scalar($c['status']) ? (string) $c['status'] : '';
    if ($st === 'covered') {
      return '';
    }
    return _provision_tasks_extra_audit_text(isset($c['message']) && is_string($c['message']) && $c['message'] !== '' ? $c['message'] : 'coverage status: ' . $st);
  }
  $t = isset($p['type']) && is_scalar($p['type']) ? (string) $p['type'] : '';
  if (in_array($t, array('drupal-module', 'drupal-theme', 'drupal-profile'), TRUE)) {
    return 'no drupal.org coverage record in the lock: ' . $t;
  }
  return '';
}

/**
 * Adds one tab-separated line per finding: id, package, version, CVE, link.
 */
function _provision_tasks_extra_audit_rows($list, $ver, $prefix, &$rows) {
  if (!is_array($list)) {
    return;
  }
  foreach ($list as $pkg => $items) {
    if (!is_array($items)) {
      continue;
    }
    foreach ($items as $x) {
      if (is_array($x)) {
        $name = isset($x['packageName']) && is_string($x['packageName']) ? $x['packageName'] : $pkg;
        $id = isset($x['advisoryId']) ? $x['advisoryId'] : (isset($x['id']) ? $x['id'] : '');
        $link = isset($x['link']) ? $x['link'] : (isset($x['url']) ? $x['url'] : '');
        $cve = isset($x['cve']) ? $x['cve'] : '';
        $tag = $prefix;
        if ($tag === '' && isset($x['listName'])) {
          $tag = $x['listName'] . ':';
        }
      }
      else {
        $name = $pkg;
        $id = $x;
        $link = '';
        $cve = '';
        $tag = $prefix;
      }
      $rows[] = implode("\t", array(
        _provision_tasks_extra_audit_word($tag . $id),
        _provision_tasks_extra_audit_word($name),
        _provision_tasks_extra_audit_word(isset($ver[$name]) ? $ver[$name] : ''),
        _provision_tasks_extra_audit_word($cve),
        _provision_tasks_extra_audit_word($link),
      ));
    }
  }
}

/**
 * Reads a composer audit --format=json document and the lock it audited.
 *
 * The same reading as staticbuild's advisory report: "advisories: N", one line
 * per finding, then "# not covered: N" and one line per locked package no
 * drupal.org advisory reaches composer audit for, because a count of 0 says
 * nothing about those. Composer's filter-list hits and anything it reports as
 * ignored count as findings. FALSE unless the document is a finished audit that
 * reached every advisory source.
 */
function _provision_tasks_extra_audit_lines($audit, $lockfile, $nodev) {
  $a = json_decode($audit, TRUE);
  if (!is_array($a) || !array_key_exists('advisories', $a) || !is_array($a['advisories'])) {
    return FALSE;
  }
  if (!empty($a['unreachable-repositories'])) {
    return FALSE;
  }
  $ver = array();
  $nc = array();
  $lock = json_decode($lockfile, TRUE);
  foreach (array('packages', 'packages-dev') as $s) {
    if (is_array($lock) && isset($lock[$s]) && is_array($lock[$s])) {
      foreach ($lock[$s] as $p) {
        if (isset($p['name'], $p['version']) && is_string($p['name']) && is_string($p['version'])) {
          $ver[$p['name']] = $p['version'];
          if ($s === 'packages' || !$nodev) {
            $why = _provision_tasks_extra_audit_uncovered($p);
            if ($why !== '') {
              $nc[] = '# not covered: ' . _provision_tasks_extra_audit_word($p['name']) . ' ' . _provision_tasks_extra_audit_word($p['version']) . ' (' . $why . ')';
            }
          }
        }
      }
    }
  }
  $rows = array();
  _provision_tasks_extra_audit_rows($a['advisories'], $ver, '', $rows);
  _provision_tasks_extra_audit_rows(isset($a['filter']) ? $a['filter'] : array(), $ver, '', $rows);
  _provision_tasks_extra_audit_rows(isset($a['ignored-advisories']) ? $a['ignored-advisories'] : array(), $ver, 'ignored:', $rows);
  sort($rows);
  $out = array_merge(array('advisories: ' . count($rows)), $rows);
  if (!is_array($lock)) {
    $out[] = '# not covered: unknown (the lock does not parse)';
  }
  else {
    sort($nc);
    $out[] = '# not covered: ' . count($nc);
    $out = array_merge($out, $nc);
  }
  return $out;
}

/**
 * Implements the provision-flush_cache command.
 */
function drush_provision_tasks_extra_provision_flush_cache() {
  drush_errors_on();
  // Grav site (DB-less capsule): flush through Grav's own CLI. Checked
  // BEFORE the Drupal version probe, which reads a Grav root as "0" and
  // would misroute into a Drupal cache-clear that cannot bootstrap.
  // Guarded like _provision_drupal_rebuild_d8plus below: the helpers ship
  // with the provision grav layer and this file must work without them.
  if (function_exists('provision_platform_is_grav')
    && function_exists('_provision_grav_cli')
    && provision_platform_is_grav(d())) {
    if (_provision_grav_cli('grav', 'clearcache -n')) {
      drush_log(dt('All caches cleared (grav clearcache)'));
    }
    else {
      drush_set_error('PROVISION_GRAV_FLUSH_FAILED', dt('grav clearcache failed; see the task log.'));
    }
    return;
  }
  // Textpattern keeps no compiled cache on disk and ships no cache tooling;
  // the frontend hides the task, and this keeps a direct CLI invoke or a
  // queued action honest instead of misrouting into a Drupal cache-clear
  // that clears only Drush's own cache and reports success.
  if (function_exists('provision_platform_is_txp')
    && provision_platform_is_txp(d())) {
    drush_log(dt('Textpattern keeps no server-side cache; nothing to clear.'), 'ok');
    return;
  }
  if (drush_drupal_major_version(d()->root) <= 7) {
    provision_backend_invoke(d()->name, 'cache-clear all');
    provision_backend_invoke(d()->name, 'cache-clear all');
  }
  elseif (drush_drupal_major_version(d()->root) >= 10 && function_exists('_provision_drupal_rebuild_d8plus')) {
    // D10/D11 must not be rebuilt by Aegir's own Drush 8, and on a locked
    // platform no site-local Drush loads under PHP 8 either. The shared
    // provision helper rebuilds the container and flushes every cache by core
    // itself, Drush-free, whatever the lock state. Non-fatal, never silent.
    // A rebuild the helper reports as not done (FALSE) already carries its
    // REBUILD/CORE warning, which the success line below would contradict;
    // an older provision returns nothing and keeps the old reporting.
    if (_provision_drupal_rebuild_d8plus() === FALSE) {
      return;
    }
  }
  else {
    // D8/D9 (and any environment without the helper): Aegir Drush 8 still
    // bootstraps these, so the double cache-rebuild works as before.
    provision_backend_invoke(d()->name, 'cache-rebuild');
    provision_backend_invoke(d()->name, 'cache-rebuild');
  }
  drush_log(dt('All caches cleared'));
}

/**
 * Implements the provision-rebuild_registry command.
 */
function drush_provision_tasks_extra_provision_rebuild_registry() {
  drush_errors_on();
  // No registry and no service container on a foreign-CMS site; the frontend
  // hides the task there, and this guard keeps a directly-invoked run honest
  // instead of misrouting through the Drupal version probe.
  if (function_exists('provision_platform_is_foreign_cms')
    && provision_platform_is_foreign_cms(d())) {
    drush_log(dt('Registry rebuild does not apply to this CMS; nothing to do.'), 'warning');
    return;
  }
  if (d()->type === 'site') {
    if (drush_drupal_major_version(d()->root) == 7) {
      provision_backend_invoke(d()->name, 'registry-rebuild --fire-bazooka');
      drush_log(dt('Registry rebuilt with --fire-bazooka plus all caches cleared'));
    }
    elseif (drush_drupal_major_version(d()->root) == 6) {
      provision_backend_invoke(d()->name, 'registry-rebuild');
      drush_log(dt('Registry rebuilt w/o --fire-bazooka plus all caches cleared'));
    }
    elseif (function_exists('_provision_drupal_rebuild_d8plus')) {
      // D8+ has no registry; the equivalent is rebuilding the service container
      // and flushing all caches (what drush cr does). The shared provision
      // helper does it by core itself, Drush-free, on D10+ (no site-local Drush
      // loads on a locked platform under PHP 8, and Aegir Drush 8 must not be
      // the compiler) and via Aegir Drush 8 'cache-rebuild' on D8/D9.
      // Non-fatal, never silent.
      if (_provision_drupal_rebuild_d8plus() !== FALSE) {
        drush_log(dt('D8+ service container and all caches rebuilt'));
      }
    }
    else {
      // Older provision without the helper: nothing safe to do on D8+.
      drush_log(dt('INFO ###---### Registry Rebuild is not supported ###---### INFO'), 'warning');
    }
  }
}

/**
 * Implements the provision-run_cron command.
 */
function drush_provision_tasks_extra_provision_run_cron() {
  drush_errors_on();
  provision_backend_invoke(d()->name, 'core-cron');
  drush_log(dt('Drush core-cron task completed'));
}

/**
 * Implements the provision-update command.
 */
function drush_provision_tasks_extra_provision_update() {
  drush_errors_on();
  $options = d()->options;
  if (drush_drupal_major_version(d()->root) >= 10) {
    // Drupal 10+: Aegir's own Drush 8 must never run updatedb here -- it is
    // version-mismatched for D10+, its FULL bootstrap is read-only
    // introspection on a locked platform, and the container it would compile
    // carries Drush 8 service definitions (the same rule the deploy follows,
    // deploy.provision.inc DEPLOY4b). The shared provision helper takes the
    // deploy's own window instead: a real unlock when the platform is locked,
    // the site-local Drush updatedb by --root/--uri, a real re-lock, then
    // core's own Drush-free container rebuild. It logs UPDATE2 .. UPDATE10
    // and a WARNING when the updates did not run or did not complete, which
    // the success line below must not contradict. An older provision without
    // the helper keeps the updates for the operator rather than running the
    // wrong Drush.
    if (!function_exists('_provision_drupal_site_local_updatedb')) {
      drush_log(dt('Drush updatedb skipped: Aegir Drush 8 must not run updatedb on Drupal 10+, and this provision carries no site-local updatedb helper; apply the pending database updates by hand'), 'warning');
      return;
    }
    if (isset($options['entity_updates']) && $options['entity_updates']) {
      // Automatic entity schema updates left core in Drupal 8.7 and the option
      // with them; the site-local Drush would refuse it.
      drush_log(dt('--entity-updates does not exist on Drupal 10+; running plain updatedb'), 'notice');
    }
    $updated = _provision_drupal_site_local_updatedb('UPDATE');
    if ($updated === TRUE) {
      drush_log(dt('Site-local Drush updatedb task completed'));
      return;
    }
    // The task's own net -- the pre-update backup taken in pre_validate --
    // is restored by the rollback hook, and Drush runs that hook only on an
    // error. Updates that did not run (no executable site-local Drush) or
    // did not complete must fail the task, as they did when Aegir Drush 8
    // ran them here; the helper logged the UPDATE4b/UPDATE9 warning above.
    return drush_set_error('PROVISION_UPDATEDB_FAILED', dt('The site-local Drush updatedb did not run or did not complete (see the UPDATE4b/UPDATE9 warning above); the pre-update backup is restored'));
  }
  if (isset($options['entity_updates']) && $options['entity_updates'] && drush_drupal_major_version(d()->root) >= 8) {
    provision_backend_invoke(d()->name, 'updatedb', array('--entity-updates'));
    drush_log(dt('Drush updatedb task completed with --entity-updates'));
  }
  else {
    provision_backend_invoke(d()->name, 'updatedb');
    drush_log(dt('Drush updatedb task completed w/o --entity-updates'));
  }
}

/**
 * Implements the provision-site_local_updatedb command.
 *
 * The deploy's Drupal 10+ updatedb window (unlock if locked, site-local
 * updatedb, re-lock, core rebuild) as a command of its own, so a task that
 * updates a site it has just written (provision-sync's destination) can run it
 * in that site's context without provision-update's backup and maintenance
 * mode. Aegir's Drush 8 never runs updatedb on Drupal 10+.
 */
function drush_provision_tasks_extra_provision_site_local_updatedb() {
  drush_errors_on();
  if (drush_drupal_major_version(d()->root) < 10) {
    drush_log(dt('Not a Drupal 10+ site; Aegir Drush 8 updatedb applies (provision-update)'), 'warning');
    return;
  }
  if (!function_exists('_provision_drupal_site_local_updatedb')) {
    drush_log(dt('Drush updatedb skipped: Aegir Drush 8 must not run updatedb on Drupal 10+, and this provision carries no site-local updatedb helper; apply the pending database updates by hand'), 'warning');
    return;
  }
  $updated = _provision_drupal_site_local_updatedb('UPDATE');
  if ($updated === TRUE) {
    drush_log(dt('Site-local Drush updatedb task completed'));
    return;
  }
  // provision-sync's own rollback (the destination's pre-sync backup) fires
  // only on an error from the commands it invokes.
  return drush_set_error('PROVISION_UPDATEDB_FAILED', dt('The site-local Drush updatedb did not run or did not complete (see the UPDATE4b/UPDATE9 warning above)'));
}

/**
 * Implements the provision-update_translations command.
 */
function drush_provision_tasks_extra_provision_update_translations() {
  drush_errors_on();
  if (drush_drupal_major_version(d()->root) >= 8) {
    provision_backend_invoke(d()->name, 'locale-check');
    drush_log(dt('Drush locale-check task completed'));
    provision_backend_invoke(d()->name, 'locale-update');
    drush_log(dt('Drush locale-update task completed'));
  }
  else {
    provision_backend_invoke(d()->name, 'l10n-update-refresh');
    drush_log(dt('Drush l10n-update-refresh task completed'));
    provision_backend_invoke(d()->name, 'l10n-update');
    drush_log(dt('Drush l10n-update task completed'));
  }
}

/**
 * Implements drush_hook_COMMAND_pre_validate().
 *
 * Make a backup of the site before we mess with it.
 */
function drush_provision_tasks_extra_provision_update_pre_validate() {
  // Drupal's database updates and Drupal's settings writer: a Backdrop, Grav
  // or Textpattern site would get a Drupal settings.php in place of its own,
  // so it is refused before anything is changed, as the utf8mb4 conversion's
  // validate below refuses it.
  if (function_exists('provision_platform_is_backdrop') && provision_platform_is_backdrop(d())) {
    return drush_set_error('PROVISION_UPDATE_NOT_DRUPAL', dt('This is a Backdrop site: provision-update runs Drupal database updates and writes a Drupal settings.php. Nothing was changed.'));
  }
  if (function_exists('provision_platform_is_foreign_cms') && provision_platform_is_foreign_cms(d())) {
    return drush_set_error('PROVISION_UPDATE_NOT_DRUPAL', dt('This site does not run Drupal: provision-update runs Drupal database updates and writes a Drupal settings.php. Nothing was changed.'));
  }

  /* Set offline mode to true and re-generate the settings.php. This will write a
   * $conf['site_offline'] = 1; to the settings.php
   */

  $list = drush_commandfile_list();
  $provision_dir = dirname($list['provision']);
  include_once($provision_dir . '/platform/backup.provision.inc');

  // What the rollback of this hook puts back, set first: a write that fails
  // half-way still leaves a site to bring back.
  drush_set_context('PROVISION_TASKS_EXTRA_UPDATE_OFFLINE', TRUE);
  drush_log(dt("Putting the site under maintenance"));
  d()->site_enabled = FALSE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();

  $backup_file = drush_provision_drupal_provision_backup_suggest_filename();
  drush_set_option('pre_update_backup_file', $backup_file);

  drush_set_option('force', TRUE);
  // A safety copy the rollback restores must be self-contained, with the
  // database inside the archive (the mode the restore pre-hook forces for its
  // own copy): the modeless fast path dumps only to the shared, rotating
  // tmp_expim store and packs a files-only archive, which the restore then
  // reports as carrying no database dump. Dispatched by alias name, so the
  // child has a real context (see the rollback below). That context comes
  // from the alias, where the site is enabled: provision_backup_keep_offline
  // keeps the child's settings.php writes in maintenance, or the update
  // would run with the site online.
  $success = provision_backend_invoke(d()->name, 'provision-backup', array($backup_file), array('selected_backup_mode' => 'site_files_with_mysqldump', 'provision_backup_keep_offline' => TRUE));
  // The invoke's return shape is the signal, as for the utf8mb4 conversion's
  // copy below: a backup that failed comes back with a non-zero error_status,
  // and one that died with no output comes back FALSE, with no error set here.
  // A simulated run (--simulate) gets FALSE too, with no backup run at all.
  if ((!$success && !drush_get_context('DRUSH_SIMULATE')) || (is_array($success) && !empty($success['error_status']))) {
    // If the backup failed, we don't want to try restoring it in the rollback.
    drush_set_option('pre_update_backup_file', FALSE);
    // No update runs without its safety copy: the task stops here, and the
    // rollback of this hook takes the site out of maintenance.
    return drush_set_error('PROVISION_UPDATE_BACKUP_FAILED', dt('The safety backup before the update failed: no update was run, and the site is taken out of maintenance.'));
  }
}

/**
 * Implements drush_hook_COMMAND_pre_validate_rollback().
 *
 * The task stops in pre_validate when the safety backup fails, after the site
 * was put under maintenance, and Drush rolls back only the hooks that ran: the
 * command's rollback below never runs then. Bring the site back as it does,
 * without a restore, as no update ran. A task that stopped later was brought
 * back by the command's rollback already, and a site refused before it was
 * taken offline is left as it was.
 */
function drush_provision_tasks_extra_provision_update_pre_validate_rollback() {
  if (!drush_get_context('PROVISION_TASKS_EXTRA_UPDATE_OFFLINE', FALSE)
    || drush_get_context('PROVISION_TASKS_EXTRA_UPDATE_SITE_BACK', FALSE)) {
    return;
  }
  drush_log(dt("Bringing the site out of maintenance"));
  d()->site_enabled = TRUE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();
}

/**
 * Implements drush_hook_COMMAND_rollback().
 */
function drush_provision_tasks_extra_provision_update_rollback() {
  $restore_file = drush_get_option('pre_update_backup_file', FALSE);

  if ($restore_file) {
    drush_set_option('force', TRUE);
    // provision_backend_invoke(), which dispatches USING the alias name, not
    // drush_invoke_process('@self'): a child dispatched by root/uri alone has
    // no alias, provision's context in it defaults to '@self', and the
    // restore's own nested deploy then dispatches to an alias nothing can
    // find -- so this restore always failed (safely, the site kept).
    provision_backend_invoke(d()->name, 'provision-restore', array($restore_file), array('force' => TRUE));
    drush_unset_option('force');
  }

  // Set site_offline to false and regenerate the config
  drush_log(dt("Bringing the site out of maintenance"));
  d()->site_enabled = TRUE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();
  // The pre_validate rollback runs after this one: nothing left for it.
  drush_set_context('PROVISION_TASKS_EXTRA_UPDATE_SITE_BACK', TRUE);
}

/**
 * Implements drush_hook_post_COMMAND().
 */
function drush_provision_tasks_extra_post_provision_update() {
  // Set site_offline to false and regenerate the config
  drush_log(dt("Bringing the site out of maintenance"));
  d()->site_enabled = TRUE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();
}

/**
 * Implements the provision-flush_drush_cache command.
 */
function drush_provision_tasks_extra_provision_flush_drush_cache() {
  drush_errors_on();
  provision_backend_invoke(d()->name, 'cache-clear drush');
  drush_log(dt('Drush cache on the server cleared'));
}

/**
 * Implements the provision-update_uri command.
 *
 * Expects site uri's as the arguments.
 */
function drush_provision_tasks_extra_provision_update_uri($old_uri, $new_uri) {
  // Delegate to provision's deploy engine, found in provision/platform/drupal/deploy*.
  // This handles path and uri changes
  drush_set_option('uri', $new_uri);
  drush_set_option('old_uri', $old_uri);

  drush_include_engine('drupal', 'deploy', drush_drupal_major_version());
}

/**
 * Implements the provision-features_revert_all command.
 */
function drush_provision_tasks_extra_provision_features_revert_all() {
  $drush_alias = d()->name;
  $features = trim(drush_get_option('features', ''));
  $options = "-y";

  if (drush_get_option('force_revert')) {
    $options .= " --force";
  }

  // Clear drush cache, just in case it is needed.
  provision_process("drush {$drush_alias} cc drush", NULL, dt('Clearing drush cache...'));

  // If features were specified...
  if (!empty($features)) {
    provision_process("drush {$drush_alias} features-revert {$features} {$options}", NULL, dt('Revert Features: !feature', [
      '!feature' => $features,
    ]));
  }
  else {
    provision_process("drush {$drush_alias} features-revert-all {$options}", NULL, dt('Revert all Features'));
  }
}

/**
 * Implements the provision-features_update_all command.
 */
function drush_provision_tasks_extra_provision_features_update_all() {
  if (d()->type == 'site') {
    $drush_alias = d()->name;
    $features = trim(drush_get_option('features', ''));
    $revert = drush_get_option('revert', FALSE);

    // Clear drush cache, just in case it is needed.
    provision_process("drush {$drush_alias} cc drush", NULL, dt('Clearing drush cache...'));

    // If features were specified...
    if (!empty($features)) {
      provision_process("drush {$drush_alias} features-update {$features} -y", NULL, dt('Update Features: !feature', [
        '!feature' => $features,
      ]));
    }
    else {
      provision_process("drush {$drush_alias} features-update-all -y", NULL, dt('Update All Features'));
    }

    // Revert force revert the features if the user asked.
    if ($revert) {
      if (!empty($features)) {
        provision_process("drush {$drush_alias} features-revert {$features} --force -y", NULL, dt('Revert Features'));
      }
      else {
        provision_process("drush {$drush_alias} features-revert-all --force -y", NULL, dt('Revert Features'));
      }
    }
  }
}

/**
 * Implements drush_hook_COMMAND_validate().
 *
 * Fail-closed gates only. The work/no-op decision is probe-driven in the
 * command body, so a database that is already fully utf8mb4 completes
 * green as an honest no-op instead of failing here.
 */
function drush_provision_tasks_extra_provision_utf8mb4_convert_validate() {
  if (d()->type !== 'site') {
    return drush_set_error('PROVISION_UTF8MB4_NOT_SITE', dt('The utf8mb4 conversion task can only run on a site context.'));
  }
  if (function_exists('provision_is_hostmaster_site') && provision_is_hostmaster_site()) {
    return drush_set_error('PROVISION_UTF8MB4_HOSTMASTER', dt('The hosting front-end database is converted by the nightly maintenance already; this task is for hosted sites only.'));
  }
  // This guard must run before any major-version check, because
  // drush_drupal_major_version() reports Backdrop core as 1.
  if (function_exists('provision_platform_is_backdrop') && provision_platform_is_backdrop(d())) {
    return drush_set_error('PROVISION_UTF8MB4_BACKDROP', dt('This is a Backdrop site. The utf8mb4 converter only exists for Drupal 7; a Backdrop site inherits converted tables when it is upgraded from a converted Drupal 7 source.'));
  }
  // Same reason as the Backdrop gate: on a foreign-CMS root
  // drush_drupal_major_version() reports the CMS's own major, so the
  // Drupal 7 test below would refuse with a Drupal-flavoured message.
  if (function_exists('provision_platform_is_foreign_cms') && provision_platform_is_foreign_cms(d())) {
    return drush_set_error('PROVISION_UTF8MB4_FOREIGN_CMS', dt('This site does not run Drupal; the utf8mb4 converter only exists for Drupal 7.'));
  }
  if (drush_drupal_major_version(d()->root) != 7) {
    return drush_set_error('PROVISION_UTF8MB4_NOT_D7', dt('The utf8mb4 conversion task requires Drupal 7 (core 7.50 or newer).'));
  }
  // Enforce the extension's own floor here: on older cores it prints a
  // notice and returns without an error code, which would waste a
  // maintenance window and a backup before the verify gate catches it.
  $core = drush_drupal_version(d()->root);
  if (is_string($core) && $core !== '' && version_compare($core, '7.50', '<')) {
    return drush_set_error('PROVISION_UTF8MB4_CORE_TOO_OLD', dt('The utf8mb4 converter requires Drupal core 7.50 or newer; this site runs !v. Upgrade core first.', array('!v' => $core)));
  }
  return TRUE;
}

/**
 * Counts 3-byte utf8 leftovers in a site database via information_schema.
 *
 * Reads through the provision db service (server master credentials), so
 * per-site grants cannot skew the result. Only the utf8 family matters for
 * the illegal-mix-of-collations problem this task exists for; content in
 * other charsets (e.g. latin1) is counted separately because the converter
 * leaves such columns alone by design.
 *
 * @return array|FALSE
 *   array('tables' => int, 'columns' => int, 'other' => int), or FALSE
 *   when information_schema could not be read.
 */
function _provision_tasks_extra_utf8mb4_probe($db_name) {
  // BASE TABLE scoping matters: SQL views mirror their underlying tables'
  // charsets in information_schema but cannot be ALTERed by the converter,
  // so counting them would leave the verify gate permanently red.
  $queries = array(
    'tables' => "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '%s' AND table_type = 'BASE TABLE' AND table_collation LIKE 'utf8%%' AND table_collation NOT LIKE 'utf8mb4%%'",
    'columns' => "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = '%s' AND character_set_name IN ('utf8', 'utf8mb3') AND table_name IN (SELECT table_name FROM information_schema.tables WHERE table_schema = '%s' AND table_type = 'BASE TABLE')",
    'other' => "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = '%s' AND table_type = 'BASE TABLE' AND table_collation IS NOT NULL AND table_collation NOT LIKE 'utf8%%'",
  );
  $counts = array();
  foreach ($queries as $key => $sql) {
    // db_name passed once per %s; the single-placeholder queries simply
    // leave the second argument unused.
    $result = d()->service('db')->query($sql, $db_name, $db_name);
    if (!is_object($result)) {
      return FALSE;
    }
    $counts[$key] = (int) $result->fetchColumn();
  }
  return $counts;
}

/**
 * Relays an un-integrated backend invoke's child log into the task log.
 *
 * With $demote, error/warning entries land as 'info': used for conversion
 * attempts that were (or can still be) recovered by a retry, because any
 * 'error' entry in the task log pins the whole task badge red even when a
 * later attempt succeeded and the result verified clean.
 */
function _provision_tasks_extra_utf8mb4_relay_log($values, $demote) {
  if (!is_array($values)) {
    return;
  }
  if (!empty($values['log']) && is_array($values['log'])) {
    foreach ($values['log'] as $entry) {
      if (!is_array($entry) || !isset($entry['message'])) {
        continue;
      }
      $type = isset($entry['type']) ? $entry['type'] : 'notice';
      if ($demote && ($type == 'error' || $type == 'failed' || $type == 'warning')) {
        $type = 'info';
      }
      drush_log($entry['message'], $type);
    }
  }
  if (!empty($values['output']) && is_string($values['output'])) {
    drush_log($values['output'], 'info');
  }
}

/**
 * Implements the provision-utf8mb4_convert command.
 *
 * Wraps the utf8mb4_convert Drush extension (deployed fleet-wide by BOA)
 * for a single Drupal 7 site: probe, then backup + convert + verify. The
 * post-conversion probe is the completion gate on purpose — the extension
 * returns without an error code on its own refusal paths (core older than
 * 7.50, no server-side utf8mb4 support), and only on-disk evidence proves
 * the conversion actually happened.
 */
function drush_provision_tasks_extra_provision_utf8mb4_convert() {
  drush_errors_on();

  $db_name = drush_get_option('db_name', '', 'site');
  if (!is_string($db_name) || $db_name === '') {
    drush_set_error('PROVISION_UTF8MB4_NO_DB', dt('Could not read the database name from the site context.'));
    return;
  }

  $probe = _provision_tasks_extra_utf8mb4_probe($db_name);
  if ($probe === FALSE) {
    drush_set_error('PROVISION_UTF8MB4_PROBE_FAILED', dt('Could not inspect the charsets of database !db via information_schema.', array('!db' => $db_name)));
    return;
  }
  drush_log(dt('utf8mb4 probe: !t table(s) and !c text column(s) in database !db still use 3-byte utf8.', array('!t' => $probe['tables'], '!c' => $probe['columns'], '!db' => $db_name)), 'info');
  if ($probe['other'] > 0) {
    // 'info' on purpose: this is an advisory, and any 'warning' in the log
    // would turn the whole task badge yellow even on a verified success.
    drush_log(dt('!o table(s) use a non-utf8 charset (e.g. latin1). The converter switches their table default to utf8mb4 but leaves their non-utf8 content columns alone; review them manually if they matter.', array('!o' => $probe['other'])), 'info');
  }
  if ($probe['tables'] == 0 && $probe['columns'] == 0) {
    if ($probe['other'] > 0) {
      drush_log(dt('Nothing to convert: no 3-byte utf8 tables or text columns are left in database !db (the !o non-utf8 table(s) reported above stay as they are).', array('!db' => $db_name, '!o' => $probe['other'])), 'ok');
    }
    else {
      drush_log(dt('Nothing to convert: database !db is already fully utf8mb4.', array('!db' => $db_name)), 'ok');
    }
    return;
  }

  // Maintenance-first: a backup taken while writes still land would not
  // match the converted result. Same trio the update task uses above.
  // The shutdown net covers a hard death (fatal/OOM) inside the window;
  // the site context on disk keeps site_enabled=TRUE throughout, so a
  // plain Verify also self-heals a stranded maintenance flag.
  register_shutdown_function('_provision_tasks_extra_utf8mb4_shutdown');
  $GLOBALS['provision_tasks_extra_utf8mb4_offline'] = TRUE;
  drush_log(dt('Putting the site under maintenance for the conversion.'));
  d()->site_enabled = FALSE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();

  $failed = FALSE;

  // Backup first — no conversion without a fresh restore point. There is
  // deliberately no automatic restore on failure: writes could have landed
  // after the backup, so rolling back is the operator's call; every failure
  // message below names the backup file instead.
  $list = drush_commandfile_list();
  $provision_dir = dirname($list['provision']);
  include_once($provision_dir . '/platform/backup.provision.inc');
  $backup_file = drush_provision_drupal_provision_backup_suggest_filename();
  drush_set_option('force', TRUE);
  // Restorable mode (database inside the archive), as for the update task's
  // safety copy: this is the copy an operator restores by hand if the
  // conversion goes wrong, and the fast path's files-only archive cannot.
  // The child keeps the site in maintenance, as for the update task's copy.
  $success = provision_backend_invoke(d()->name, 'provision-backup', array($backup_file), array('selected_backup_mode' => 'site_files_with_mysqldump', 'provision_backup_keep_offline' => TRUE));
  drush_unset_option('force');
  // No drush_get_error() here: that flag is process-sticky, so a
  // recoverable error logged by an earlier step would misfire as a backup
  // failure. The invoke return shape is the authoritative signal — same
  // posture as the update task above.
  if (!$success || (is_array($success) && !empty($success['error_status']))) {
    drush_set_error('PROVISION_UTF8MB4_BACKUP_FAILED', dt('The pre-conversion backup failed; nothing was converted.'));
    $failed = TRUE;
  }

  if (!$failed) {
    drush_log(dt('Converting database !db to utf8mb4 / utf8mb4_unicode_ci. Pre-conversion backup: !file', array('!db' => $db_name, '!file' => $backup_file)), 'info');
    // Bounded retry: ALTER TABLE can lose a transient lock race (deadlock
    // 1213) against a live request even in maintenance mode, and a re-run
    // is safe by construction — already converted tables are skipped. The
    // attempts run un-integrated so a recovered attempt's error lines do
    // not poison the task log (any 'error' entry would pin the badge red);
    // each attempt's child log is relayed at controlled levels instead.
    $converted = FALSE;
    $attempt = 0;
    while ($attempt < 3 && !$converted) {
      $attempt++;
      if ($attempt > 1) {
        drush_log(dt('Conversion attempt !n of 3 — the previous attempt hit a transient database error; tables it already converted are skipped now.', array('!n' => $attempt)), 'info');
      }
      $values = drush_invoke_process(d()->name, 'utf8mb4-convert-databases', array(), array('yes' => TRUE), array('method' => 'GET', 'integrate' => FALSE, 'dispatch-using-alias' => TRUE));
      $converted = is_array($values) && empty($values['error_status']);
      // Demote a recovered attempt's errors to info; the last attempt's
      // errors surface as-is and fail the task honestly.
      _provision_tasks_extra_utf8mb4_relay_log($values, $converted || $attempt < 3);
    }
    if (!$converted) {
      drush_set_error('PROVISION_UTF8MB4_CONVERT_FAILED', dt('The conversion reported errors on !n attempt(s) and the database may now hold a mix of utf8 and utf8mb4 tables. Restore the pre-conversion backup !file, or resolve the cause and re-run this task — already converted tables are skipped on a re-run.', array('!n' => $attempt, '!file' => $backup_file)));
      $failed = TRUE;
    }
  }

  if (!$failed) {
    $verify = _provision_tasks_extra_utf8mb4_probe($db_name);
    if ($verify === FALSE) {
      drush_set_error('PROVISION_UTF8MB4_VERIFY_FAILED', dt('Could not re-inspect database !db after the conversion. The pre-conversion backup is !file.', array('!db' => $db_name, '!file' => $backup_file)));
      $failed = TRUE;
    }
    elseif ($verify['tables'] > 0 || $verify['columns'] > 0) {
      drush_set_error('PROVISION_UTF8MB4_VERIFY_FAILED', dt('Conversion incomplete: !t table(s) and !c text column(s) still use 3-byte utf8. Check the conversion log above; re-running this task is safe. The pre-conversion backup is !file.', array('!t' => $verify['tables'], '!c' => $verify['columns'], '!file' => $backup_file)));
      $failed = TRUE;
    }
  }

  drush_log(dt('Bringing the site out of maintenance.'));
  d()->site_enabled = TRUE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();
  $GLOBALS['provision_tasks_extra_utf8mb4_offline'] = FALSE;

  if (!$failed) {
    drush_log(dt('Database !db verified fully utf8mb4. The pre-conversion backup is kept at !file', array('!db' => $db_name, '!file' => $backup_file)), 'ok');
  }
}

/**
 * Shutdown net for the conversion's maintenance window.
 *
 * Runs only when the command died hard (fatal/OOM) between the maintenance
 * toggles; every graceful path clears the flag after bringing the site back
 * online itself. Best-effort by nature — a SIGKILL skips shutdown handlers
 * entirely, which is why the site context on disk is never flipped: a plain
 * Verify task always regenerates settings.php without the maintenance flag.
 */
function _provision_tasks_extra_utf8mb4_shutdown() {
  if (!empty($GLOBALS['provision_tasks_extra_utf8mb4_offline'])) {
    $GLOBALS['provision_tasks_extra_utf8mb4_offline'] = FALSE;
    d()->site_enabled = TRUE;
    _provision_drupal_create_settings_file();
    provision_drupal_push_site();
  }
}

/**
 * Implements drush_HOOK_pre_COMMAND().
 *
 * This runs for each tasks during the command
 *    drush @hostmaster hosting-tasks
 *
 * NOTE: This ONLY runs when being called from a hostmaster task.
 * This hook should ONLY be used to pass Options from a hostmaster task form to
 * the $task object, or if you don't need this functionality from the command
 * line.
 */
function drush_provision_tasks_extra_pre_hosting_task() {
  $task = &drush_get_context('HOSTING_TASK');

  // Update.
  if ($task->ref->type == 'site' && $task->task_type == 'update' && isset($task->task_args['entity_updates'])) {
    $task->options['entity_updates'] = $task->task_args['entity_updates'];
  }
}

/**
 * Refuses an Upgrade before anything is made.
 */
function _provision_tasks_extra_upgrade_refuse($why, $plan = array()) {
  if (!empty($plan['built'])) {
    return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', dt('UPGRADE/RESULT Not upgraded: !why. The site is on its platform as before.!built', array(
      '!why' => _provision_tasks_extra_no_markup($why),
      '!built' => _provision_tasks_extra_no_markup(_provision_tasks_extra_upgrade_built_kept($plan)),
    )));
  }
  return drush_set_error('PROVISION_UPGRADE_NOT_STARTED', dt('UPGRADE/RESULT Not started: !why. Nothing was changed.', array('!why' => _provision_tasks_extra_no_markup($why))));
}

/**
 * Two roots as one place on disk.
 */
function _provision_tasks_extra_upgrade_same_root($one, $two) {
  $a = @realpath($one);
  $b = @realpath($two);
  if ($a === FALSE || $b === FALSE) {
    return rtrim((string) $one, '/') === rtrim((string) $two, '/');
  }
  return $a === $b;
}

/**
 * The docroot a site's alias names on disk now, '' for none, FALSE for no alias.
 *
 * Read as text: the context this process loaded goes stale the moment a
 * nested task moves the site.
 */
function _provision_tasks_extra_upgrade_alias_root($uri) {
  $file = drush_server_home() . '/.drush/' . $uri . '.alias.drushrc.php';
  clearstatcache(TRUE, $file);
  if (!is_file($file) || is_link($file)) {
    return FALSE;
  }
  $text = @file_get_contents($file);
  $m = array();
  if (!is_string($text) || !preg_match("/'root'\\s*=>\\s*'((?:[^'\\\\]|\\\\.)*)'/", $text, $m)) {
    return '';
  }
  return _provision_tasks_extra_docroot(stripslashes($m[1]));
}

/**
 * The switched-on modules a site's own record holds, or FALSE.
 *
 * Each Verify writes the record into the site's drushrc.php, which is the
 * backend user's own file; it is read the way every Migrate reads it.
 *
 * @return array|false
 *   name => array('schema_version', 'filename'), FALSE when there is no
 *   record to read.
 */
function _provision_tasks_extra_upgrade_site_modules($file) {
  clearstatcache(TRUE, $file);
  $stat = @lstat($file);
  if ($stat === FALSE || ($stat['mode'] & 0170000) !== 0100000) {
    return FALSE;
  }
  if (function_exists('posix_geteuid') && $stat['uid'] !== posix_geteuid()) {
    return FALSE;
  }
  $record = _provision_tasks_extra_upgrade_include_options($file);
  if (!isset($record['packages']['modules']) || !is_array($record['packages']['modules'])) {
    return FALSE;
  }
  $modules = array();
  foreach ($record['packages']['modules'] as $name => $module) {
    if (is_array($module) && !empty($module['status']) && isset($module['schema_version'])) {
      $modules[$name] = array(
        'schema_version' => $module['schema_version'],
        'filename' => isset($module['filename']) ? (string) $module['filename'] : '',
      );
    }
  }
  return $modules;
}

/**
 * The $options a drushrc.php sets, in a scope of its own.
 */
function _provision_tasks_extra_upgrade_include_options($file) {
  $options = array();
  include $file;
  return is_array($options) ? $options : array();
}

/**
 * The Upgrade task whose way-back copy of a site a name is, or 0.
 *
 * Exactly back-<task nid>.temp.<site>: a copy of temp.<site> is not one.
 */
function _provision_tasks_extra_upgrade_copy_run($name, $uri) {
  $m = array();
  if (preg_match('/^back-([0-9]+)\.temp\.(.+)$/', $name, $m) && $m[2] === $uri && (int) $m[1] > 0) {
    return (int) $m[1];
  }
  return 0;
}

/**
 * The names under a folder that are way-back copies of a site.
 */
function _provision_tasks_extra_upgrade_copies($dir, $uri, $suffix = '') {
  $found = array();
  $names = @scandir($dir);
  if (!is_array($names)) {
    return $found;
  }
  foreach ($names as $name) {
    if ($suffix !== '' && (strlen($name) <= strlen($suffix) || substr($name, -strlen($suffix)) !== $suffix)) {
      continue;
    }
    $base = ($suffix === '') ? $name : substr($name, 0, strlen($name) - strlen($suffix));
    if (_provision_tasks_extra_upgrade_copy_run($base, $uri)) {
      $found[] = $base;
    }
  }
  return $found;
}

/**
 * The folder of the Upgrade runs' marks, or ''.
 *
 * In the account's own folder, which its shell user cannot write: a mark is
 * what lets a later run remove a copy.
 */
function _provision_tasks_extra_upgrade_marks_dir() {
  $server = d('@server_master');
  $root = (is_object($server) && !empty($server->aegir_root)) ? rtrim($server->aegir_root, '/') : '';
  return ($root === '' || !is_dir($root . '/log')) ? '' : $root . '/log/upgrade-runs';
}

/**
 * Writes this run's mark before a step: which step it is in.
 *
 * Rewritten before each step and removed when the run ends by its own code,
 * so a mark that stays tells a later run that this one was cut off, and in
 * which step.
 *
 * @return bool
 *   FALSE when the mark could not be written.
 */
function _provision_tasks_extra_upgrade_mark($plan, $phase) {
  $dir = _provision_tasks_extra_upgrade_marks_dir();
  if ($dir === '' || (!is_dir($dir) && !@mkdir($dir, 0700)) || is_link($dir)) {
    return FALSE;
  }
  $file = $dir . '/' . (int) $plan['task_nid'] . '.txt';
  $text = 'site=' . $plan['uri'] . "\n" . 'copy=' . $plan['copy'] . "\n" . 'phase=' . $phase . "\n"
    . 'pid=' . getmypid() . "\n" . 'source_root=' . $plan['source_root'] . "\n" . 'started=' . (int) $plan['started'] . "\n";
  if (@file_put_contents($file . '.new', $text) !== strlen($text) || !@rename($file . '.new', $file)) {
    @unlink($file . '.new');
    return FALSE;
  }
  drush_set_context('PROVISION_UPGRADE_MARKED', TRUE);
  return TRUE;
}

/**
 * Removes the mark of a run.
 */
function _provision_tasks_extra_upgrade_mark_clear($task_nid) {
  $dir = _provision_tasks_extra_upgrade_marks_dir();
  if ($dir !== '' && (int) $task_nid > 0) {
    @unlink($dir . '/' . (int) $task_nid . '.txt');
  }
}

/**
 * The marks runs of a site left, by task nid.
 *
 * Only a mark that is a plain file, names this site, its own copy and a
 * known step is taken; any other is no mark.
 *
 * @return array
 *   task nid => array with 'phase', 'pid', 'started', 'source_root', 'path'.
 */
function _provision_tasks_extra_upgrade_marks($uri) {
  $out = array();
  $dir = _provision_tasks_extra_upgrade_marks_dir();
  $names = ($dir === '' || is_link($dir)) ? FALSE : @scandir($dir);
  if (!is_array($names)) {
    return $out;
  }
  foreach ($names as $name) {
    $m = array();
    $path = $dir . '/' . $name;
    if (!preg_match('/^([0-9]+)\.txt$/', $name, $m) || is_link($path) || !is_file($path) || @filesize($path) > 4096) {
      continue;
    }
    $fields = array();
    foreach (explode("\n", (string) @file_get_contents($path)) as $line) {
      $pair = explode('=', $line, 2);
      if (count($pair) === 2) {
        $fields[$pair[0]] = $pair[1];
      }
    }
    $nid = (int) $m[1];
    if (!isset($fields['site'], $fields['copy'], $fields['phase'], $fields['pid'], $fields['started'], $fields['source_root'])
      || $fields['site'] !== $uri || $fields['copy'] !== 'back-' . $nid . '.temp.' . $uri
      || !in_array($fields['phase'], array('1', '1a', '1b', '2'), TRUE) || !ctype_digit($fields['pid']) || !ctype_digit($fields['started'])) {
      continue;
    }
    $out[$nid] = array(
      'phase' => $fields['phase'],
      'pid' => (int) $fields['pid'],
      'started' => (int) $fields['started'],
      'source_root' => $fields['source_root'],
      'path' => $path,
    );
  }
  return $out;
}

/**
 * Whether a process of an Upgrade run may still be alive.
 *
 * Each step of a run is a process of its own, and a step's own children
 * can outlive a runner that was killed: every process a run starts carries
 * BOA_UPGRADE_RUN=<its task nid>. TRUE also when this cannot be read.
 */
function _provision_tasks_extra_upgrade_run_alive($task_nid, $pid) {
  if ($pid > 0 && $pid != getmypid() && @file_exists('/proc/' . (int) $pid)) {
    return TRUE;
  }
  $self = @file_get_contents('/proc/self/environ');
  $dirs = @glob('/proc/[0-9]*', GLOB_ONLYDIR);
  if (!is_string($self) || $self === '' || !is_array($dirs) || !$dirs) {
    return TRUE;
  }
  $uid = function_exists('posix_getuid') ? posix_getuid() : -1;
  $needle = "\0BOA_UPGRADE_RUN=" . (int) $task_nid . "\0";
  foreach ($dirs as $dir) {
    if ($uid >= 0 && @fileowner($dir) !== $uid) {
      continue;
    }
    $env = @file_get_contents($dir . '/environ');
    if (is_string($env) && strpos("\0" . $env, $needle) !== FALSE) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * Checks an Upgrade before anything is made.
 *
 * @return array|string
 *   The plan of the run, or the reason it is refused, a clause with no full
 *   stop.
 */
function _provision_tasks_extra_upgrade_plan() {
  $site = d();
  if (!is_object($site) || $site->type !== 'site' || empty($site->uri)) {
    return 'this is not a site';
  }
  $uri = strtolower(ltrim((string) $site->uri, '@'));
  $opt = array();
  foreach (array('upgrade_target', 'upgrade_target_nid', 'upgrade_source_nid', 'upgrade_db_nid', 'upgrade_task_nid', 'upgrade_copy') as $name) {
    $opt[$name] = trim((string) drush_get_option($name, ''));
    if ($opt[$name] === '') {
      return 'the task did not say where the site should go (' . $name . ' is missing); start the Upgrade from the site\'s page';
    }
  }
  foreach (array('upgrade_target_nid', 'upgrade_source_nid', 'upgrade_db_nid', 'upgrade_task_nid') as $name) {
    if (!ctype_digit($opt[$name]) || (int) $opt[$name] < 1) {
      return 'the task carries a value that is no id (' . $name . '); start the Upgrade from the site\'s page';
    }
  }
  $copy = 'back-' . $opt['upgrade_task_nid'] . '.temp.' . $uri;
  if ($opt['upgrade_copy'] !== $copy) {
    return 'the name given for the way-back copy is not the one this run makes (' . $copy . ')';
  }
  // The move leg relies on the Migrate that fails closed: without it a
  // failed update would only warn.
  if (!function_exists('provision_upgrade_strict')) {
    return 'this server does not have the Migrate that stops on a failed update yet; it comes with the next system upgrade';
  }
  $source_title = trim((string) drush_get_option('upgrade_source_title', ''));
  $target_title = trim((string) drush_get_option('upgrade_target_title', ''));
  $source_title = ($source_title === '') ? 'its current platform' : $source_title;
  $target_title = ($target_title === '') ? 'the new platform' : $target_title;

  // V1: Drupal 10 or later on both sides, by a file.
  if (function_exists('provision_platform_is_backdrop') && provision_platform_is_backdrop($site)) {
    return 'the Upgrade is for Drupal 10 and later sites, and this is a Backdrop site';
  }
  if (function_exists('provision_platform_is_foreign_cms') && provision_platform_is_foreign_cms($site)) {
    return 'the Upgrade is for Drupal 10 and later sites, and this site runs another system';
  }
  if (function_exists('provision_is_hostmaster_site') && provision_is_hostmaster_site()) {
    return 'this is the control panel\'s own site';
  }
  $source_root = _provision_tasks_extra_docroot($site->root);
  $target = d($opt['upgrade_target'], FALSE, FALSE);
  if (!is_object($target) || $target->type !== 'platform' || empty($target->root)) {
    return $target_title . ' is not known to the backend as a platform; verify it first';
  }
  $target_root = _provision_tasks_extra_docroot($target->root);
  if (!is_file($source_root . '/core/lib/Drupal.php')) {
    return 'the Upgrade is for Drupal 10 and later sites';
  }
  if (!is_file($target_root . '/core/lib/Drupal.php')) {
    return $target_title . ' holds no Drupal 10 or later code';
  }
  // V2.
  if (_provision_tasks_extra_upgrade_is_catalogue($target_root)) {
    return $target_title . ' is one of the platforms the system ships, which are no upgrade targets';
  }
  // V8.
  if (_provision_tasks_extra_upgrade_same_root($source_root, $target_root)) {
    return 'this site already runs on ' . $target_title;
  }

  // V3: the two releases and the two locks; then both lists, as far as they
  // can be judged before the audits.
  $source_release = _provision_tasks_extra_core_release($source_root);
  $target_release = _provision_tasks_extra_core_release($target_root);
  $platform_modes = _provision_tasks_extra_upgrade_modes($source_root . '/sites/all/modules/boa_platform_control.ini');
  $site_modes = _provision_tasks_extra_upgrade_modes($source_root . '/sites/' . $uri . '/modules/boa_site_control.ini');
  $early = _provision_tasks_extra_upgrade_judge($source_release, $target_release, _provision_tasks_extra_lock_diff($source_root, $target_root), $platform_modes, $site_modes, TRUE);
  if (!$early['ok']) {
    return $early['why'];
  }
  // V3, the database half, from the site's own record.
  $modules = _provision_tasks_extra_upgrade_site_modules($source_root . '/sites/' . $uri . '/drushrc.php');
  if ($modules === FALSE || !isset($modules['system'])) {
    return 'Verify this site first: it has no record of its modules yet';
  }
  $floor = _provision_tasks_extra_database_floor($modules, $source_root, $target_root);
  foreach ($floor['not_read'] as $name) {
    drush_log(dt('UPGRADE/CHECK the last update the new code removed could not be read for @module; the database updates of the move decide.', array('@module' => $name)), 'warning');
  }
  $too_old = _provision_tasks_extra_database_floor_text($floor, $target_release);
  if ($too_old !== '') {
    return $too_old;
  }

  // V4: a Drush of its own, and not one known to die at start. It is not
  // run here: a locked codebase can run its Drush only inside the move.
  $app = _provision_tasks_extra_app_root($target_root);
  $has_drush = ($app !== '')
    ? _provision_codebase_has_drush($app)
    : (_provision_codebase_has_drush($target_root) || _provision_codebase_has_drush(dirname($target_root)));
  if (!$has_drush) {
    return $target_title . ' has no Drush of its own. Verify the platform (BOA adds Drush where it can), or put drush/drush into the project';
  }
  $dead = _provision_tasks_extra_drush_cannot_start($target_root);
  if ($dead !== '') {
    return "the new platform's Drush cannot start (" . $dead . '). Rebuild the platform, or require consolidation/robo 5.1.1 in it';
  }

  // V7: what an earlier run left behind.
  $stray = $target_root . '/sites/' . $uri;
  if (file_exists($stray) || is_link($stray)) {
    return 'a leftover of this site is in the way on ' . $target_title . ' (' . $stray . '). Delete it, then start again';
  }
  $left = _provision_tasks_extra_upgrade_leftovers($uri, $source_root, $source_title);
  if (is_string($left)) {
    return $left;
  }
  $tidy = $left['tidy'];
  $stale = $left['stale'];

  // V9, V11.
  if (file_exists('/run/boa_queue_stop.pid')) {
    return 'the task queue is paused on this server for maintenance; start again later';
  }
  if (file_exists('/run/octopus_install_run.pid')) {
    return 'a system upgrade of this account is running; start again when it has ended';
  }

  // V12.
  $secs = (int) drush_get_option('upgrade_copy_secs', 0);
  $ceiling = (int) drush_get_option('upgrade_ceiling', 0);
  if ($secs > 0 && $ceiling > 0) {
    if (4 * $secs + 1800 > $ceiling) {
      return 'the last copy of this site took ' . (int) ceil($secs / 60) . ' minutes, and an upgrade needs about four times that and half an hour for its checks: more than the ' . round($ceiling / 3600, 1) . ' hours one task may run';
    }
  }
  else {
    drush_log(dt('UPGRADE/CHECK no earlier Backup or Clone of this site says how long a copy takes; the time check is skipped.'), 'ok');
  }

  // V5: the target's PHP, asked the way the move will ask.
  if (function_exists('provision_cli_php_target_preflight') && !provision_cli_php_target_preflight('migrate', $opt['upgrade_target'])) {
    return $target_title . ' cannot run this site on its PHP version (see above)';
  }
  // V6: each leg's own space check, each alone.
  if (function_exists('provision_copy_task_space_preflight')) {
    if (!provision_copy_task_space_preflight('clone', '@' . ltrim((string) $site->platform->name, '@'), '@' . $copy)) {
      return 'the disk has too little room for the way-back copy (see above)';
    }
    if (!provision_copy_task_space_preflight('migrate', $opt['upgrade_target'])) {
      return 'the disk has too little room for the move (see above)';
    }
  }

  // V10: both audits. An advisory the site does not have now is named in the
  // plan and the result, never refused; a list that asks for security fixes
  // still refuses such a move, which is no security fix.
  $source_audit = _provision_tasks_extra_lock_advisories($source_root);
  $target_audit = _provision_tasks_extra_lock_advisories($target_root);
  $sides = array('current' => $source_audit, 'new' => $target_audit);
  foreach ($sides as $side => $audit) {
    if ($audit['lines'] === FALSE && $audit['lock'] !== '') {
      drush_log(dt('UPGRADE/CHECK the security audit of the @side platform did not finish (@why): advisories were not compared.', array('@side' => $side, '@why' => $audit['why'])), 'ok');
    }
  }
  $diff = _provision_tasks_extra_upgrade_release_step($source_release, $target_release, _provision_tasks_extra_lock_diff($source_root, $target_root, $source_audit, $target_audit));
  $added = '';
  // An older copy of the rules, loaded while the panel moves to a new tree,
  // names nothing.
  if (!empty($diff['brought']) && function_exists('_provision_tasks_extra_audit_pair_names')) {
    $names = _provision_tasks_extra_audit_pair_names($diff['brought']);
    $added = 'security ' . (count($names) === 1 ? 'advisory ' : 'advisories ') . _provision_tasks_extra_upgrade_names($names);
  }
  // V13: both lists, each on its own, with what the audits said.
  $kind = _provision_tasks_extra_upgrade_kind_text($diff);
  $verdict = _provision_tasks_extra_upgrade_judge($source_release, $target_release, $diff, $platform_modes, $site_modes, FALSE);
  if (!$verdict['ok']) {
    return $verdict['why'] . ' (' . $target_title . ' is: ' . $kind . ')';
  }

  $advisories = 'security advisories were not compared';
  if ($diff['security'] !== NULL || $diff['of'] > 0 || ($source_audit['lines'] !== FALSE && $target_audit['lines'] !== FALSE)) {
    $advisories = 'security advisories: ' . $diff['of'] . ' on the current platform, ' . count($diff['open']) . ' on the new one';
  }
  $lists = "the platform's list (" . basename($platform_modes['file']) . '): ' . _provision_tasks_extra_upgrade_modes_text($platform_modes)
    . "; this site's list (" . basename($site_modes['file']) . '): ' . _provision_tasks_extra_upgrade_modes_text($site_modes);
  // A platform this run built carries the line of the one it was built from.
  $stays = ($platform_modes['state'] === 'absent' || drush_get_option('upgrade_build', FALSE)) ? '' : ' The platform\'s list belongs to ' . $source_title . ' and does not follow the site to ' . $target_title . '.';
  drush_log(dt('UPGRADE/PLAN @uri: from Drupal @from on @source to Drupal @to on @target: !kind; !advisories; !lists. The way back will be @copy.!stays', array(
    '@uri' => $uri,
    '@from' => $source_release,
    '@source' => $source_title,
    '@to' => $target_release,
    '@target' => $target_title,
    '!kind' => _provision_tasks_extra_no_markup($kind),
    '!advisories' => $advisories,
    '!lists' => _provision_tasks_extra_no_markup($lists),
    '@copy' => $copy,
    '!stays' => _provision_tasks_extra_no_markup($stays),
  )), 'ok');
  drush_log(dt('UPGRADE/CHECK passed: the two releases, the site\'s database, the new platform\'s Drush and PHP, leftovers, the queue, free space and both lists.'), 'ok');

  return array(
    'uri' => $uri,
    'copy' => $copy,
    'marker' => 'upgrade_run=' . $opt['upgrade_task_nid'],
    'source_nid' => $opt['upgrade_source_nid'],
    'target_nid' => $opt['upgrade_target_nid'],
    'db_nid' => $opt['upgrade_db_nid'],
    'source_root' => $source_root,
    'target_root' => $target_root,
    'source_title' => $source_title,
    'target_title' => $target_title,
    'source_release' => $source_release,
    'target_release' => $target_release,
    'added' => $added,
    'stays' => $stays,
    'return' => (bool) drush_get_option('upgrade_return', FALSE),
    'task_nid' => (int) $opt['upgrade_task_nid'],
    'tidy' => $tidy,
    'stale_marks' => $stale,
  );
}

/**
 * What an earlier run of this site left beside it, V7's copies part: a
 * refusal, or the copies this run removes and the marks it clears.
 *
 * A copy that a run cut off before its move provably made (its mark says
 * so, nothing of it runs, its alias names this platform) is removed by this
 * run when that is switched on; anything else that is left stays a refusal.
 *
 * @return string|array
 *   The reason, or 'tidy' and 'stale'.
 */
function _provision_tasks_extra_upgrade_leftovers($uri, $source_root, $source_title) {
  $marks = _provision_tasks_extra_upgrade_marks($uri);
  $tidy_on = (bool) drush_get_option('upgrade_tidy', FALSE);
  $tidy = array();
  $in_dir = _provision_tasks_extra_upgrade_copies($source_root . '/sites', $uri);
  $names = $in_dir;
  foreach (_provision_tasks_extra_upgrade_copies(drush_server_home() . '/.drush', $uri, '.alias.drushrc.php') as $name) {
    $root = _provision_tasks_extra_upgrade_alias_root($name);
    if (!in_array($name, $names, TRUE) && ($root === '' || ($root !== FALSE && _provision_tasks_extra_upgrade_same_root($root, $source_root)))) {
      $names[] = $name;
    }
  }
  foreach ($names as $name) {
    $run = _provision_tasks_extra_upgrade_copy_run($name, $uri);
    $mark = isset($marks[$run]) ? $marks[$run] : NULL;
    $root = _provision_tasks_extra_upgrade_alias_root($name);
    $dir = $source_root . '/sites/' . $name;
    $when = is_array($mark) ? date('Y-m-d H:i', $mark['started']) : '';
    if (is_array($mark) && $mark['phase'] === '2') {
      return 'an earlier upgrade (task ' . $run . ') was cut off on ' . $when . ' during its move and left ' . $name . ' on ' . $source_title . '; what it left needs support (its mark is ' . $mark['path'] . ')';
    }
    if (is_array($mark) && _provision_tasks_extra_upgrade_run_alive($run, $mark['pid'])) {
      return 'an earlier upgrade of this site (task ' . $run . ') is still running on this server; start again when it has ended';
    }
    $provable = is_array($mark)
      && _provision_tasks_extra_upgrade_same_root($mark['source_root'], $source_root)
      && $root !== FALSE && $root !== '' && _provision_tasks_extra_upgrade_same_root($root, $source_root)
      && ((!file_exists($dir) && !is_link($dir)) || (is_dir($dir) && !is_link($dir)));
    if ($provable && $tidy_on) {
      $tidy[] = array('copy' => $name, 'run' => $run, 'when' => $when);
      continue;
    }
    if ($provable) {
      return 'an earlier upgrade (task ' . $run . ') was cut off on ' . $when . ' before it moved the site and left the copy ' . $name . ' on ' . $source_title . '. Delete that site in the panel, then start again; when the panel does not list it, ask support';
    }
    if (in_array($name, $in_dir, TRUE)) {
      return 'an earlier upgrade left the copy ' . $name . ' on ' . $source_title . '. Delete that site in the panel, or its folder ' . $dir . ' when the panel does not list it, then start again';
    }
    return 'an earlier upgrade left the copy ' . $name . ' on ' . $source_title . '. Delete that site in the panel, then start again';
  }
  // The marks of this site's runs whose copy is gone and that did not reach
  // their move: nothing is left of them to look at.
  $stale = array();
  foreach ($marks as $run => $mark) {
    $name = 'back-' . $run . '.temp.' . $uri;
    if ($mark['phase'] !== '2' && !in_array($name, $names, TRUE) && _provision_tasks_extra_upgrade_alias_root($name) === FALSE
      && !file_exists($source_root . '/sites/' . $name) && !is_link($source_root . '/sites/' . $name)
      && !_provision_tasks_extra_upgrade_run_alive($run, $mark['pid'])) {
      $stale[] = $run;
    }
  }
  return array('tidy' => $tidy, 'stale' => $stale);
}

/**
 * Implements drush_hook_COMMAND_validate() for provision-drupal-upgrade.
 *
 * Every check runs before anything is made, fails closed, and relies on
 * nothing the panel's form did.
 */
function drush_provision_tasks_extra_provision_drupal_upgrade_validate() {
  // An Upgrade that builds its target first: what needs no target now, the
  // rest of the plan once the build is kept.
  if (drush_get_option('upgrade_build', FALSE)) {
    $pre = _provision_tasks_extra_upgrade_build_plan();
    if (!is_array($pre)) {
      return _provision_tasks_extra_successor_unstarted($pre, 'UPGRADE/RESULT', 'PROVISION_UPGRADE_NOT_STARTED');
    }
    drush_set_context('PROVISION_UPGRADE_BUILD_PLAN', $pre);
    return;
  }
  $plan = _provision_tasks_extra_upgrade_plan();
  if (!is_array($plan)) {
    return _provision_tasks_extra_upgrade_refuse($plan);
  }
  drush_set_context('PROVISION_UPGRADE_PLAN', $plan);
}

/**
 * What an Upgrade that builds its target first checks before the build:
 * everything that needs no target. A plan, or why not.
 *
 * Build newer version's own plan on the site's platform, and the Upgrade's
 * checks of the site: its own record, the two lists (the kind a template's
 * Mode tells included), what an earlier run left, the time, the build's PHP
 * and the room for the way back and for a second codebase. After the panel's
 * start these fail only by a race.
 */
function _provision_tasks_extra_upgrade_build_plan() {
  if (!function_exists('_provision_tasks_extra_build_kind_before') || !function_exists('_provision_tasks_extra_build_time_why')
    || !function_exists('_provision_tasks_extra_build_php_why') || !function_exists('_provision_tasks_extra_codebase_kb')) {
    return 'the backend\'s copy of the task\'s rules is older than the panel\'s; start again in a few minutes';
  }
  $site = d();
  if (!is_object($site) || $site->type !== 'site' || empty($site->uri)) {
    return 'this is not a site';
  }
  $uri = strtolower(ltrim((string) $site->uri, '@'));
  $opt = array();
  foreach (array('upgrade_target', 'upgrade_target_nid', 'upgrade_source_nid', 'upgrade_db_nid', 'upgrade_task_nid', 'upgrade_copy', 'upgrade_started') as $name) {
    $opt[$name] = trim((string) drush_get_option($name, ''));
    if ($opt[$name] === '') {
      return 'the task did not say what to build (' . $name . ' is missing); start the Upgrade from the site\'s page';
    }
  }
  foreach (array('upgrade_target_nid', 'upgrade_source_nid', 'upgrade_db_nid', 'upgrade_task_nid', 'upgrade_started') as $name) {
    if (!ctype_digit($opt[$name]) || (int) $opt[$name] < 1) {
      return 'the task carries a value that is no id (' . $name . '); start the Upgrade from the site\'s page';
    }
  }
  if ($opt['upgrade_copy'] !== 'back-' . $opt['upgrade_task_nid'] . '.temp.' . $uri) {
    return 'the name given for the way-back copy is not the one this run makes';
  }
  if (trim((string) drush_get_option('successor_context', '')) !== $opt['upgrade_target']
    || (string) (int) drush_get_option('successor_nid', 0) !== $opt['upgrade_target_nid']
    || (string) (int) drush_get_option('successor_task_nid', 0) !== $opt['upgrade_task_nid']) {
    return 'the platform to build is not the one the Upgrade names as its target';
  }
  if (!function_exists('provision_upgrade_strict')) {
    return 'this server does not have the Migrate that stops on a failed update yet; it comes with the next system upgrade';
  }
  if ((function_exists('provision_platform_is_backdrop') && provision_platform_is_backdrop($site))
    || (function_exists('provision_platform_is_foreign_cms') && provision_platform_is_foreign_cms($site))) {
    return 'the Upgrade is for Drupal 10 and later sites';
  }
  if (function_exists('provision_is_hostmaster_site') && provision_is_hostmaster_site()) {
    return 'this is the control panel\'s own site';
  }
  $source_root = _provision_tasks_extra_docroot($site->root);
  if (!is_file($source_root . '/core/lib/Drupal.php')) {
    return 'the Upgrade is for Drupal 10 and later sites';
  }
  $platform = (isset($site->platform) && is_object($site->platform)) ? $site->platform : NULL;
  if (!$platform || empty($platform->root) || !_provision_tasks_extra_upgrade_same_root(_provision_tasks_extra_docroot($platform->root), $source_root)) {
    return 'this site\'s platform is not known to the backend as the one it runs on; verify the site first';
  }
  // Build newer version's own checks, on the site's platform.
  $build = _provision_tasks_extra_successor_plan($platform);
  if (!is_array($build)) {
    return $build;
  }
  $source_title = trim((string) drush_get_option('upgrade_source_title', ''));
  $source_title = ($source_title === '') ? 'its current platform' : $source_title;
  // The site's own record of its modules.
  $modules = _provision_tasks_extra_upgrade_site_modules($source_root . '/sites/' . $uri . '/drushrc.php');
  if ($modules === FALSE || !isset($modules['system'])) {
    return 'Verify this site first: it has no record of its modules yet';
  }
  // The two lists: switched off or unreadable, and the kind a template's
  // Mode tells before the build.
  $platform_modes = _provision_tasks_extra_upgrade_modes($source_root . '/sites/all/modules/boa_platform_control.ini');
  $site_modes = _provision_tasks_extra_upgrade_modes($source_root . '/sites/' . $uri . '/modules/boa_site_control.ini');
  foreach (array("the platform's" => $platform_modes, "this site's" => $site_modes) as $whose => $modes) {
    if ($modes['state'] === 'off' || $modes['state'] === 'unreadable') {
      return _provision_tasks_extra_upgrade_list_refusal($whose, $modes, _provision_tasks_extra_upgrade_modes_allow($modes, array()));
    }
  }
  $kind = _provision_tasks_extra_build_kind_before((bool) drush_get_option('successor_template', FALSE), trim((string) drush_get_option('successor_mode', '')), isset($build['choice']) ? $build['choice'] : NULL);
  $why = _provision_tasks_extra_build_lists_refusal($kind, $platform_modes, $site_modes);
  if ($why !== '') {
    return $why;
  }
  // V7: what an earlier run of this site left.
  $left = _provision_tasks_extra_upgrade_leftovers($uri, $source_root, $source_title);
  if (is_string($left)) {
    return $left;
  }
  // V12, as today, against the whole time.
  $secs = (int) drush_get_option('upgrade_copy_secs', 0);
  $ceiling = (int) drush_get_option('upgrade_ceiling', 0);
  if ($secs > 0 && $ceiling > 0 && 4 * $secs + 1800 > $ceiling) {
    return 'the last copy of this site took ' . (int) ceil($secs / 60) . ' minutes, and an upgrade needs about four times that and half an hour for its checks: more than the ' . round($ceiling / 3600, 1) . ' hours one task may run';
  }
  // The build's PHP, where the version chosen names Drupal's major.
  if ($build['major'] > 0) {
    $why = _provision_tasks_extra_build_php_why(_provision_tasks_extra_build_php($build['path']), _provision_tasks_extra_php_minimum($build['major'], ($build['major'] === $build['source_major']) ? $build['source_lock'] : NULL), $build['major']);
    if ($why !== '') {
      return $why;
    }
  }
  // Room for the way back, and for a second codebase beside the first.
  if (function_exists('provision_copy_task_space_preflight')
    && !provision_copy_task_space_preflight('clone', '@' . ltrim((string) $platform->name, '@'), '@' . $opt['upgrade_copy'])) {
    return 'the disk has too little room for the way-back copy (see above)';
  }
  $from = _provision_tasks_extra_successor_path(drush_get_option('successor_source_path', ''));
  $docroot = ($build['source_root'] !== $from && dirname($build['source_root']) === $from) ? basename($build['source_root']) : '';
  $kb = _provision_tasks_extra_codebase_kb($from, $docroot);
  $free = @disk_free_space(dirname($build['path']));
  if ($kb > 0 && $free !== FALSE && $kb * 1024 > $free) {
    return 'the disk has too little room for the newer version: its codebase ' . $from . ' uses ' . _provision_tasks_extra_human_bytes($kb * 1024) . ' and ' . _provision_tasks_extra_human_bytes((int) $free) . ' is free';
  }
  return array(
    'build' => $build,
    'uri' => $uri,
    'source_root' => $source_root,
    'source_title' => $source_title,
    'source_release' => $build['source_release'],
    'task_nid' => (int) $opt['upgrade_task_nid'],
    'source_nid' => (int) $opt['upgrade_source_nid'],
    'started' => (int) $opt['upgrade_started'],
  );
}

/**
 * An Upgrade that builds its target first: the build, the two judgements,
 * then today's Upgrade from its plan on.
 *
 * Nothing of the site is copied or moved before a build is kept and both
 * judgements say the site may move onto it; a kept build is never deleted.
 */
function _provision_tasks_extra_upgrade_build_run($pre) {
  $build = $pre['build'];
  $after = $pre['uri'] . ' runs Drupal ' . $pre['source_release'] . ' on ' . $pre['source_title'] . ' as before; nothing on it was changed.';
  $built = _provision_tasks_extra_successor_build($build, array($build['marker'], 'successor_from=' . (int) $pre['source_nid']));
  if ($built['state'] === 'not_kept') {
    return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', 'UPGRADE/RESULT Not upgraded: the newer version was not kept: ' . $built['why'] . '. ' . $built['tail'] . ' ' . _provision_tasks_extra_no_markup($after));
  }
  $head = 'Built ' . $build['title'] . ' at ' . $build['built_root'] . ': ' . $build['kind'] . '.';
  if ($built['state'] === 'locked') {
    return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', dt('UPGRADE/RESULT !head Not upgraded: it stays Locked, since !why; !next, then press Upgrade and choose it. !after', array(
      '!head' => _provision_tasks_extra_no_markup($head),
      '!why' => $built['why'],
      '!next' => $built['next'],
      '!after' => _provision_tasks_extra_no_markup($after),
    )));
  }
  drush_log('UPGRADE/BUILD ' . _provision_tasks_extra_no_markup($head), 'ok');
  $kept =' ' . $build['title'] . ' is kept: press Upgrade and choose it';
  // The time, asked again against what is left of it.
  $ceiling = (int) drush_get_option('upgrade_ceiling', 0);
  $elapsed = time() - $pre['started'];
  $why = _provision_tasks_extra_build_time_why((int) drush_get_option('upgrade_copy_secs', 0), $ceiling, $elapsed);
  if ($why !== '') {
    return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', _provision_tasks_extra_no_markup('UPGRADE/RESULT ' . $head . ' Not upgraded: ' . $why . '. ' . $after . $kept . '.'));
  }
  if ($ceiling > 0) {
    drush_set_option('upgrade_ceiling', max(1, $ceiling - max(0, $elapsed)));
  }
  // The panel's judgement, asked of a panel process of its own: a step's own
  // process keeps what it read before the build was unlocked.
  $why = _provision_tasks_extra_upgrade_panel_judge($pre['task_nid']);
  if ($why === '') {
    $plan = _provision_tasks_extra_upgrade_plan();
    $why = is_array($plan) ? '' : $plan;
  }
  if ($why !== '') {
    return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', _provision_tasks_extra_no_markup('UPGRADE/RESULT ' . $head . ' Not upgraded: ' . $why . '. ' . $after . $kept . ' once that is put right.'));
  }
  $plan['built'] = $build['title'];
  return _provision_tasks_extra_upgrade_run($plan);
}

/**
 * Why the panel says the site may not move onto the build, or ''.
 *
 * The panel's own judgement for the users who queued the task, asked of
 * hosting-drupal-upgrade-judge. No answer is a refusal.
 */
function _provision_tasks_extra_upgrade_panel_judge($task_nid) {
  $result = _provision_tasks_extra_quiet_invoke('@hostmaster', 'hosting-drupal-upgrade-judge', array((int) $task_nid), array());
  $answer = (is_array($result) && isset($result['object']) && is_array($result['object'])) ? $result['object'] : NULL;
  if (!is_array($answer) || !isset($answer['ready'])) {
    $error = '';
    if (is_array($result) && !empty($result['error_log']) && is_array($result['error_log'])) {
      foreach ($result['error_log'] as $messages) {
        $error = trim(preg_replace('/\s+/', ' ', (string) reset($messages)));
        break;
      }
    }
    return 'the panel could not say whether this site may move onto it (' . ($error !== '' ? $error : 'it gave no answer') . ')';
  }
  if (!empty($answer['ready']) && isset($result['error_status']) && (int) $result['error_status'] === 0) {
    return '';
  }
  $why = trim((string) (isset($answer['why']) ? $answer['why'] : ''));
  return ($why !== '') ? $why : 'the panel did not say that this site may move onto it';
}

/**
 * Runs one nested task and says how it went.
 *
 * A leg is judged by what its own invoke returned, never by this process's
 * error flag, which stays set from the first failed leg on. A leg that made
 * no task row returns an error like any other.
 *
 * A quiet leg is one the run may get past when it fails: nothing of it is
 * taken into this process while it runs (_provision_tasks_extra_quiet_invoke()),
 * and what it logged comes back in 'log' for
 * _provision_tasks_extra_upgrade_replay(). It counts as done only with the
 * closing report of its own process.
 *
 * @return array
 *   'ok', 'codes' (its error codes), 'errors' (their messages) and 'log'
 *   (what it logged, as this process received it).
 */
function _provision_tasks_extra_upgrade_leg($context, $type, $args = array(), $quiet = FALSE) {
  $before = count(drush_get_log());
  $command = array_merge(array($context, $type), $args);
  if ($quiet) {
    $result = _provision_tasks_extra_quiet_invoke('@hostmaster', 'hosting-task', $command, array('force' => TRUE));
  }
  else {
    $result = provision_backend_invoke('@hostmaster', 'hosting-task', $command, array('force' => TRUE));
  }
  $codes = array();
  $errors = array();
  if (is_array($result) && !empty($result['error_log']) && is_array($result['error_log'])) {
    foreach ($result['error_log'] as $code => $messages) {
      $codes[] = (string) $code;
      foreach ((array) $messages as $message) {
        $errors[] = (string) $message;
      }
    }
  }
  $ok = is_array($result) && isset($result['error_status']) && (int) $result['error_status'] === 0;
  // A step the panel could not start because it is moving to its new
  // version: its hosting-task stops at bootstrap and says so only on its
  // own output, which no log line here carries.
  if (!$ok && _provision_tasks_extra_upgrade_panel_moving()) {
    $codes[] = 'PROVISION_UPGRADE_PANEL_MOVING';
    $errors[] = 'The control panel is moving to its new version.';
  }
  if ($quiet) {
    return array(
      'ok' => $ok && array_key_exists('error_log', $result),
      'codes' => $codes,
      'errors' => $errors,
      'log' => (is_array($result) && isset($result['log']) && is_array($result['log'])) ? $result['log'] : array(),
      // What a step that ended with no closing report printed, as Drush
      // gave it back.
      'output' => (is_array($result) && !array_key_exists('error_log', $result) && isset($result['output']) && is_string($result['output'])) ? $result['output'] : '',
    );
  }
  return array(
    'ok' => $ok,
    'codes' => $codes,
    'errors' => $errors,
    'log' => array_slice(drush_get_log(), $before),
  );
}

/**
 * Whether this account's control panel is moving to its new version now.
 */
function _provision_tasks_extra_upgrade_panel_moving() {
  $hm = d('@hostmaster');
  return is_object($hm) && !empty($hm->root) && _provision_tasks_extra_panel_moving_at($hm->root, time());
}

/**
 * The reason a step gives: the panel's move when that stopped it, else $why.
 */
function _provision_tasks_extra_upgrade_step_why($leg, $why) {
  if (in_array('PROVISION_UPGRADE_PANEL_MOVING', $leg['codes'], TRUE)) {
    return 'the control panel is moving to its new version, so the panel could not start this step; start again in a few minutes';
  }
  return $why;
}

/**
 * provision_backend_invoke() for a step the run may get past when it fails.
 *
 * A step's errors, taken in as they come, set this process's error flag and
 * reach the panel at once, and its log lines of the error level set the
 * task's status: either ends the run in Error, whatever it does next. So
 * nothing of the step is taken in while it runs, and a step that ended with
 * no closing report is read as failed instead of being raised here. Drush
 * still raises a step that died of a fatal PHP error (exit code 255).
 */
function _provision_tasks_extra_quiet_invoke($target, $command, $arguments, $data) {
  $context = '@' . ltrim($target, '@');
  $backend_options = array(
    'method' => 'GET',
    'integrate' => FALSE,
    'log' => FALSE,
    'output' => FALSE,
    'convert-stdout-to-backend' => TRUE,
    'dispatch-using-alias' => TRUE,
  );
  // As provision_backend_invoke(): the Drush running Aegir, not a platform's.
  $record = drush_sitealias_get_record($context);
  if (empty($record['remote-host'])) {
    $backend_options['drush-script'] = DRUSH_COMMAND;
  }
  return drush_invoke_process($context, $command, $arguments, $data, $backend_options);
}

/**
 * Logs here what a quiet leg logged, its errors as warnings when $demote.
 *
 * A step run by a task is in Drush's backend mode, where an error goes to
 * the caller and into no line of the step's own log: its errors are logged
 * after its log, and after them what a step that ended with no closing
 * report printed last.
 */
function _provision_tasks_extra_upgrade_replay($leg, $demote) {
  foreach ($leg['log'] as $entry) {
    if (!is_array($entry) || !isset($entry['type'], $entry['message'])) {
      continue;
    }
    $message = is_array($entry['message']) ? implode("\n", $entry['message']) : (string) $entry['message'];
    $type = (string) $entry['type'];
    if ($demote && ($type === 'error' || $type === 'failed')) {
      $type = 'warning';
    }
    drush_log($message, $type);
  }
  foreach ($leg['errors'] as $error) {
    drush_log((string) $error, $demote ? 'warning' : 'error');
  }
  // Raw output, never decoded: only printable ASCII and line breaks go into
  // a log line, and no angle brackets, which Drush's own report markers use.
  $output = isset($leg['output']) ? (string) $leg['output'] : '';
  $output = trim(_provision_tasks_extra_no_markup(preg_replace('/[^\x20-\x7e\n]+/', ' ', $output)));
  if ($output !== '') {
    if (strlen($output) > 2000) {
      $output = '...' . substr($output, -2000);
    }
    drush_log('The step ended with no closing report; what it printed last: ' . $output, 'warning');
  }
}

/**
 * Whether a leg logged a line that starts with a marker, at one level or any.
 */
function _provision_tasks_extra_upgrade_logged($leg, $marker, $type = NULL) {
  foreach ($leg['log'] as $entry) {
    if (!is_array($entry) || !isset($entry['message']) || !is_string($entry['message'])) {
      continue;
    }
    if (strpos($entry['message'], $marker) !== FALSE && ($type === NULL || (isset($entry['type']) && $entry['type'] === $type))) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * The warnings this process holds, one printable line each.
 */
function _provision_tasks_extra_upgrade_warnings() {
  $warnings = array();
  foreach (drush_get_log() as $entry) {
    if (is_array($entry) && isset($entry['type'], $entry['message']) && $entry['type'] === 'warning' && is_string($entry['message'])) {
      $text = trim(preg_replace('/\s+/', ' ', strip_tags($entry['message'])));
      if ($text !== '') {
        $warnings[$text] = TRUE;
      }
    }
  }
  return array_keys($warnings);
}

/**
 * Why the move leg failed, as a clause.
 */
function _provision_tasks_extra_upgrade_why($leg) {
  if (in_array('PROVISION_UPGRADE_PANEL_MOVING', $leg['codes'], TRUE)) {
    return _provision_tasks_extra_upgrade_step_why($leg, '');
  }
  $text = implode(' ', $leg['errors']);
  if (in_array('PROVISION_UPGRADE_UPDATEDB', $leg['codes'], TRUE)) {
    if (strpos($text, 'could not be run') !== FALSE) {
      return "the new platform's own Drush could not be run";
    }
    if (strpos($text, 'cannot start') !== FALSE) {
      return "the new platform's own Drush cannot start";
    }
    // The updates ran; only Drupal's requirements check failed before them.
    if (strpos($text, 'requirements check reported errors') !== FALSE) {
      return "Drupal's requirements check reported errors on the new platform (its database updates ran)";
    }
    return "Drupal's database updates did not finish on the new platform";
  }
  if (in_array('PROVISION_UPGRADE_PAGE_CHECK', $leg['codes'], TRUE)) {
    $m = array();
    if (preg_match('/After the move (.+?): the move is undone/s', $text, $m)) {
      return 'after the move ' . trim(preg_replace('/\s+/', ' ', strip_tags($m[1])));
    }
    return 'after the move a page no longer answered as it did before (see above)';
  }
  if (in_array('PROVISION_INSUFFICIENT_SPACE', $leg['codes'], TRUE) || in_array('PROVISION_INSUFFICIENT_INODES', $leg['codes'], TRUE)) {
    return 'the disk was too full for the move (see above)';
  }
  if (in_array('PROVISION_CLI_PHP_TARGET', $leg['codes'], TRUE)) {
    return 'the new platform cannot run this site on its PHP version (see above)';
  }
  return 'the move stopped before the site left its platform (see above)';
}

/**
 * Removes the copy this run made, and nothing else.
 *
 * @return bool|null
 *   NULL when there is nothing of the copy, TRUE when it is gone, FALSE
 *   when something of it is left.
 */
function _provision_tasks_extra_upgrade_undo($plan) {
  $copy = $plan['copy'];
  $dir = $plan['source_root'] . '/sites/' . $copy;
  clearstatcache();
  $root = _provision_tasks_extra_upgrade_alias_root($copy);
  if ($root === FALSE && !file_exists($dir) && !is_link($dir)) {
    return NULL;
  }
  $clean = TRUE;
  // A delete drops the database the copy's settings name, and a deploy cut
  // off before it wrote the copy's own still names the site's: such a copy
  // is left for support, never deleted.
  $unsafe = _provision_tasks_extra_upgrade_copy_db_unsafe($dir, $plan);
  if ($unsafe !== '') {
    drush_log(dt('UPGRADE/UNDO @copy is left as it is: @why. Ask support.', array('@copy' => $copy, '@why' => $unsafe)), 'warning');
    return FALSE;
  }
  // Only a copy on the site's own platform is this run's.
  if ($root !== FALSE && $root !== '' && _provision_tasks_extra_upgrade_same_root($root, $plan['source_root'])) {
    // Quiet: a panel with no record of the copy refuses the Delete with an
    // error of its own, which must not end in Error a run that then removes
    // the copy by the backend and goes on to upgrade the site.
    $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'delete', array($plan['marker']), TRUE);
    // A panel moving to its new version makes no task either: its own
    // hosting-task stops at bootstrap, with no error code of hosting's.
    $no_task = !$leg['ok'] && (in_array('HOSTING_TASK_LOAD_FAILED', $leg['codes'], TRUE) || in_array('PROVISION_UPGRADE_PANEL_MOVING', $leg['codes'], TRUE));
    // A copy another run left: whether the panel holds it is not known, and
    // only a panel that made no task for want of a record of it says no.
    $recorded = isset($plan['recorded']) ? $plan['recorded'] : FALSE;
    if ($recorded === 'unknown') {
      $recorded = !in_array('HOSTING_TASK_LOAD_FAILED', $leg['codes'], TRUE);
    }
    if ($no_task && $recorded) {
      // The panel made its record of the copy when the Clone ended well,
      // and still made no task now (it is moving to its new version, say).
      // Removing the copy here would leave that record with nothing behind
      // it: the copy stays, for a Delete in the panel.
      _provision_tasks_extra_upgrade_replay($leg, FALSE);
      drush_log(dt('UPGRADE/UNDO the panel could not start the Delete of @copy now: the copy is left as it is. Delete it in the panel.', array('@copy' => $copy)), 'warning');
    }
    elseif ($no_task) {
      // A Clone that failed after its deploy keeps the copy and makes no
      // panel record of it: no task row can delete it, the backend can.
      // A panel without a record of the copy says no more than the line
      // below; a panel that could not start the Delete says why.
      if (!in_array('HOSTING_TASK_LOAD_FAILED', $leg['codes'], TRUE)) {
        _provision_tasks_extra_upgrade_replay($leg, FALSE);
      }
      drush_log(dt('UPGRADE/UNDO the panel holds no record of @copy, so it is removed by the backend alone.', array('@copy' => $copy)), 'ok');
      $before = count(drush_get_log());
      $result = provision_backend_invoke('@' . $copy, 'provision-delete');
      $leg = array(
        'ok' => is_array($result) && isset($result['error_status']) && (int) $result['error_status'] === 0,
        'codes' => array(),
        'errors' => array(),
        'log' => array_slice(drush_get_log(), $before),
      );
      if ($leg['ok']) {
        provision_backend_invoke('@none', 'provision-save', array('@' . $copy), array('delete' => TRUE));
      }
    }
    else {
      // The panel's Delete ran: what it logged belongs in this run's log.
      _provision_tasks_extra_upgrade_replay($leg, FALSE);
    }
    foreach ($leg['log'] as $entry) {
      if (is_array($entry) && isset($entry['type'], $entry['message']) && $entry['type'] === 'warning' && is_string($entry['message']) && stripos($entry['message'], 'database') !== FALSE) {
        $clean = FALSE;
      }
    }
    $clean = $clean && $leg['ok'];
  }
  clearstatcache();
  return $clean && _provision_tasks_extra_upgrade_alias_root($copy) === FALSE && !file_exists($dir) && !is_link($dir);
}

/**
 * Why deleting a way-back copy could drop another database, or ''.
 *
 * A delete drops the database the copy's folder names. A copy whose deploy
 * was cut off before it wrote its own settings still names the site's
 * database, as provision's own clean-up of an unfinished copy knows.
 */
function _provision_tasks_extra_upgrade_copy_db_unsafe($dir, $plan) {
  if (!file_exists($dir) && !is_link($dir)) {
    return '';
  }
  if (is_link($dir) || !is_dir($dir)) {
    return 'its folder is not a plain folder';
  }
  if (!function_exists('_provision_target_copy_parse_db')) {
    return 'this server cannot read the database it names';
  }
  $parsed = _provision_target_copy_parse_db($dir);
  if ($parsed['db_name'] === '') {
    return 'the database it names cannot be read';
  }
  $own = array();
  if (!empty($plan['uri'])) {
    $site = _provision_target_copy_parse_db($plan['source_root'] . '/sites/' . $plan['uri']);
    if ($site['db_name'] !== '') {
      $own[] = $site['db_name'];
    }
  }
  $option = drush_get_option('db_name', '', 'site');
  if (is_string($option) && $option !== '') {
    $own[] = $option;
  }
  if (!$own) {
    return "the site's own database cannot be read to tell the two apart";
  }
  if (in_array($parsed['db_name'], $own, TRUE)) {
    return "its settings still name the site's own database (" . $parsed['db_name'] . ')';
  }
  return '';
}

/**
 * Ends a run that did not upgrade the site: the copy goes, the reason stays.
 */
function _provision_tasks_extra_upgrade_stop($plan, $why, $move_tried) {
  $removed = _provision_tasks_extra_upgrade_undo($plan);
  $line = 'Not upgraded: ' . $why . '. ' . $plan['uri'] . ' is on ' . $plan['source_title'] . ' as before'
    . ($move_tried ? '; its page cache was cleared.' : '.');
  if ($removed === TRUE) {
    $line .= ' The copy made for this run was removed.';
  }
  elseif ($removed === FALSE) {
    $line .= ' The copy made for this run (' . $plan['copy'] . ') could not be fully removed, see above.';
  }
  $line .= _provision_tasks_extra_upgrade_built_kept($plan);
  return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', dt('UPGRADE/RESULT !line', array('!line' => _provision_tasks_extra_no_markup($line))));
}

/**
 * The sentence a result line of a run that built its target ends with, or ''.
 */
function _provision_tasks_extra_upgrade_built_kept($plan) {
  return empty($plan['built']) ? '' : ' ' . $plan['built'] . ', built by this task, is kept.';
}

/**
 * A size in bytes as people read it.
 */
function _provision_tasks_extra_human_bytes($bytes) {
  $units = array('GB' => 1073741824, 'MB' => 1048576, 'KB' => 1024);
  foreach ($units as $unit => $size) {
    if ($bytes >= $size) {
      return round($bytes / $size, 1) . ' ' . $unit;
    }
  }
  return (int) $bytes . ' bytes';
}

/**
 * The database a site's folder names, in bytes, or FALSE.
 *
 * The name from the folder's own drushrc.php, read and never included, and
 * the size from the database server of the site this process runs for: a
 * way-back copy is made on that same server. The figure is worth no warning
 * and no failure of the run, so it is read on a connection of its own,
 * opened quietly (the one the run opened before its steps can have timed
 * out since), and nothing it meets is logged.
 */
function _provision_tasks_extra_upgrade_db_bytes($site_dir) {
  $body = @file_get_contents($site_dir . '/drushrc.php');
  $m = array();
  if (!is_string($body) || !preg_match('/\$options\s*\[\s*[\'"]db_name[\'"]\s*\]\s*=\s*[\'"]([A-Za-z0-9_]+)[\'"]/', $body, $m)) {
    return FALSE;
  }
  try {
    $db = d()->service('db');
    if (!is_object($db) || !($db instanceof Provision_Service_db_pdo)) {
      return FALSE;
    }
    $db->close();
    if (!$db->connect(TRUE) || !is_object($db->conn)) {
      $db->close();
      return FALSE;
    }
    $statement = @$db->conn->prepare('SELECT COALESCE(SUM(data_length + index_length), 0) FROM information_schema.tables WHERE table_schema = ?');
    $row = ($statement && @$statement->execute(array($m[1]))) ? $statement->fetch(PDO::FETCH_NUM) : FALSE;
    $db->close();
    return (is_array($row) && isset($row[0])) ? (float) $row[0] : FALSE;
  }
  catch (Exception $e) {
    return FALSE;
  }
}

/**
 * What a way-back copy uses on disk, as the end of a log line, or ''.
 *
 * Its folder followed through its links, as a copy of it would be, without
 * the cache folders an archive leaves out; and its own database. Each figure
 * is also given in bytes, for the panel to read.
 */
function _provision_tasks_extra_upgrade_copy_size($copy_dir) {
  $files = function_exists('provision_site_bytes') ? provision_site_bytes($copy_dir, TRUE) : FALSE;
  if ($files !== FALSE && function_exists('provision_site_excluded_bytes')) {
    $excluded = provision_site_excluded_bytes($copy_dir);
    if ($excluded > 0 && $excluded < $files) {
      $files -= $excluded;
    }
  }
  $database = _provision_tasks_extra_upgrade_db_bytes($copy_dir);
  if ($files === FALSE && $database === FALSE) {
    return '';
  }
  $parts = array();
  $parts[] = ($files === FALSE) ? 'files not measured' : sprintf('files %.0f bytes', $files);
  $parts[] = ($database === FALSE) ? 'database not measured' : sprintf('database %.0f bytes', $database);
  $total = ($files === FALSE ? 0 : $files) + ($database === FALSE ? 0 : $database);
  $about = ($files === FALSE || $database === FALSE) ? 'at least ' : 'about ';
  return $about . _provision_tasks_extra_human_bytes($total) . ' on disk (' . implode(', ', $parts) . ')';
}

/**
 * The folder that keeps the database as it was before the move, or ''.
 */
function _provision_tasks_extra_upgrade_kept_dump($uri, $since) {
  $server = d('@server_master');
  $path = (is_object($server) && !empty($server->backup_path)) ? rtrim($server->backup_path, '/') : '';
  $names = ($path === '') ? FALSE : @scandir($path);
  if (!is_array($names)) {
    return '';
  }
  $prefix = $uri . '_sql_dump_deploy_';
  $found = '';
  $newest = 0;
  foreach ($names as $name) {
    if (strncmp($name, $prefix, strlen($prefix)) !== 0 || !is_dir($path . '/' . $name) || is_link($path . '/' . $name)) {
      continue;
    }
    $mtime = (int) @filemtime($path . '/' . $name);
    if ($mtime >= $since && $mtime >= $newest) {
      $newest = $mtime;
      $found = $path . '/' . $name;
    }
  }
  return $found;
}

/**
 * Implements the provision-drupal-upgrade command.
 *
 * Two existing tasks as real task rows inside this one: a Clone of the site
 * on its own platform, switched off, as the way back; then a Migrate onto
 * the new platform that undoes itself when the updates do not finish or a
 * page no longer answers. Whatever fails before the site has moved ends
 * with the site where it was and the copy removed.
 */
function drush_provision_tasks_extra_provision_drupal_upgrade() {
  $pre = drush_get_context('PROVISION_UPGRADE_BUILD_PLAN', FALSE);
  $plan = drush_get_context('PROVISION_UPGRADE_PLAN');
  if (drush_get_option('upgrade_build', FALSE) ? !is_array($pre) : !is_array($plan)) {
    return _provision_tasks_extra_upgrade_refuse('its checks did not run');
  }
  // Every process this run starts, each step's own included, says whose it
  // is: a later run tells by it that nothing of this one is still at work.
  putenv('BOA_UPGRADE_RUN=' . (int) (is_array($pre) ? $pre['task_nid'] : $plan['task_nid']));
  if (is_array($pre)) {
    // This process holds the site as it was before the move: nothing of it
    // is saved when the command ends.
    drush_set_option('provision_save_config', FALSE);
    $result = _provision_tasks_extra_upgrade_build_run($pre);
    if (drush_get_context('PROVISION_UPGRADE_MARKED', FALSE)) {
      _provision_tasks_extra_upgrade_mark_clear($pre['task_nid']);
    }
    return $result;
  }
  $result = _provision_tasks_extra_upgrade_run($plan);
  // The run ended by its own code, whichever way: its mark goes. Only a run
  // that never got here leaves one, and a run that wrote none (refused
  // before its first step, a task run again under the same number) leaves
  // an earlier run's mark as it is.
  if (drush_get_context('PROVISION_UPGRADE_MARKED', FALSE)) {
    _provision_tasks_extra_upgrade_mark_clear($plan['task_nid']);
  }
  return $result;
}

/**
 * The Upgrade itself, from the way back to the result.
 */
function _provision_tasks_extra_upgrade_run($plan) {
  $uri = $plan['uri'];
  $copy = $plan['copy'];
  $copy_dir = $plan['source_root'] . '/sites/' . $copy;
  // This process holds the site as it was before the move: nothing of it
  // is saved when the command ends.
  drush_set_option('provision_save_config', FALSE);
  // The name is free now, so whatever holds it after leg 1 is this run's.
  clearstatcache();
  if (_provision_tasks_extra_upgrade_alias_root($copy) !== FALSE || file_exists($copy_dir) || is_link($copy_dir)) {
    return _provision_tasks_extra_upgrade_refuse('the name of the way-back copy, ' . $copy . ', is taken', $plan);
  }
  $started = time();
  $when = date('Y-m-d H:i');
  $plan['started'] = $started;
  if (!_provision_tasks_extra_upgrade_mark($plan, '1')) {
    return _provision_tasks_extra_upgrade_refuse("this run's mark could not be written in the account's log folder", $plan);
  }
  foreach ($plan['stale_marks'] as $run) {
    _provision_tasks_extra_upgrade_mark_clear($run);
  }
  // Leg 0: what a run cut off before its move left.
  foreach ($plan['tidy'] as $item) {
    $removed = _provision_tasks_extra_upgrade_undo(array(
      'uri' => $uri,
      'copy' => $item['copy'],
      'source_root' => $plan['source_root'],
      'marker' => $plan['marker'],
      'recorded' => 'unknown',
    ));
    clearstatcache();
    $dir = $plan['source_root'] . '/sites/' . $item['copy'];
    if ($removed !== TRUE || _provision_tasks_extra_upgrade_alias_root($item['copy']) !== FALSE || file_exists($dir) || is_link($dir)) {
      return drush_set_error('PROVISION_UPGRADE_NOT_STARTED', dt('UPGRADE/RESULT !verdict: the copy @copy left by the upgrade task @run could not be removed (see above). @uri is on @source as before.!built', array(
        '!verdict' => empty($plan['built']) ? 'Not started' : 'Not upgraded',
        '@copy' => $item['copy'],
        '@run' => $item['run'],
        '@uri' => $uri,
        '@source' => $plan['source_title'],
        '!built' => _provision_tasks_extra_no_markup(_provision_tasks_extra_upgrade_built_kept($plan)),
      )));
    }
    _provision_tasks_extra_upgrade_mark_clear($item['run']);
    drush_log(dt('UPGRADE/0/TIDY removed @copy, left by the upgrade task @run that was cut off on @when before it moved the site.', array('@copy' => $item['copy'], '@run' => $item['run'], '@when' => $item['when'])), 'ok');
  }

  // Leg 1: the way back.
  drush_log(dt('UPGRADE/1/COPY making the way back: a full copy of @uri as @copy on @platform.', array('@uri' => $uri, '@copy' => $copy, '@platform' => $plan['source_title'])), 'ok');
  $leg = _provision_tasks_extra_upgrade_leg('@' . $uri, 'clone', array(
    'target_platform=' . $plan['source_nid'],
    'new_uri=' . $copy,
    'new_db_server=' . $plan['db_nid'],
    $plan['marker'],
  ));
  clearstatcache();
  // The panel records the copy when its Clone ends well, and only then.
  $plan['recorded'] = $leg['ok'];
  if (!$leg['ok'] || _provision_tasks_extra_upgrade_alias_root($copy) === FALSE || !is_file($copy_dir . '/settings.php')) {
    return _provision_tasks_extra_upgrade_stop($plan, _provision_tasks_extra_upgrade_step_why($leg, 'the copy of the site could not be made (see above)'), FALSE);
  }
  if (!_provision_tasks_extra_upgrade_mark($plan, '1a')) {
    return _provision_tasks_extra_upgrade_stop($plan, "this run's mark could not be written", FALSE);
  }
  // Leg 1a: the Import the Clone queued for the copy, run now. Left to the
  // queue it would run on the switched-off copy and mark it enabled.
  $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'import');
  if (!$leg['ok']) {
    return _provision_tasks_extra_upgrade_stop($plan, _provision_tasks_extra_upgrade_step_why($leg, 'the copy of the site could not be recorded in the panel (see above)'), FALSE);
  }
  if (!_provision_tasks_extra_upgrade_mark($plan, '1b')) {
    return _provision_tasks_extra_upgrade_stop($plan, "this run's mark could not be written", FALSE);
  }
  // Leg 1b.
  $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'disable', array($plan['marker']));
  if (!$leg['ok']) {
    return _provision_tasks_extra_upgrade_stop($plan, _provision_tasks_extra_upgrade_step_why($leg, 'the copy of the site could not be switched off (see above)'), FALSE);
  }
  drush_log(dt('UPGRADE/1/COPY done: @copy holds the site as it was at @when, switched off.', array('@copy' => $copy, '@when' => $when)), 'ok');

  // No move begins before its mark says so: a later run must never take
  // this copy for one a run made and left before its move.
  if (!_provision_tasks_extra_upgrade_mark($plan, '2')) {
    return _provision_tasks_extra_upgrade_stop($plan, "this run's mark could not be written", FALSE);
  }
  // Leg 2: the move, three tasks deep.
  drush_log(dt('UPGRADE/2/MOVE moving @uri onto @platform; Drupal\'s database updates run there.', array('@uri' => $uri, '@platform' => $plan['target_title'])), 'ok');
  $leg = _provision_tasks_extra_upgrade_leg('@' . $uri, 'migrate', array(
    'target_platform=' . $plan['target_nid'],
    'new_uri=' . $uri,
    'new_db_server=' . $plan['db_nid'],
    'upgrade_strict=1',
    $plan['marker'],
  ));
  clearstatcache();
  $root = _provision_tasks_extra_upgrade_alias_root($uri);
  $known = ($root !== FALSE && $root !== '');
  $on_target = $known && _provision_tasks_extra_upgrade_same_root($root, $plan['target_root']);
  $on_source = $known && _provision_tasks_extra_upgrade_same_root($root, $plan['source_root']);
  $way = ' The way back is ' . $copy . ' (Drupal ' . $plan['source_release'] . ', as it was at ' . $when . ').'
    . ($plan['return'] ? ' If something is wrong, press Go back on the site page.' : ' It is a full copy of the site, switched off.');

  if ($on_source) {
    return _provision_tasks_extra_upgrade_stop($plan, _provision_tasks_extra_upgrade_why($leg), TRUE);
  }
  if (!$on_target) {
    // Neither platform by the alias: nothing is removed.
    return drush_set_error('PROVISION_UPGRADE_UNFINISHED', dt('UPGRADE/RESULT Not finished: the move ended without saying which platform @uri is on (see above). Nothing was removed: the copy @copy is kept. Ask support.!built', array('@uri' => $uri, '@copy' => $copy, '!built' => _provision_tasks_extra_no_markup(_provision_tasks_extra_upgrade_built_kept($plan)))));
  }

  $kept = _provision_tasks_extra_upgrade_kept_dump($uri, $started);
  if ($kept !== '') {
    drush_log(dt('UPGRADE/KEPT the database as it was before the move is kept in @path, for support; the panel\'s Restore does not use it.', array('@path' => $kept)), 'ok');
  }
  // What the way back costs, for the site page: measured once the move has
  // gone through, so that the walk does not lengthen the time in which the
  // site's changes are lost on Go back; a switched-off copy does not change.
  $size = _provision_tasks_extra_upgrade_copy_size($copy_dir);
  if ($size !== '') {
    drush_log(dt('UPGRADE/1/COPY size: @copy uses !size.', array('@copy' => $copy, '!size' => $size)), 'ok');
  }
  $base = $uri . ' runs Drupal ' . $plan['target_release'] . ' on ' . $plan['target_title'];
  if (!empty($plan['added'])) {
    $base .= ', with ' . $plan['added'] . ', which it did not have on ' . $plan['source_title'];
  }
  $gone = !file_exists($plan['source_root'] . '/sites/' . $uri) && !is_link($plan['source_root'] . '/sites/' . $uri);
  $there = is_dir($plan['target_root'] . '/sites/' . $uri);
  // A move that failed while it retired the old copy is finished forward
  // by the Migrate itself, which says so: the site is served from the new
  // platform, whatever is left of the old folder.
  $forward = _provision_tasks_extra_upgrade_logged($leg, 'The source was retired when the migrate failed');
  if (!$there || (!$leg['ok'] && !$gone && !$forward)) {
    // The alias names the new platform and the old one still holds the
    // site, or the new one does not: the move stopped midway, and its own
    // undo did not run. Nothing is removed.
    return drush_set_error('PROVISION_UPGRADE_UNFINISHED', dt('UPGRADE/RESULT Not finished: the move of @uri onto @target stopped midway (see above), and the site may not be served. This task removed nothing: what @source holds of the site is as the move left it, and the copy @copy is kept. Ask support.!built', array(
      '@uri' => $uri,
      '@target' => $plan['target_title'],
      '@source' => $plan['source_title'],
      '@copy' => $copy,
      '!built' => _provision_tasks_extra_no_markup(_provision_tasks_extra_upgrade_built_kept($plan)),
    )));
  }
  if (!$leg['ok'] || !$gone) {
    // The move went through and its tidying did not: the site stays where
    // it is, and so does the way back.
    return drush_set_error('PROVISION_UPGRADE_UNFINISHED', dt('UPGRADE/RESULT Upgraded, last step unfinished: !base, but tidying @source failed (see above).!way!stays', array(
      '!base' => _provision_tasks_extra_no_markup($base),
      '@source' => $plan['source_title'],
      '!way' => _provision_tasks_extra_no_markup($way),
      '!stays' => _provision_tasks_extra_no_markup($plan['stays']),
    )));
  }

  $tail = _provision_tasks_extra_no_markup($way . $plan['stays']);
  if (!_provision_tasks_extra_upgrade_logged($leg, 'UPGRADE/UPDATEDB the database updates completed')) {
    // The move ran, and nothing shows that its two checks did.
    drush_log(dt('UPGRADE/RESULT Upgraded, not checked: !base, but the move did not say that the database updates completed or that the pages were read. Look at your site now.!tail', array('!base' => _provision_tasks_extra_no_markup($base), '!tail' => $tail)), 'warning');
    return;
  }
  if (_provision_tasks_extra_upgrade_logged($leg, 'UPGRADE/PAGES not checked') || !_provision_tasks_extra_upgrade_logged($leg, 'UPGRADE/PAGES ')) {
    drush_log(dt('UPGRADE/RESULT Upgraded, not page-checked: !base; the pages could not be read from this server (see above). Look at your site now.!tail', array('!base' => _provision_tasks_extra_no_markup($base), '!tail' => $tail)), 'warning');
    return;
  }
  $pages = '';
  if (_provision_tasks_extra_upgrade_logged($leg, 'UPGRADE/PAGES ', 'ok')) {
    $pages = _provision_tasks_extra_upgrade_logged($leg, 'left out of the check') ? '; its home page answers' : '; its home and login pages answer';
  }
  $warnings = _provision_tasks_extra_upgrade_warnings();
  if ($warnings) {
    $first = $warnings[0];
    if (strlen($first) > 240) {
      $first = substr($first, 0, 237) . '...';
    }
    $more = (count($warnings) > 1) ? ' (and ' . (count($warnings) - 1) . ' more, above)' : '';
    drush_log(dt('UPGRADE/RESULT Upgraded, with a note: !base!pages, and a step warned: "!first"!more.!tail', array(
      '!base' => _provision_tasks_extra_no_markup($base),
      '!pages' => $pages,
      '!first' => _provision_tasks_extra_no_markup($first),
      '!more' => $more,
      '!tail' => $tail,
    )), 'warning');
    return;
  }
  drush_log(dt('UPGRADE/RESULT Upgraded: !base!pages.!tail', array('!base' => _provision_tasks_extra_no_markup($base), '!pages' => $pages, '!tail' => $tail)), 'success');
}

/**
 * Refuses a Go back before anything is changed.
 *
 * @param array $where
 *   'stage', 'domain' and 'aside' when the stage was read: a refusal that
 *   finds the name free says the site is not being served.
 */
function _provision_tasks_extra_return_refuse($why, $where = array()) {
  $tail = '';
  if (isset($where['stage'], $where['domain'], $where['aside']) && $where['stage'] === 'aside') {
    $tail = ' ' . $where['domain'] . ' is still not being served: an earlier Go back set the upgraded site aside as ' . $where['aside'] . '. Press Go back again once this is cleared, or ask support.';
  }
  return drush_set_error('PROVISION_UPGRADE_RETURN_NOT_STARTED', dt('GOBACK/RESULT Not started: !why. Nothing was changed.!tail', array(
    '!why' => _provision_tasks_extra_no_markup($why),
    '!tail' => _provision_tasks_extra_no_markup($tail),
  )));
}

/**
 * The three names of a Go back, checked against each other.
 *
 * The copy is one an Upgrade of this site made and the upgraded site is kept
 * under a name of this task's own making: neither is whatever a caller
 * passes.
 *
 * @return string
 *   '' when they fit, or what is wrong, as a clause.
 */
function _provision_tasks_extra_return_names($domain, $copy, $aside) {
  if (!is_string($domain) || !preg_match('/^[a-z0-9][a-z0-9.-]*[a-z0-9]$/', $domain) || strpos($domain, '..') !== FALSE) {
    return 'the task names no site to go back with (return_domain)';
  }
  $tail = '.temp.' . $domain;
  foreach (array('back-' => $copy, 'upgraded-' => $aside) as $prefix => $name) {
    $ok = is_string($name) && strlen($name) > strlen($prefix) + strlen($tail)
      && strncmp($name, $prefix, strlen($prefix)) === 0 && substr($name, -strlen($tail)) === $tail;
    if ($ok) {
      $number = substr($name, strlen($prefix), strlen($name) - strlen($prefix) - strlen($tail));
      $ok = ctype_digit($number) && (int) $number > 0;
    }
    if (!$ok) {
      return ($prefix === 'back-')
        ? 'the name given for the copy is not one an Upgrade of ' . $domain . ' makes'
        : 'the name given for the upgraded site is not one a Go back of ' . $domain . ' makes';
    }
  }
  return '';
}

/**
 * How far a Go back has come, read from where three aliases point.
 *
 * Each root is what _provision_tasks_extra_upgrade_alias_root() gave: FALSE
 * for no alias, '' for an alias that names no root, else a docroot.
 *
 * @return string
 *   'start' (nothing done: the site on the new platform, its copy on the old
 *   one), 'aside' (the upgraded site is set aside and the name is free),
 *   'back' (the copy holds the name), or '' for anything else.
 */
function _provision_tasks_extra_return_stage($domain_root, $copy_root, $aside_root, $new_root, $old_root) {
  $domain_new = is_string($domain_root) && $domain_root !== '' && _provision_tasks_extra_upgrade_same_root($domain_root, $new_root);
  $domain_old = is_string($domain_root) && $domain_root !== '' && _provision_tasks_extra_upgrade_same_root($domain_root, $old_root);
  $copy_old = is_string($copy_root) && $copy_root !== '' && _provision_tasks_extra_upgrade_same_root($copy_root, $old_root);
  $aside_new = is_string($aside_root) && $aside_root !== '' && _provision_tasks_extra_upgrade_same_root($aside_root, $new_root);
  if ($domain_new && $copy_old && $aside_root === FALSE) {
    return 'start';
  }
  if ($domain_root === FALSE && $copy_old && $aside_new) {
    return 'aside';
  }
  if ($domain_old && $copy_root === FALSE && $aside_new) {
    return 'back';
  }
  return '';
}

/**
 * Whether a first rename that failed left the name with the upgraded site.
 *
 * A rename whose deploy came through and whose later step failed keeps the
 * new name's context and puts the site back under its own (provision's
 * keep_new_context outcome): the name points at the new platform again, the
 * copy at the old one, and the name the site was to take has an alias too.
 * Roots as for _provision_tasks_extra_return_stage().
 */
function _provision_tasks_extra_return_left_aside($domain_root, $copy_root, $aside_root, $new_root, $old_root) {
  return is_string($domain_root) && $domain_root !== '' && _provision_tasks_extra_upgrade_same_root($domain_root, $new_root)
    && is_string($copy_root) && $copy_root !== '' && _provision_tasks_extra_upgrade_same_root($copy_root, $old_root)
    && $aside_root !== FALSE;
}

/**
 * What a run does with the copy's rename, by the stage it starts at.
 *
 * @param string $stage
 *   As _provision_tasks_extra_return_stage() gives it.
 * @param string $copy_fault
 *   Why the copy cannot be served, '' when it can.
 *
 * @return string
 *   'rename' (the run renames the copy, and its checks count it), 'skip'
 *   (the copy cannot be served: the run renames the upgraded site back
 *   instead, and nothing of the copy is counted) or 'none' (the copy holds
 *   the name already). A first run with a copy fault is refused before
 *   this is asked.
 */
function _provision_tasks_extra_return_copy_step($stage, $copy_fault) {
  if ($stage === 'back') {
    return 'none';
  }
  return ($stage === 'aside' && $copy_fault !== '') ? 'skip' : 'rename';
}

/**
 * Where the three names of a Go back point on disk now.
 *
 * @return array
 *   'domain', 'copy' and 'aside', each as _provision_tasks_extra_upgrade_alias_root()
 *   gives it.
 */
function _provision_tasks_extra_return_roots($plan) {
  clearstatcache();
  return array(
    'domain' => _provision_tasks_extra_upgrade_alias_root($plan['domain']),
    'copy' => _provision_tasks_extra_upgrade_alias_root($plan['copy']),
    'aside' => _provision_tasks_extra_upgrade_alias_root($plan['aside']),
  );
}

/**
 * The stage of a Go back as the disk shows it now.
 */
function _provision_tasks_extra_return_where($plan) {
  $roots = _provision_tasks_extra_return_roots($plan);
  return _provision_tasks_extra_return_stage($roots['domain'], $roots['copy'], $roots['aside'], $plan['new_root'], $plan['old_root']);
}

/**
 * The first filesystem that is too full for a Go back, or NULL.
 *
 * @param array $devices
 *   One array a filesystem: 'path', and 'need', 'free' and 'margin' in bytes;
 *   'inodes', 'inodes_free' and 'inodes_margin' where its files were counted
 *   ('inodes_free' FALSE where the filesystem keeps no count).
 *
 * @return array|null
 *   That filesystem, with 'short' set to 'space' or 'inodes'.
 */
function _provision_tasks_extra_return_space_short($devices) {
  foreach ($devices as $device) {
    if ($device['free'] < $device['need'] + $device['margin']) {
      $device['short'] = 'space';
      return $device;
    }
  }
  foreach ($devices as $device) {
    if (!empty($device['inodes']) && isset($device['inodes_free']) && $device['inodes_free'] !== FALSE
      && $device['inodes_free'] < $device['inodes'] + $device['inodes_margin']) {
      $device['short'] = 'inodes';
      return $device;
    }
  }
  return NULL;
}

/**
 * What one rename of a Go back writes, place by place.
 *
 * As provision's own check counts a site renamed on its own platform
 * (provision_copy_task_space_preflight(), its migrate term): a safety copy
 * with the database dump, the site unpacked under its new name, and the
 * renamed site's own copy of a linked store. A safety copy follows links
 * only where the server says so, and then the store is copied apart only
 * when it lives on another filesystem. The cache folders no archive carries
 * are left out of every copy but the store's own.
 *
 * @param array $m
 *   The site as measured: 'full' and 'local' (bytes following links and
 *   not), 'excluded' (bytes of its cache folders), 'full_inodes',
 *   'local_inodes' and 'excluded_inodes' (the same in files; FALSE for a
 *   count that was not made), 'database' (bytes), 'linked' (its files or
 *   private folder is a link into the store), 'store_apart' (the store is on
 *   another filesystem than the platform), and the places 'backups', 'root'
 *   and 'store'.
 * @param bool $dereference
 *   Whether a safety copy follows links on this server.
 *
 * @return array
 *   One array a place: path, bytes, inodes.
 */
function _provision_tasks_extra_return_rename_needs($m, $dereference) {
  $full = (float) $m['full'];
  $excluded = (float) $m['excluded'];
  if (!($excluded > 0 && $excluded < $full)) {
    $excluded = 0.0;
  }
  $copy = $full - $excluded;
  $local = (float) $m['local'];
  $copy_inodes = 0.0;
  $local_inodes = 0.0;
  $excluded_inodes = 0.0;
  if ($m['full_inodes'] !== FALSE) {
    $copy_inodes = (float) $m['full_inodes'];
    if ($m['excluded_inodes'] !== FALSE && $m['excluded_inodes'] > 0 && $m['excluded_inodes'] < $copy_inodes) {
      $excluded_inodes = (float) $m['excluded_inodes'];
    }
    $copy_inodes -= $excluded_inodes;
    $local_inodes = ($m['local_inodes'] === FALSE) ? $copy_inodes : (float) $m['local_inodes'];
  }
  $archive = $dereference ? $copy : $local;
  $needs = array(
    array($m['backups'], $archive + (float) $m['database'], 0.0),
    array($m['root'], $archive, $dereference ? $copy_inodes : $local_inodes),
  );
  if ($m['linked'] && ($dereference ? $m['store_apart'] : TRUE)) {
    if ($dereference) {
      $bytes = $copy - $local;
      $inodes = max(0.0, $copy_inodes - $local_inodes);
    }
    else {
      $bytes = $copy + $excluded - $local;
      $inodes = max(0.0, $copy_inodes + $excluded_inodes - $local_inodes);
    }
    if ($bytes > 0) {
      $needs[] = array($m['store'], $bytes, $inodes);
    }
  }
  return $needs;
}

/**
 * What the renames of a Go back need, summed per filesystem.
 *
 * Each rename takes a safety copy of the site's own files and its database,
 * unpacks the site under its new name and gives it its own copy of the store
 * of uploaded files, while the store at the old name is set aside and kept
 * (_provision_tasks_extra_return_rename_needs()).
 *
 * @param array $sites
 *   One array a site that will be renamed: 'path' (its folder) and 'root'
 *   (its platform's docroot).
 *
 * @return array|false
 *   Filesystems as _provision_tasks_extra_return_space_short() takes them;
 *   FALSE when a size could not be read (each rename still checks its own).
 */
function _provision_tasks_extra_return_space($sites) {
  foreach (array('provision_site_bytes', 'provision_site_inodes', 'provision_site_excluded_bytes', 'provision_site_excluded_inodes', 'provision_site_database_bytes', 'provision_path_device', 'provision_path_probe', 'provision_path_inodes', 'provision_space_margin', 'provision_inode_margin') as $function) {
    if (!function_exists($function)) {
      return FALSE;
    }
  }
  $server = d('@server_master');
  if (!is_object($server) || empty($server->backup_path) || empty($server->aegir_root)) {
    return FALSE;
  }
  $store = $server->aegir_root . '/static/files';
  $dereference = is_file('/data/conf/dereference_backups.conf') && !is_file('/data/conf/force_symlinks.conf');
  $database = (float) provision_site_database_bytes();
  $needs = array();
  foreach ($sites as $one) {
    $full = provision_site_bytes($one['path'], TRUE);
    $local = provision_site_bytes($one['path'], FALSE);
    if ($full === FALSE || $local === FALSE) {
      return FALSE;
    }
    $full_inodes = provision_site_inodes($one['path'], TRUE);
    $measured = array(
      'full' => $full,
      'local' => $local,
      'excluded' => (float) provision_site_excluded_bytes($one['path']),
      'full_inodes' => $full_inodes,
      'local_inodes' => ($full_inodes === FALSE) ? FALSE : provision_site_inodes($one['path'], FALSE),
      'excluded_inodes' => ($full_inodes === FALSE) ? FALSE : provision_site_excluded_inodes($one['path']),
      'database' => $database,
      'linked' => is_link($one['path'] . '/files') || is_link($one['path'] . '/private'),
      'store_apart' => provision_path_device($store) !== provision_path_device($one['root']),
      'backups' => $server->backup_path,
      'root' => $one['root'],
      'store' => $store,
    );
    $needs = array_merge($needs, _provision_tasks_extra_return_rename_needs($measured, $dereference));
  }
  $devices = array();
  foreach ($needs as $need) {
    $probe = provision_path_probe($need[0]);
    $device = provision_path_device($need[0]);
    if ($probe === FALSE || $device === FALSE) {
      return FALSE;
    }
    if (!isset($devices[$device])) {
      $free = @disk_free_space($probe);
      if ($free === FALSE) {
        return FALSE;
      }
      $in = provision_path_inodes($probe);
      $devices[$device] = array(
        'path' => $probe,
        'need' => 0.0,
        'free' => (float) $free,
        'margin' => (float) provision_space_margin($probe),
        'inodes' => 0.0,
        'inodes_free' => ($in === FALSE) ? FALSE : (float) $in['free'],
        'inodes_margin' => ($in === FALSE) ? 0.0 : (float) provision_inode_margin($in['total']),
      );
    }
    $devices[$device]['need'] += $need[1];
    $devices[$device]['inodes'] += $need[2];
  }
  return array_values($devices);
}

/**
 * Whether the site's database server answers, asked without raising an error.
 *
 * The room check reads the database's size through the server's own
 * connection, and a server that does not answer sets the connection error
 * there: the run would then stop with no line of its own.
 */
function _provision_tasks_extra_return_db_answers() {
  $db = d()->service('db');
  if (!is_object($db) || !method_exists($db, 'connect') || !property_exists($db, 'conn')) {
    return TRUE;
  }
  if (!empty($db->conn)) {
    return TRUE;
  }
  return (bool) $db->connect(TRUE);
}

/**
 * Why a rename of a Go back failed, as a clause: $what is the site renamed,
 * $target the name it was to take, where a rename checks for a store.
 */
function _provision_tasks_extra_return_why($leg, $what, $target) {
  if (in_array('PROVISION_UPGRADE_PANEL_MOVING', $leg['codes'], TRUE)) {
    return 'the control panel is moving to its new version, so the panel could not start the step for ' . $what . '; start again in a few minutes';
  }
  if (in_array('PROVISION_SITE_STORE_AT_NAME', $leg['codes'], TRUE)) {
    return 'a store of uploaded files is still at the name ' . $target . ' and could not be set aside (see above)';
  }
  if (in_array('PROVISION_INSUFFICIENT_SPACE', $leg['codes'], TRUE) || in_array('PROVISION_INSUFFICIENT_INODES', $leg['codes'], TRUE)) {
    return 'the disk was too full to rename ' . $what . ' (see above)';
  }
  if (in_array('HOSTING_TASK_LOAD_FAILED', $leg['codes'], TRUE) || in_array('HOSTING_INVALID_TASK', $leg['codes'], TRUE)) {
    return 'the panel could not start the step for ' . $what . ' (see above)';
  }
  return 'the rename of ' . $what . ' did not go through (see above)';
}

/**
 * Checks a Go back before anything is changed.
 *
 * @return array|string
 *   The plan of the run, or the reason it is refused, a clause with no full
 *   stop.
 */
function _provision_tasks_extra_return_plan() {
  $site = d();
  if (!is_object($site) || $site->type !== 'site' || empty($site->uri)) {
    return 'this is not a site';
  }
  $uri = strtolower(ltrim((string) $site->uri, '@'));
  $opt = array();
  foreach (array('return_domain', 'return_copy', 'return_aside', 'return_new', 'return_new_nid', 'return_old', 'return_old_nid', 'return_new_db_nid', 'return_old_db_nid', 'return_task_nid') as $name) {
    $opt[$name] = trim((string) drush_get_option($name, ''));
    if ($opt[$name] === '') {
      return 'the task did not say what to put back (' . $name . ' is missing); start Go back from the site\'s page';
    }
  }
  foreach (array('return_new_nid', 'return_old_nid', 'return_new_db_nid', 'return_old_db_nid', 'return_task_nid') as $name) {
    if (!ctype_digit($opt[$name]) || (int) $opt[$name] < 1) {
      return 'the task carries a value that is no id (' . $name . '); start Go back from the site\'s page';
    }
  }
  $domain = strtolower($opt['return_domain']);
  $copy = strtolower($opt['return_copy']);
  $aside = strtolower($opt['return_aside']);
  $wrong = _provision_tasks_extra_return_names($domain, $copy, $aside);
  if ($wrong !== '') {
    return $wrong;
  }
  // The task runs on the site itself, or, when an earlier run stopped
  // midway, on the same site under the name it was set aside as.
  if ($uri !== $domain && $uri !== $aside) {
    return 'this task was started on ' . $uri . ', which is neither ' . $domain . ' nor the name it is set aside as';
  }
  if (function_exists('provision_is_hostmaster_site') && provision_is_hostmaster_site()) {
    return 'this is the control panel\'s own site';
  }
  $new_title = trim((string) drush_get_option('return_new_title', ''));
  $old_title = trim((string) drush_get_option('return_old_title', ''));
  $new_title = ($new_title === '') ? 'the platform it was upgraded onto' : $new_title;
  $old_title = ($old_title === '') ? 'the platform it ran on before' : $old_title;
  $new = d($opt['return_new'], FALSE, FALSE);
  $old = d($opt['return_old'], FALSE, FALSE);
  if (!is_object($new) || $new->type !== 'platform' || empty($new->root)) {
    return $new_title . ' is not known to the backend as a platform; verify it first';
  }
  if (!is_object($old) || $old->type !== 'platform' || empty($old->root)) {
    return $old_title . ' is not known to the backend as a platform; verify it first';
  }
  $new_root = _provision_tasks_extra_docroot($new->root);
  $old_root = _provision_tasks_extra_docroot($old->root);
  if (_provision_tasks_extra_upgrade_same_root($new_root, $old_root)) {
    return 'the copy is on the platform the site runs on, so there is no older version to go back to';
  }
  $plan = array(
    'domain' => $domain,
    'copy' => $copy,
    'aside' => $aside,
    'marker' => 'upgrade_run=' . $opt['return_task_nid'],
    'new_nid' => $opt['return_new_nid'],
    'old_nid' => $opt['return_old_nid'],
    'new_db_nid' => $opt['return_new_db_nid'],
    'old_db_nid' => $opt['return_old_db_nid'],
    'new_root' => $new_root,
    'old_root' => $old_root,
    'new_title' => $new_title,
    'old_title' => $old_title,
    'new_release' => _provision_tasks_extra_core_release($new_root),
    'old_release' => _provision_tasks_extra_core_release($old_root),
    'when' => preg_replace('/[^0-9: -]/', '', (string) drush_get_option('return_when', '')),
    'aside_on' => (bool) drush_get_option('return_aside_on', FALSE),
    'copy_on' => (bool) drush_get_option('return_copy_on', FALSE),
    'copy_was_on' => (bool) drush_get_option('return_copy_was_on', FALSE),
    'no_copy' => '',
  );
  $server = d('@server_master');
  $home = (is_object($server) && !empty($server->aegir_root)) ? rtrim($server->aegir_root, '/') : '';
  $plan['store'] = ($home === '') ? '' : $home . '/static/files/' . $domain;
  $plan['share'] = $home . '/static/control/share.files.' . $domain . '.info';

  $stage = _provision_tasks_extra_return_where($plan);
  if ($stage === '') {
    return 'where ' . $domain . ' and its copy are now does not fit a Go back: the site must be on ' . $new_title . ' with its copy ' . $copy . ' on ' . $old_title . ', or an earlier Go back must have set it aside as ' . $aside;
  }
  $plan['stage'] = $stage;
  // A refusal from here on can find the name already free (an earlier run
  // set the upgraded site aside): the refusal says so.
  drush_set_context('PROVISION_UPGRADE_RETURN_STAGE', array('stage' => $stage, 'domain' => $domain, 'aside' => $aside));
  // Both concern the copy alone. Once an earlier run has set the upgraded
  // site aside, the run that renames it back needs neither of them.
  $copy_fault = '';
  if (!is_file($old_root . '/core/lib/Drupal.php')) {
    $copy_fault = $old_title . ' holds no Drupal code any more, so the copy cannot be served from it';
  }
  elseif (!is_file($old_root . '/sites/' . $copy . '/settings.php')) {
    $copy_fault = 'the copy ' . $copy . ' is not complete on ' . $old_title . ' (it has no settings.php)';
  }

  // The queue, as for an Upgrade.
  if (file_exists('/run/boa_queue_stop.pid')) {
    return 'the task queue is paused on this server for maintenance; start again later';
  }
  if (file_exists('/run/octopus_install_run.pid')) {
    return 'a system upgrade of this account is running; start again when it has ended';
  }

  $renames = array();
  if ($stage === 'start') {
    if ($copy_fault !== '') {
      return $copy_fault;
    }
    // The second rename hands the site's name to the copy, and a rename
    // refuses a name whose store of uploaded files stays in place. Each of
    // these keeps it in place, so the run would stop with the site down.
    if ($home !== '' && is_file($plan['share'])) {
      return 'the uploaded files of ' . $domain . ' are shared with another site (the file static/control/share.files.' . $domain . '.info names their store), so the name cannot be handed to the copy. Remove that file when no site uses the store any more, then press Go back again';
    }
    if (is_file('/data/conf/disable_orphan_store_archiving.cnf')) {
      return 'this server keeps the stores of uploaded files in place (/data/conf/disable_orphan_store_archiving.cnf), so the name of the site cannot be handed to its copy. Ask support';
    }
    $stray = $old_root . '/sites/' . $domain;
    if (file_exists($stray) || is_link($stray)) {
      return 'a leftover of this site is in the way on ' . $old_title . ' (' . $stray . '). Delete it, then press Go back again';
    }
    $taken = $new_root . '/sites/' . $aside;
    if (file_exists($taken) || is_link($taken)) {
      return 'the name the upgraded site would be kept under, ' . $aside . ', is taken on ' . $new_title . ' (' . $taken . ')';
    }
    $renames[] = array('path' => $new_root . '/sites/' . $domain, 'root' => $new_root);
  }
  $copy_step = _provision_tasks_extra_return_copy_step($stage, $copy_fault);
  if ($copy_step === 'skip') {
    // The copy's rename is not tried: the upgraded site is renamed back
    // (step 3), with the checks of its own rename.
    $plan['no_copy'] = $copy_fault;
  }
  elseif ($copy_step === 'rename') {
    $renames[] = array('path' => $old_root . '/sites/' . $copy, 'root' => $old_root);
  }
  // Put back already, the copy holds the name on that platform: a run that
  // only finishes it must not say it serves the site from a platform with
  // no code.
  if ($stage === 'back' && !is_file($old_root . '/core/lib/Drupal.php')) {
    return 'an earlier Go back handed the name ' . $domain . ' to the copy on ' . $old_title . ', which holds no Drupal code any more, so ' . $domain . ' cannot be served from it. Ask support';
  }

  if ($renames) {
    // How long: each rename is about one copy of the site, and a rename
    // that fails is followed by one more, back.
    $secs = (int) drush_get_option('return_copy_secs', 0);
    $ceiling = (int) drush_get_option('return_ceiling', 0);
    if ($secs > 0 && $ceiling > 0) {
      if ((count($renames) + 1) * $secs + 1800 > $ceiling) {
        return 'the last copy of this site took ' . (int) ceil($secs / 60) . ' minutes, and a Go back can need ' . (count($renames) + 1) . ' times that and half an hour: more than the ' . round($ceiling / 3600, 1) . ' hours one task may run. Ask support';
      }
    }
    else {
      drush_log(dt('GOBACK/CHECK no earlier Backup or Clone of this site says how long a copy takes; the time check is skipped.'), 'ok');
    }
    // How much room, for all of it at once: a rename that is refused for
    // space after the first went through would leave the site down.
    if (is_file('/data/conf/disable_space_preflight.cnf')) {
      drush_log(dt('GOBACK/CHECK the space check is switched off on this server (/data/conf/disable_space_preflight.cnf).'), 'ok');
    }
    // The room check reads the size of the database through its server; one
    // that does not answer would stop the run there, with no line of its own.
    elseif (!_provision_tasks_extra_return_db_answers()) {
      return 'the database server of ' . $domain . ' does not answer; start again when it does';
    }
    else {
      $devices = _provision_tasks_extra_return_space($renames);
      if ($devices === FALSE) {
        drush_log(dt('GOBACK/CHECK the room a Go back needs could not be measured here; each step checks its own.'), 'ok');
      }
      else {
        $short = _provision_tasks_extra_return_space_short($devices);
        if ($short !== NULL && $short['short'] === 'inodes') {
          return 'Go back needs about ' . number_format($short['inodes'] + $short['inodes_margin'], 0, '.', ',') . ' free inodes on the filesystem holding ' . $short['path'] . ' (every file of the sites under their new names takes one; ' . number_format($short['inodes_margin'], 0, '.', ',') . ' of them headroom), and ' . number_format($short['inodes_free'], 0, '.', ',') . ' are free. Free inodes first';
        }
        if ($short !== NULL) {
          // in a unit that tells the need from the free space (provision's
          // provision_space_figures()), in gigabytes before provision has it
          $gb = 1073741824;
          $fig = function_exists('provision_space_figures') ? provision_space_figures($short['need'] + $short['margin'], $short['margin'], $short['free']) : array(
            '@need' => round(($short['need'] + $short['margin']) / $gb, 1),
            '@margin' => round($short['margin'] / $gb, 1),
            '@free' => round($short['free'] / $gb, 1),
            '@unit' => 'GB',
          );
          return 'Go back needs about ' . $fig['@need'] . ' ' . $fig['@unit'] . ' free on the filesystem holding ' . $short['path'] . ' (the safety copies, the two sites under their new names and their own copies of the uploaded files; ' . $fig['@margin'] . ' ' . $fig['@unit'] . ' of it headroom), and ' . $fig['@free'] . ' ' . $fig['@unit'] . ' is free. Free space first';
        }
      }
    }
  }

  if ($plan['no_copy'] !== '') {
    drush_log(dt('GOBACK/PLAN @domain: an earlier run set the upgraded site aside as @aside, and the copy @copy cannot be put back (!why): the upgraded site (Drupal @new on @new_title) is renamed back to @domain.', array(
      '@domain' => $domain,
      '@aside' => $aside,
      '@copy' => $copy,
      '!why' => _provision_tasks_extra_no_markup($plan['no_copy']),
      '@new' => $plan['new_release'],
      '@new_title' => $new_title,
    )), 'warning');
    return $plan;
  }
  drush_log(dt('GOBACK/PLAN @domain: the copy @copy (Drupal @old on @old_title!when) is put back, and the upgraded site (Drupal @new on @new_title) is kept, switched off, as @aside.!resumed', array(
    '@domain' => $domain,
    '@copy' => $copy,
    '@old' => $plan['old_release'],
    '@old_title' => $old_title,
    '!when' => ($plan['when'] === '') ? '' : ', as it was at ' . $plan['when'],
    '@new' => $plan['new_release'],
    '@new_title' => $new_title,
    '@aside' => $aside,
    '!resumed' => ($stage === 'start') ? '' : (($stage === 'aside') ? ' An earlier run set the upgraded site aside already: this run goes on from there.' : ' An earlier run put the copy back already: this run finishes it.'),
  )), 'ok');
  return $plan;
}

/**
 * Implements drush_hook_COMMAND_validate() for provision-drupal-upgrade-return.
 */
function drush_provision_tasks_extra_provision_drupal_upgrade_return_validate() {
  $plan = _provision_tasks_extra_return_plan();
  if (!is_array($plan)) {
    $where = drush_get_context('PROVISION_UPGRADE_RETURN_STAGE', array());
    return _provision_tasks_extra_return_refuse($plan, $where);
  }
  drush_set_context('PROVISION_UPGRADE_RETURN_PLAN', $plan);
}

/**
 * Ends a Go back that left neither version under the site's name.
 */
function _provision_tasks_extra_return_unfinished($plan, $why, $tail = '') {
  return drush_set_error('PROVISION_UPGRADE_RETURN_UNFINISHED', dt('GOBACK/RESULT Not finished: !why@domain is not being served right now. Both versions are intact (@aside and @copy).!tail Press Go back again to finish, or ask support.', array(
    '!why' => ($why === '') ? '' : _provision_tasks_extra_no_markup($why) . '. ',
    '@domain' => $plan['domain'],
    '@aside' => $plan['aside'],
    '@copy' => $plan['copy'],
    '!tail' => _provision_tasks_extra_no_markup($tail),
  )));
}

/**
 * Ends a Go back whose steps left a picture no later run can finish.
 *
 * A rename whose deploy came through and whose later step failed keeps both
 * names (provision's keep_new_context outcome): the stage is then none of
 * start, aside and back, and a later run would be refused before anything
 * is changed. Whether the site's name is served then cannot be read here.
 */
function _provision_tasks_extra_return_stuck($plan, $why, $tail = '') {
  $roots = _provision_tasks_extra_return_roots($plan);
  return drush_set_error('PROVISION_UPGRADE_RETURN_STUCK', dt('GOBACK/RESULT Not finished: !why@domain !served: a step failed after it had made a site under its new name, and the names left in place are not ones Go back can finish from (see above). Both versions are kept (@aside and @copy).!tail Ask support.', array(
    '!why' => ($why === '') ? '' : _provision_tasks_extra_no_markup($why) . '. ',
    '@domain' => $plan['domain'],
    '!served' => ($roots['domain'] === FALSE) ? 'is not being served right now' : 'may not be served as it should right now',
    '@aside' => $plan['aside'],
    '@copy' => $plan['copy'],
    '!tail' => _provision_tasks_extra_no_markup($tail),
  )));
}

/**
 * Implements the provision-drupal-upgrade-return command.
 *
 * Existing tasks as real task rows inside this one. The upgraded site is
 * renamed aside and switched off; the copy the Upgrade kept is switched on
 * and renamed to the site's name. When the copy cannot take the name, the
 * upgraded site is renamed back. Every step is judged by where the three
 * aliases point afterwards, so a run that was cut can be finished by running
 * the task again: a step whose work is done is not run twice.
 */
function drush_provision_tasks_extra_provision_drupal_upgrade_return() {
  $plan = drush_get_context('PROVISION_UPGRADE_RETURN_PLAN');
  if (!is_array($plan)) {
    return _provision_tasks_extra_return_refuse('its checks did not run');
  }
  $domain = $plan['domain'];
  $copy = $plan['copy'];
  $aside = $plan['aside'];
  // This process holds the site as it was before the renames: nothing of it
  // is saved when the command ends.
  drush_set_option('provision_save_config', FALSE);
  $stage = $plan['stage'];
  $fresh = ($stage === 'start');
  $notes = array();
  $before = $domain . ' runs Drupal ' . $plan['new_release'] . ' on ' . $plan['new_title'];

  // Step 1: the upgraded site out of the way, under a name of its own. A
  // rename that failed and still moved the site is one the run gets past,
  // so nothing of it is taken in until its outcome is read.
  if ($stage === 'start') {
    drush_log(dt('GOBACK/1 setting the upgraded site aside: @domain is renamed to @aside on @platform.', array('@domain' => $domain, '@aside' => $aside, '@platform' => $plan['new_title'])), 'ok');
    $leg = _provision_tasks_extra_upgrade_leg('@' . $domain, 'migrate', array(
      'target_platform=' . $plan['new_nid'],
      'new_uri=' . $aside,
      'new_db_server=' . $plan['new_db_nid'],
      $plan['marker'],
      'return_step=1',
    ), TRUE);
    $roots = _provision_tasks_extra_return_roots($plan);
    $stage = _provision_tasks_extra_return_stage($roots['domain'], $roots['copy'], $roots['aside'], $plan['new_root'], $plan['old_root']);
    _provision_tasks_extra_upgrade_replay($leg, $stage === 'aside');
    if ($stage === 'start') {
      return drush_set_error('PROVISION_UPGRADE_RETURN_NOT_GONE', dt('GOBACK/RESULT Not gone back: !why. !before as before, and the copy @copy is kept.', array(
        '!why' => _provision_tasks_extra_no_markup(_provision_tasks_extra_return_why($leg, $domain, $aside)),
        '!before' => _provision_tasks_extra_no_markup($before),
        '@copy' => $copy,
      )));
    }
    if ($stage !== 'aside') {
      if (_provision_tasks_extra_return_left_aside($roots['domain'], $roots['copy'], $roots['aside'], $plan['new_root'], $plan['old_root'])) {
        // The rename's own undo put the upgraded site back under its name
        // and kept what it had made under the new one.
        return drush_set_error('PROVISION_UPGRADE_RETURN_NOT_GONE', dt('GOBACK/RESULT Not gone back: !why. !before as before, and the copy @copy is kept. The failed step left a second copy of the upgraded site under the name @aside: ask support to remove it.', array(
          '!why' => _provision_tasks_extra_no_markup(_provision_tasks_extra_return_why($leg, $domain, $aside)),
          '!before' => _provision_tasks_extra_no_markup($before),
          '@copy' => $copy,
          '@aside' => $aside,
        )));
      }
      return _provision_tasks_extra_return_stuck($plan, $leg['ok'] ? 'the first step ended without saying where ' . $domain . ' is' : _provision_tasks_extra_return_why($leg, $domain, $aside));
    }
    if (!$leg['ok']) {
      $notes[] = 'the step that set it aside reported a failure (see above)';
    }
    drush_log(dt('GOBACK/1 done: the upgraded site is @aside.', array('@aside' => $aside)), 'ok');
  }

  // Step 1b: switched off. A rename brings a site out switched on. The name
  // is free by now, so a failure here stops nothing.
  if ($fresh || $plan['aside_on']) {
    $leg = _provision_tasks_extra_upgrade_leg('@' . $aside, 'disable', array($plan['marker'], 'return_step=1b'), TRUE);
    _provision_tasks_extra_upgrade_replay($leg, TRUE);
    if (!$leg['ok']) {
      $notes[] = 'the upgraded site ' . $aside . ' could not be switched off (see above); disable it in the panel';
      drush_log(dt('GOBACK/1 the upgraded site @aside could not be switched off; the run goes on.', array('@aside' => $aside)), 'warning');
    }
  }

  $why = '';
  $copy_enabled = FALSE;
  if ($stage === 'aside') {
    // Step 2: the copy takes the name. The first rename set the store of
    // uploaded files at that name aside; while it is still there the copy's
    // rename is refused, so it is not tried.
    clearstatcache();
    $left = ($plan['store'] !== '' && (is_dir($plan['store']) || is_link($plan['store'])));
    if ($plan['no_copy'] !== '') {
      // Found by the checks: the copy cannot be served, so it does not take
      // the name.
      $why = $plan['no_copy'];
      drush_log(dt('GOBACK/2 not tried: !why.', array('!why' => _provision_tasks_extra_no_markup($why))), 'warning');
    }
    // Left by the rename this run just made, the move was declined a moment
    // ago. Found by a later run, the copy's own rename tries that move
    // again, unless what keeps the store in place is plain to see.
    elseif ($left && ($fresh || is_file($plan['share']) || is_file('/data/conf/disable_orphan_store_archiving.cnf'))) {
      $why = 'the uploaded files of the upgraded site are still at ~/static/files/' . $domain . ' and could not be set aside, so the copy cannot take the name';
      drush_log(dt('GOBACK/2 not tried: a store of uploaded files is still at @store.', array('@store' => $plan['store'])), 'warning');
    }
    else {
      drush_log(dt('GOBACK/2 putting the copy back: @copy is switched on and renamed to @domain on @platform.', array('@copy' => $copy, '@domain' => $domain, '@platform' => $plan['old_title'])), 'ok');
      $leg = array('ok' => TRUE, 'codes' => array(), 'errors' => array(), 'log' => array());
      if (!$plan['copy_on']) {
        $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'enable', array($plan['marker'], 'return_step=2a'));
        $copy_enabled = $leg['ok'];
        if (!$leg['ok']) {
          $why = 'the copy ' . $copy . ' could not be switched on (see above)';
        }
      }
      if ($leg['ok']) {
        $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'migrate', array(
          'target_platform=' . $plan['old_nid'],
          'new_uri=' . $domain,
          'new_db_server=' . $plan['old_db_nid'],
          $plan['marker'],
          'return_step=2',
        ));
        $stage = _provision_tasks_extra_return_where($plan);
        if ($stage === 'back' && !$leg['ok']) {
          // The copy holds the name and its rename still reported a
          // failure: the panel's record of it may not have followed.
          return drush_set_error('PROVISION_UPGRADE_RETURN_UNFINISHED', dt('GOBACK/RESULT Gone back, last step unfinished: @domain is served from @platform again, but the step that renamed the copy reported a failure (see above), and the panel may still list it as @copy. Ask support.', array(
            '@domain' => $domain,
            '@platform' => $plan['old_title'],
            '@copy' => $copy,
          )));
        }
        if ($stage !== 'back') {
          $why = _provision_tasks_extra_return_why($leg, $copy, $domain);
        }
        if ($stage !== 'back' && $stage !== 'aside') {
          // The copy's rename kept both names: renaming the upgraded site
          // back now would take a name that is held.
          return _provision_tasks_extra_return_stuck($plan, $why, ($copy_enabled || $plan['copy_on']) ? ' The copy ' . $copy . ' is switched on.' : '');
        }
      }
    }
  }

  if ($stage === 'aside') {
    // Step 3: the name is free and the copy did not take it. The upgraded
    // site goes back under it, so the site is served as before Go back.
    drush_log(dt('GOBACK/3 the copy was not put back: the upgraded site @aside is renamed back to @domain.', array('@aside' => $aside, '@domain' => $domain)), 'ok');
    $leg = _provision_tasks_extra_upgrade_leg('@' . $aside, 'migrate', array(
      'target_platform=' . $plan['new_nid'],
      'new_uri=' . $domain,
      'new_db_server=' . $plan['new_db_nid'],
      $plan['marker'],
      'return_step=3',
    ));
    $stage = _provision_tasks_extra_return_where($plan);
    if ($stage !== 'start') {
      $back = _provision_tasks_extra_return_why($leg, $aside, $domain);
      $both = ($why === '') ? $back : $why . ', and then ' . $back;
      // Undone by its own rollback, the rename left the name free and both
      // versions where they were: a later run tries again.
      $on = ($copy_enabled || ($plan['copy_on'] && !$plan['copy_was_on'])) ? ' The copy ' . $copy . ' is switched on.' : '';
      if ($stage === 'aside') {
        return _provision_tasks_extra_return_unfinished($plan, $both, $on);
      }
      return _provision_tasks_extra_return_stuck($plan, $both, $on);
    }
    // A rename brings the site out switched on in the backend; the panel
    // still holds it as switched off.
    $leg = _provision_tasks_extra_upgrade_leg('@' . $domain, 'enable', array($plan['marker'], 'return_step=3b'));
    $tail = '';
    if (!$leg['ok']) {
      $tail .= ' The panel could not mark ' . $domain . ' as switched on (see above); enable it in the panel.';
    }
    // The copy was switched off before this Go back: a run of it that
    // switched the copy on, this one or an earlier one, switches it off.
    if ($copy_enabled || ($plan['copy_on'] && !$plan['copy_was_on'])) {
      $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'disable', array($plan['marker'], 'return_step=3c'));
      if (!$leg['ok']) {
        $tail .= ' The copy ' . $copy . ' is still switched on (see above); disable it in the panel.';
      }
    }
    return drush_set_error('PROVISION_UPGRADE_RETURN_NOT_GONE', dt('GOBACK/RESULT Not gone back: !why. !before again, as before Go back, and the copy @copy is kept.!tail', array(
      '!why' => _provision_tasks_extra_no_markup($why),
      '!before' => _provision_tasks_extra_no_markup($before),
      '@copy' => $copy,
      '!tail' => _provision_tasks_extra_no_markup($tail),
    )));
  }

  if ($stage !== 'back' || !is_dir($plan['old_root'] . '/sites/' . $domain)) {
    return _provision_tasks_extra_return_stuck($plan, ($why !== '') ? $why : 'the steps ended without saying where ' . $domain . ' is');
  }
  $base = $domain . ' runs Drupal ' . $plan['old_release'] . ' on ' . $plan['old_title'] . ' again'
    . (($plan['when'] === '') ? '' : ', as it was at ' . $plan['when']);
  if ($notes) {
    drush_log(dt('GOBACK/RESULT Gone back, with a note: !base. The upgraded site is kept as @aside, but !note.', array(
      '!base' => _provision_tasks_extra_no_markup($base),
      '@aside' => $aside,
      '!note' => _provision_tasks_extra_no_markup(implode('; ', $notes)),
    )), 'warning');
    return;
  }
  drush_log(dt('GOBACK/RESULT Gone back: !base. The upgraded site is kept, switched off, as @aside: what was added to it since the upgrade can still be copied from there.', array(
    '!base' => _provision_tasks_extra_no_markup($base),
    '@aside' => $aside,
  )), 'success');
}

/**
 * A path as the panel writes one: no repeated or dot segments, no end slash.
 */
function _provision_tasks_extra_successor_path($path) {
  $path = preg_replace('#/(\./)+#', '/', preg_replace('#/+#', '/', trim((string) $path)));
  return rtrim(preg_replace('#/\.$#', '', $path), '/');
}

/**
 * Whether the build's Verify found its folder already there and used it.
 *
 * A folder put there after the run's own check is no build of this run:
 * it is never kept and never removed.
 */
function _provision_tasks_extra_successor_found($plan, $build) {
  clearstatcache();
  return is_link($plan['path']) || _provision_tasks_extra_upgrade_logged($build, 'path already exists: nothing is built');
}

/**
 * The result of a run whose folder appeared before its build.
 *
 * Provision's Delete removes what a platform's root holds only when that
 * root holds a codebase Drush can load; otherwise only the record goes.
 */
function _provision_tasks_extra_successor_found_why($title, $path, $has_code) {
  $parts = _provision_tasks_extra_successor_found_parts($title, $path, $has_code);
  return 'Not kept: ' . $parts['why'] . '. ' . $parts['tail'];
}

/**
 * The two halves of that result: why, and what is left.
 */
function _provision_tasks_extra_successor_found_parts($title, $path, $has_code) {
  $tail = $title . ' is left, Locked, naming that folder';
  if ($has_code) {
    $tail .= ', and its Delete would remove the code there too: move the folder away first if it is wanted, then the account\'s main login can delete ' . $title . '.';
  }
  else {
    $tail .= '. It holds no code, so a Delete takes the record only: the account\'s main login can delete ' . $title . ' and the folder stays as it is.';
  }
  return array(
    'why' => 'a folder appeared at ' . $path . ' before the build, so nothing was built and nothing there was deleted',
    'tail' => $tail,
  );
}

/**
 * The new platform's name, for the log.
 */
function _provision_tasks_extra_successor_title() {
  $title = trim((string) drush_get_option('successor_title', ''));
  return ($title === '') ? 'the new platform' : $title;
}

/**
 * Ends a Build newer version that was refused before its build.
 *
 * The panel made the new platform's record when the task started, and it
 * was never built: a quiet Delete ends that record. A platform never
 * verified has no alias, so its Delete removes the record and touches
 * nothing on disk.
 *
 * @param string $prefix
 *   The result line's marker: an Upgrade that builds first says its own.
 * @param string $code
 *   The error's code.
 */
function _provision_tasks_extra_successor_unstarted($why, $prefix = 'SUCCESSOR/RESULT', $code = 'PROVISION_SUCCESSOR_NOT_STARTED') {
  $context = trim((string) drush_get_option('successor_context', ''));
  $task = trim((string) drush_get_option('successor_task_nid', ''));
  $title = _provision_tasks_extra_successor_title();
  $tail = 'Nothing was changed.';
  if (preg_match('/^@platform_[A-Za-z0-9_.-]+$/', $context) && ctype_digit($task) && (int) $task > 0) {
    if (_provision_tasks_extra_upgrade_alias_root(ltrim($context, '@')) === FALSE) {
      $leg = _provision_tasks_extra_upgrade_leg($context, 'delete', array('upgrade_run=' . (int) $task), TRUE);
      $tail = $leg['ok']
        ? 'The platform record made for it, ' . $title . ', was deleted; nothing else was changed.'
        : 'The platform record made for it, ' . $title . ', is left, Locked, with nothing on disk; the account\'s main login can delete it.';
    }
    else {
      $tail = 'The platform record made for it, ' . $title . ', is left, Locked; the account\'s main login can delete it.';
    }
  }
  return drush_set_error($code, dt('!prefix Not started: !why. !tail', array(
    '!prefix' => $prefix,
    '!why' => _provision_tasks_extra_no_markup($why),
    '!tail' => _provision_tasks_extra_no_markup($tail),
  )));
}

/**
 * What a Build newer version will do, every check made: a plan, or why not.
 *
 * After the panel's start these can fail only by a race.
 *
 * @param object $source
 *   The platform to build from: this task's own (d()) when NULL, the site's
 *   platform for an Upgrade that builds its target first.
 */
function _provision_tasks_extra_successor_plan($source = NULL) {
  if (!function_exists('_provision_tasks_extra_successor_newer') || !function_exists('_provision_tasks_extra_build_marker') || !function_exists('_provision_tasks_extra_tooling_skip')) {
    return 'the backend\'s copy of the task\'s rules is older than the panel\'s; start again in a few minutes';
  }
  if ($source === NULL) {
    $source = d();
  }
  if (!is_object($source) || $source->type !== 'platform' || empty($source->root)) {
    return 'this task runs on a platform';
  }
  $opt = array();
  foreach (array('successor_context', 'successor_nid', 'successor_path', 'successor_root', 'successor_source_path', 'successor_task_nid') as $name) {
    $opt[$name] = trim((string) drush_get_option($name, ''));
    if ($opt[$name] === '') {
      return 'the task did not say what to build (' . $name . ' is missing); start it from the platform\'s page';
    }
  }
  foreach (array('successor_nid', 'successor_task_nid') as $name) {
    if (!ctype_digit($opt[$name]) || (int) $opt[$name] < 1) {
      return 'the task did not say what to build (' . $name . ' is not an id); start it from the platform\'s page';
    }
  }
  if (!preg_match('/^@platform_[A-Za-z0-9_.-]+$/', $opt['successor_context'])) {
    return 'the new platform\'s name is not one the panel makes';
  }
  $path = _provision_tasks_extra_successor_path($opt['successor_path']);
  $from = _provision_tasks_extra_successor_path($opt['successor_source_path']);
  $root = _provision_tasks_extra_successor_path($opt['successor_root']);
  $source_root = _provision_tasks_extra_docroot($source->root);
  $built = '';
  foreach (array('composer_project_path', 'composer_git_path') as $property) {
    if (isset($source->{$property}) && is_string($source->{$property}) && $source->{$property} !== '') {
      $built = _provision_tasks_extra_successor_path($source->{$property});
    }
  }
  if ($built !== '' && $built !== $from) {
    return 'the build folder of the platform it is built from is not the one the panel named';
  }
  if ($path === '' || $path[0] !== '/' || strpos($path, '/../') !== FALSE || dirname($path) !== dirname($from) || $path === $from) {
    return 'the new folder is not one beside the platform it is built from';
  }
  if ($root !== $path && (strpos($root, $path . '/') !== 0 || strpos(substr($root, strlen($path) + 1), '/') !== FALSE)) {
    return 'the new Platform Path is not its folder or a folder in it';
  }
  if (file_exists('/run/boa_queue_stop.pid')) {
    return 'the task queue is paused on this server for maintenance; start again later';
  }
  if (file_exists('/run/octopus_install_run.pid')) {
    return 'a system upgrade of this account is running; start again when it has ended';
  }
  if (_provision_tasks_extra_upgrade_panel_moving()) {
    return 'the control panel is moving to its new version; start again in a few minutes';
  }
  $lock = _provision_tasks_extra_lock_load($source_root);
  if (empty($lock['ok'])) {
    return 'the platform it is built from has ' . $lock['why'];
  }
  $release = _provision_tasks_extra_core_release($source_root);
  if ($release === '') {
    return 'the Drupal release of the platform it is built from cannot be read';
  }
  // The line of the platform built from governs the build, read here once:
  // a line changed while the build runs does not change the answer.
  $modes = function_exists('_provision_tasks_extra_build_lists_refusal')
    ? _provision_tasks_extra_upgrade_modes($source_root . '/sites/all/modules/boa_platform_control.ini')
    : array('state' => 'absent');
  $mode = trim((string) drush_get_option('successor_mode', ''));
  $lock_build = _provision_tasks_extra_successor_lock_plan($source, $mode, $from, $built, $source_root, $lock);
  if (is_string($lock_build)) {
    return $lock_build;
  }
  if (function_exists('_provision_tasks_extra_build_lists_refusal')) {
    $kind = _provision_tasks_extra_build_kind_before((bool) drush_get_option('successor_template', FALSE), $mode, isset($lock_build['choice']) ? $lock_build['choice'] : NULL);
    $why = _provision_tasks_extra_build_lists_refusal($kind, $modes, array('state' => 'absent'));
    if ($why !== '') {
      return $why;
    }
  }
  clearstatcache();
  if (file_exists($path) || is_link($path)) {
    return 'its folder ' . $path . ' is taken';
  }
  $marker = _provision_tasks_extra_build_marker($path);
  if ($marker['state'] !== 'none') {
    return 'beside its folder ' . $path . ' stands the mark of an unfinished build, or one that cannot be read';
  }
  $numbers = _provision_tasks_extra_release_numbers($release);
  return array(
    'context' => $opt['successor_context'],
    'nid' => (int) $opt['successor_nid'],
    'path' => $path,
    'root' => $root,
    'task_nid' => (int) $opt['successor_task_nid'],
    'major' => (int) drush_get_option('successor_major', 0),
    'title' => _provision_tasks_extra_successor_title(),
    'source_title' => trim((string) drush_get_option('source_title', '')) !== '' ? trim((string) drush_get_option('source_title', '')) : 'the platform it is built from',
    'source_root' => $source_root,
    'source_lock' => $lock,
    'source_release' => $release,
    'source_major' => $numbers ? $numbers[0] : 0,
    'marker' => 'upgrade_run=' . (int) $opt['successor_task_nid'],
    'modes' => $modes,
  ) + $lock_build;
}

/**
 * The plan of a build from the platform's own lock, array() for another
 * build, or why it cannot be built so.
 *
 * Only a platform the panel built from a published package, whose two files
 * are still the ones the panel's build left (the panel names them in the
 * task's options), and whose composer.json takes no package from the
 * account's own folders.
 *
 * @param object $source
 *   The platform built from.
 * @param string $mode
 *   The task's Mode.
 * @param string $from
 *   Its project folder, as the panel named it.
 * @param string $built
 *   Its build folder, as its record has it.
 * @param string $source_root
 *   Its docroot.
 * @param array $lock
 *   _provision_tasks_extra_lock_load() of it.
 */
function _provision_tasks_extra_successor_lock_plan($source, $mode, $from, $built, $source_root, $lock) {
  $route = trim((string) drush_get_option('successor_route', ''));
  if ($route === '' && $mode !== 'lock') {
    return array();
  }
  if ($route !== 'lock' || $mode !== 'lock') {
    return 'the task did not say how to build (its route and its Mode differ); start it from the platform\'s page';
  }
  if (!function_exists('_provision_tasks_extra_lock_build_args')) {
    return 'the backend\'s copy of the task\'s rules is older than the panel\'s; start again in a few minutes';
  }
  $choice = _provision_tasks_extra_lock_build_choice(array(
    'successor_scope' => drush_get_option('successor_scope', ''),
    'successor_level' => drush_get_option('successor_level', ''),
    'successor_security' => drush_get_option('successor_security', ''),
  ), 'successor_');
  if (is_string($choice)) {
    return $choice;
  }
  if (!class_exists('Provision_ComposerLockBuild')) {
    return 'the backend\'s copy of the platform builds is older than the panel\'s; start again in a few minutes';
  }
  $package = (isset($source->composer_project_package) && is_string($source->composer_project_package)) ? trim($source->composer_project_package) : '';
  $git = (isset($source->composer_git_path) && is_string($source->composer_git_path)) ? trim($source->composer_git_path) : '';
  if ($package === '' || $git !== '' || $built === '' || $built !== $from) {
    return 'it was not built by the panel from a published package, so it cannot be built from its own composer.lock';
  }
  $expected = trim((string) drush_get_option('successor_fingerprint', ''));
  if (!preg_match('/^sha256:[0-9a-f]{64}:[0-9a-f]{64}$/', $expected)) {
    return 'the task did not say what the files it builds from must be; start it from the platform\'s page';
  }
  $now = _provision_tasks_extra_lock_fingerprint($from);
  if ($now === '') {
    return 'its composer.json or composer.lock cannot be read';
  }
  if ($now !== $expected) {
    return 'its composer.json or composer.lock changed since the panel built it';
  }
  $raw = _provision_tasks_extra_read_plain($from . '/composer.json', 4194304);
  $why = _provision_tasks_extra_lock_build_repositories_why(($raw === FALSE) ? NULL : json_decode($raw, TRUE));
  if ($why !== '') {
    return $why;
  }
  $advised = NULL;
  if ($choice['security']) {
    $advised = _provision_tasks_extra_lock_build_advised($lock, _provision_tasks_extra_lock_advisories($source_root));
    if (is_string($advised)) {
      return $advised . ', so no fix can be chosen';
    }
  }
  $words = _provision_tasks_extra_lock_build_args($lock, $choice, $advised);
  if (is_string($words)) {
    return $words;
  }
  $perms = @fileperms($from);
  return array(
    'route' => 'lock',
    'choice' => $choice,
    'resolve' => $words,
    'fingerprint' => $expected,
    'source_path' => $from,
    'source_dev' => _provision_tasks_extra_lock_dev($from),
    'source_mode' => ($perms === FALSE) ? 0 : ($perms & 07777),
  );
}

/**
 * Builds the new folder from the platform's own lock, before the new
 * platform's Verify: '' when it is built, or why not.
 *
 * hosting_deploy's build runs in this process, so the recipe and the files
 * it checks never leave the task that checked them; its marker, its step
 * limits and the removal of a failed build are every panel build's. Sets
 * 'files' in $plan, the fingerprint of the two files it made.
 */
function _provision_tasks_extra_successor_lock_run(&$plan) {
  drush_log(dt('SUCCESSOR/CHECK building !title from the composer.lock of !source: !words.', array(
    '!title' => _provision_tasks_extra_no_markup($plan['title']),
    '!source' => _provision_tasks_extra_no_markup($plan['source_title']),
    '!words' => _provision_tasks_extra_lock_build_words($plan['choice']),
  )), 'ok');
  $judged = array(
    'source_lock' => $plan['source_lock'],
    'source_release' => $plan['source_release'],
    'source_dev' => $plan['source_dev'],
    'choice' => $plan['choice'],
  );
  $docroot = ($plan['root'] === $plan['path']) ? '' : basename($plan['root']);
  $build = new Provision_ComposerLockBuild(array(
    'path' => $plan['path'],
    'source' => $plan['source_path'],
    'docroot' => $docroot,
    'resolve' => $plan['resolve'],
    'fingerprint' => $plan['fingerprint'],
    'judge' => function ($folder) use ($judged) {
      return _provision_tasks_extra_lock_build_judge($judged, $folder);
    },
    'mode' => $plan['source_mode'],
  ));
  if ($build->deploy() !== TRUE) {
    $text = trim(preg_replace('/\s+/', ' ', (string) $build->errorText()));
    if (strlen($text) > 600) {
      $text = substr($text, 0, 600) . '...';
    }
    if ($build->errorCode() === 'PLATFORM_LOCK_BUILD_REFUSED' && $text !== '') {
      return $text;
    }
    return 'its build from the composer.lock failed: ' . ($text === '' ? 'it ended with no result' : $text);
  }
  $plan['files'] = $build->files();
  return '';
}

/**
 * Implements drush_hook_COMMAND_validate() for provision-platform-successor.
 */
function drush_provision_tasks_extra_provision_platform_successor_validate() {
  $plan = _provision_tasks_extra_successor_plan();
  if (!is_array($plan)) {
    return _provision_tasks_extra_successor_unstarted($plan);
  }
  drush_set_context('PROVISION_SUCCESSOR_PLAN', $plan);
}

/**
 * Implements the provision-platform-successor command.
 *
 * The new platform's own Verify is the build, as a step of this task; the
 * build is kept when the gate says so and is otherwise deleted, unless a
 * build process still writes into its folder.
 */
function drush_provision_tasks_extra_provision_platform_successor() {
  $plan = drush_get_context('PROVISION_SUCCESSOR_PLAN', FALSE);
  if (!is_array($plan) || empty($plan['context'])) {
    return _provision_tasks_extra_successor_unstarted('its checks did not run');
  }
  // This process holds the platform it builds from: nothing of it is saved
  // when the command ends.
  drush_set_option('provision_save_config', FALSE);
  $built = _provision_tasks_extra_successor_build($plan, array($plan['marker']));
  if ($built['state'] === 'not_kept') {
    return drush_set_error('PROVISION_SUCCESSOR_NOT_KEPT', 'SUCCESSOR/RESULT Not kept: ' . $built['why'] . '. ' . $built['tail']);
  }
  if ($built['state'] === 'locked') {
    drush_log(dt('SUCCESSOR/RESULT Built !title at !path: !kind. It stays Locked, since !why: !next, and the Upgrade then offers it to the sites of !source.', array(
      '!title' => _provision_tasks_extra_no_markup($plan['title']),
      '!path' => _provision_tasks_extra_no_markup($plan['built_root']),
      '!kind' => _provision_tasks_extra_no_markup($plan['kind']),
      '!why' => $built['why'],
      '!next' => $built['next'],
      '!source' => _provision_tasks_extra_no_markup($plan['source_title']),
    )), 'warning');
    return;
  }
  drush_log(dt('SUCCESSOR/RESULT Built !title at !path: !kind.', array(
    '!title' => _provision_tasks_extra_no_markup($plan['title']),
    '!path' => _provision_tasks_extra_no_markup($plan['built_root']),
    '!kind' => _provision_tasks_extra_no_markup($plan['kind']),
  )), 'success');
}

/**
 * Builds, judges and keeps or removes a new platform: the body of Build
 * newer version, which an Upgrade that builds its target first runs too.
 *
 * The new platform's own Verify is the build, as a step of the calling
 * task; the build is kept when the gate says so and unlocked, and is
 * otherwise deleted, unless a build process still writes into its folder.
 * Each caller writes its own result line.
 *
 * @param array $plan
 *   _provision_tasks_extra_successor_plan(); 'built_root', 'kind' and
 *   'release' are set in it once the build is judged.
 * @param array $step_args
 *   The arguments of each step row: the marker of the calling task first.
 *
 * @return array
 *   'state': kept, locked (kept, its Unlock failed) or not_kept; 'why' (why
 *   it was not kept, or why its Unlock failed) and 'tail' (what is left of
 *   it, for not_kept), in plain words.
 */
function _provision_tasks_extra_successor_build(&$plan, $step_args) {
  drush_log(dt('SUCCESSOR/CHECK building !title in !path from !source (Drupal !release).', array(
    '!title' => _provision_tasks_extra_no_markup($plan['title']),
    '!path' => _provision_tasks_extra_no_markup($plan['path']),
    '!source' => _provision_tasks_extra_no_markup($plan['source_title']),
    '!release' => _provision_tasks_extra_no_markup($plan['source_release']),
  )), 'ok');
  // A build from the platform's own lock makes the folder first, here; the
  // Verify then takes it as it is.
  if (isset($plan['route']) && $plan['route'] === 'lock') {
    clearstatcache();
    if (file_exists($plan['path']) || is_link($plan['path'])) {
      $parts = _provision_tasks_extra_successor_found_parts(
        _provision_tasks_extra_no_markup($plan['title']),
        _provision_tasks_extra_no_markup($plan['path']),
        (bool) drush_valid_root($plan['root'])
      );
      return array('state' => 'not_kept', 'why' => $parts['why'], 'tail' => $parts['tail']);
    }
    $why = _provision_tasks_extra_successor_lock_run($plan);
    if ($why !== '') {
      return _provision_tasks_extra_successor_remove($plan, $why, NULL, $step_args);
    }
  }
  $build = _provision_tasks_extra_upgrade_leg($plan['context'], 'verify', $step_args, TRUE);
  // The caller sets its own result: an error of its build is kept as a
  // warning, whether the build ended well or not.
  _provision_tasks_extra_upgrade_replay($build, TRUE);
  // After a build from the lock the Verify finds this run's own folder, and
  // says so: only a link put in its place is a folder that appeared.
  clearstatcache();
  if (!empty($plan['files']) ? is_link($plan['path']) : _provision_tasks_extra_successor_found($plan, $build)) {
    clearstatcache();
    $parts = _provision_tasks_extra_successor_found_parts(
      _provision_tasks_extra_no_markup($plan['title']),
      _provision_tasks_extra_no_markup($plan['path']),
      (bool) drush_valid_root($plan['root'])
    );
    return array('state' => 'not_kept', 'why' => $parts['why'], 'tail' => $parts['tail']);
  }
  if (!$build['ok']) {
    return _provision_tasks_extra_successor_remove($plan, _provision_tasks_extra_successor_build_why($plan, $build), $build, $step_args);
  }
  $why = _provision_tasks_extra_successor_gate($plan);
  if ($why !== '') {
    return _provision_tasks_extra_successor_remove($plan, $why, NULL, $step_args);
  }
  // What a build from the lock made, for the panel to record on the new
  // platform's first Verify: the panel reads it from this line alone.
  if (!empty($plan['files'])) {
    drush_log('SUCCESSOR/FILES ' . $plan['files'], 'ok');
  }
  // The line of the platform built from goes with the build, before it is
  // unlocked: an unlocked build without it would be offered ungoverned.
  $carry = _provision_tasks_extra_successor_carry_line($plan);
  if ($carry !== '') {
    return array(
      'state' => 'locked',
      'why' => 'the drupal_upgrade_modes line of the platform it was built from could not be written into it (' . _provision_tasks_extra_no_markup($carry) . ')',
      'next' => 'add `' . _provision_tasks_extra_build_modes_line($plan['modes']) . '` to its sites/all/modules/boa_platform_control.ini, then the account\'s main login can Unlock it on its page',
      'tail' => '',
    );
  }
  $unlock = _provision_tasks_extra_upgrade_leg($plan['context'], 'unlock', array($step_args[0]), TRUE);
  if (!$unlock['ok']) {
    _provision_tasks_extra_upgrade_replay($unlock, TRUE);
    return array(
      'state' => 'locked',
      'why' => 'its Unlock did not work (' . _provision_tasks_extra_no_markup(_provision_tasks_extra_upgrade_step_why($unlock, $unlock['errors'] ? (string) reset($unlock['errors']) : 'it ended with no result')) . ')',
      'next' => 'the account\'s main login can Unlock it on its page',
      'tail' => '',
    );
  }
  return array('state' => 'kept', 'why' => '', 'tail' => '');
}

/**
 * Writes the drupal_upgrade_modes line of the platform a build was made from
 * into the kept build's own control file, or says why it could not.
 *
 * A new platform has no live line: its first Verify seeds only the inactive
 * template. Only a list goes, as read in the plan; a line the build's own
 * file already holds (a repository's own policy) is kept. The file is made
 * as the nightly makes it, a copy of the template, the account's own at
 * 0664, written as a new file renamed into a folder that is no link.
 *
 * @return string
 *   '' when written, kept or not needed; else why.
 */
function _provision_tasks_extra_successor_carry_line($plan) {
  if (!isset($plan['modes']['state']) || $plan['modes']['state'] !== 'list' || !function_exists('_provision_tasks_extra_build_modes_text')) {
    return '';
  }
  $root = rtrim((string) $plan['built_root'], '/');
  $dir = $root . '/sites/all/modules';
  clearstatcache();
  foreach (array($root . '/sites', $root . '/sites/all', $dir) as $step) {
    if (is_link($step) || !is_dir($step)) {
      return $step . ' is not a plain folder';
    }
  }
  $file = $dir . '/boa_platform_control.ini';
  if (is_link($file) || (file_exists($file) && !is_file($file))) {
    return $file . ' is not a plain file';
  }
  if (file_exists($file)) {
    $text = _provision_tasks_extra_read_plain($file, 1048576);
    if ($text === FALSE) {
      return $file . ' cannot be read';
    }
  }
  else {
    $template = '/data/conf/default.boa_platform_control.ini';
    $text = (is_file($template) && !is_link($template)) ? _provision_tasks_extra_read_plain($template, 1048576) : '';
    $text = ($text === FALSE) ? '' : $text;
  }
  $line = _provision_tasks_extra_build_modes_line($plan['modes']);
  $new = _provision_tasks_extra_build_modes_text($text, $line);
  if ($new === FALSE) {
    drush_log(dt('SUCCESSOR/CHECK !file already holds its own drupal_upgrade_modes line, which is kept.', array('!file' => _provision_tasks_extra_no_markup($file))), 'ok');
    return '';
  }
  $tmp = @tempnam($dir, '.ctrl.');
  // tempnam() names the folder by its real path.
  if ($tmp === FALSE || dirname($tmp) !== realpath($dir)) {
    if ($tmp !== FALSE) {
      @unlink($tmp);
    }
    return 'no file can be made in ' . $dir;
  }
  if (@file_put_contents($tmp, $new) !== strlen($new) || !@chmod($tmp, 0664) || !@rename($tmp, $file)) {
    @unlink($tmp);
    return $file . ' could not be written';
  }
  drush_log(dt('SUCCESSOR/CHECK the line of the platform it was built from, !line, is written into !file.', array(
    '!line' => _provision_tasks_extra_no_markup($line),
    '!file' => _provision_tasks_extra_no_markup($file),
  )), 'ok');
  return '';
}

/**
 * Why a build failed, in the account's words.
 *
 * A build of core's templates for a major whose own PHP floor is above the
 * PHP it ran on says so, with the line that names a newer one.
 */
function _provision_tasks_extra_successor_build_why($plan, $build) {
  $messages = array();
  foreach ($build['log'] as $entry) {
    if (is_array($entry) && isset($entry['message'])) {
      $messages[] = is_array($entry['message']) ? implode("\n", $entry['message']) : (string) $entry['message'];
    }
  }
  if ($plan['major'] > 0) {
    $said = _provision_tasks_extra_cli_php_said($messages, $plan['context']);
    $floor = _provision_tasks_extra_php_minimum($plan['major'], ($plan['major'] === $plan['source_major']) ? $plan['source_lock'] : NULL);
    // A cli-per-platform.info line counts only for a folder that is there,
    // and the build makes its folder: only the account's own PHP can change
    // what a new build runs on.
    if ($said['php'] !== '' && $floor !== '' && version_compare($said['php'], $floor, '<')) {
      return 'the build ran on PHP ' . $said['php'] . ' (' . $said['from'] . ') and Drupal ' . $plan['major'] . ' needs ' . $floor
        . ' or newer. Raise the account\'s command-line PHP to ' . $floor . ' or newer in ~/static/control/cli.info'
        . ' (a line in cli-per-platform.info counts only for a folder that is already there), then build again';
    }
  }
  $first = $build['errors'] ? trim(preg_replace('/\s+/', ' ', (string) reset($build['errors']))) : '';
  if (strlen($first) > 600) {
    $first = substr($first, 0, 600) . '...';
  }
  return _provision_tasks_extra_upgrade_step_why($build, $first === '' ? 'its build ended with no result' : 'its build failed: ' . $first);
}

/**
 * How ~/static/control/cli-per-platform.info names a build folder.
 */
function _provision_tasks_extra_successor_folder_key($path) {
  $master = d('@server_master');
  $home = (is_object($master) && !empty($master->aegir_root)) ? rtrim($master->aegir_root, '/') : drush_server_home();
  return (strpos($path, $home . '/static/') === 0) ? substr($path, strlen($home . '/static/')) : $path;
}

/**
 * Judges a build that ended well: '' to keep it, or why not.
 *
 * Its release and lock, its own Drush, the same codebase and newer by the
 * platform's rule. Sets 'kind' and 'release' in $plan; the kind names a
 * security advisory the build adds.
 */
function _provision_tasks_extra_successor_gate(&$plan) {
  $root = _provision_tasks_extra_upgrade_alias_root(ltrim($plan['context'], '@'));
  if (!is_string($root) || $root === '') {
    return 'the new platform\'s record names no folder';
  }
  // Both as provision resolves a root: a platform recorded at its project
  // folder is served from the docroot below it.
  if (!_provision_tasks_extra_upgrade_same_root($root, _provision_tasks_extra_docroot($plan['root']))) {
    return 'the new platform\'s record names ' . $root . ', not ' . $plan['root'];
  }
  $plan['built_root'] = $root;
  $release = _provision_tasks_extra_core_release($root);
  if ($release === '') {
    return 'the Drupal release of the build cannot be read';
  }
  $lock = _provision_tasks_extra_lock_load($root);
  if (empty($lock['ok'])) {
    return 'the build has ' . $lock['why'];
  }
  $app = _provision_tasks_extra_app_root($root);
  $has = ($app !== '')
    ? _provision_codebase_has_drush($app)
    : (_provision_codebase_has_drush($root) || _provision_codebase_has_drush(dirname($root)));
  if (!$has) {
    return 'the build brings no Drush of its own';
  }
  $dead = _provision_tasks_extra_drush_cannot_start($root);
  if ($dead !== '') {
    return 'its own Drush cannot start: ' . $dead;
  }
  $other = _provision_tasks_extra_codebase_other($plan['source_lock'], $lock, 0, 'the platform it was built from');
  if ($other === NULL) {
    return 'its composer.lock cannot be compared with the one of the platform it was built from';
  }
  if ($other !== '') {
    return $other;
  }
  $skip = _provision_tasks_extra_tooling_skip($plan['source_lock'], $lock);
  $newer = _provision_tasks_extra_successor_newer($plan['source_release'], $release, _provision_tasks_extra_lock_diff($plan['source_root'], $root), $skip);
  if ($newer['why'] !== '') {
    return $newer['why'];
  }
  // The audits only once the build is newer: each may take minutes.
  $audits = array();
  foreach (array('source' => $plan['source_root'], 'build' => $root) as $side => $audit_root) {
    $audits[$side] = _provision_tasks_extra_lock_advisories($audit_root);
    if ($audits[$side]['lines'] === FALSE && $audits[$side]['lock'] !== '') {
      drush_log(dt('SUCCESSOR/CHECK the security audit of @side did not finish (@why): advisories were not compared.', array(
        '@side' => ($side === 'source') ? 'the platform it was built from' : 'the build',
        '@why' => _provision_tasks_extra_no_markup($audits[$side]['why']),
      )), 'ok');
    }
  }
  // An advisory the build adds is named by the kind, never refused.
  $diff = _provision_tasks_extra_upgrade_release_step($plan['source_release'], $release, _provision_tasks_extra_lock_diff($plan['source_root'], $root, $audits['source'], $audits['build']));
  $plan['release'] = $release;
  $plan['kind'] = _provision_tasks_extra_upgrade_kind_text($diff);
  // The platform's line keeps only a build of a kind it admits; a kind that
  // cannot be told on an axis the line restricts is no such build.
  if (isset($plan['modes']) && function_exists('_provision_tasks_extra_build_kept_refusal')) {
    $why = _provision_tasks_extra_build_kept_refusal($plan['modes'], $diff);
    if ($why !== '') {
      return $why . ' (the build is: ' . $plan['kind'] . ')';
    }
  }
  // A build from the lock is kept only as what it was asked to be: a
  // security build that fixes nothing among them.
  if (isset($plan['choice']) && is_array($plan['choice'])) {
    $allow = _provision_tasks_extra_upgrade_modes_allow(_provision_tasks_extra_lock_build_modes($plan['choice']), $diff);
    if (!$allow['allowed']) {
      return 'it was asked for ' . _provision_tasks_extra_lock_build_words($plan['choice']) . ', and the choice ' . $allow['why'] . ' (the build is: ' . $plan['kind'] . ')';
    }
  }
  return '';
}

/**
 * Removes a build that is not kept, when nothing keeps it, and says what is
 * left: _provision_tasks_extra_successor_build()'s not_kept answer, its words
 * free of markup.
 *
 * @param array $step_args
 *   The arguments of the build's step rows, the calling task's marker first.
 */
function _provision_tasks_extra_successor_remove($plan, $why, $build, $step_args) {
  $codes = is_array($build) ? $build['codes'] : array();
  $args = array(
    '!why' => _provision_tasks_extra_no_markup($why),
    '!title' => _provision_tasks_extra_no_markup($plan['title']),
    '!path' => _provision_tasks_extra_no_markup($plan['path']),
  );
  $out = array('state' => 'not_kept', 'why' => $args['!why'], 'tail' => '');
  $marker = _provision_tasks_extra_build_marker($plan['path']);
  if ($marker['alive'] || in_array('PLATFORM_BUILD_STILL_RUNNING', $codes, TRUE)) {
    $out['tail'] = dt('A build process still writes into !path; delete !title once it has ended (the account\'s main login can delete it).', $args);
    return $out;
  }
  if ($marker['state'] === 'unknown') {
    $out['tail'] = dt('Whether a build process still writes into !path cannot be read, so nothing was deleted; the account\'s main login can delete !title.', $args);
    return $out;
  }
  // A build from the lock made its folder itself: its Verify's "already
  // there" is that folder.
  if (is_array($build) && (!empty($plan['files']) ? is_link($plan['path']) : _provision_tasks_extra_successor_found($plan, $build))) {
    $out['tail'] = dt('A folder appeared at !path before the build, so nothing there was deleted; !title is left, Locked, naming it: move the folder away first if it is wanted, then the account\'s main login can delete !title.', $args);
    return $out;
  }
  // websh refuses a Verify and a Delete of a codebase that needs a newer PHP
  // than the one named for it: it stays until one is.
  if (in_array('PLATFORM_BUILD_PHP_TOO_OLD', $codes, TRUE)) {
    $m = array();
    $need = (preg_match('/needs PHP ([0-9]+\.[0-9]+) or newer/', implode("\n", $build['errors']), $m)) ? $m[1] : 'X.Y';
    $args['!line'] = _provision_tasks_extra_no_markup(_provision_tasks_extra_successor_folder_key($plan['path']) . ' ' . $need);
    $args['!need'] = $need;
    $out['why'] = dt('the build at !path needs PHP !need or newer and none is named for it', $args);
    $out['tail'] = dt('Add `!line` to ~/static/control/cli-per-platform.info, then the account\'s main login can delete !title.', $args);
    return $out;
  }
  $leg = _provision_tasks_extra_upgrade_leg($plan['context'], 'delete', array($step_args[0]), TRUE);
  _provision_tasks_extra_upgrade_replay($leg, TRUE);
  clearstatcache();
  $after = _provision_tasks_extra_build_marker($plan['path']);
  $alias = _provision_tasks_extra_upgrade_alias_root(ltrim($plan['context'], '@'));
  if ($leg['ok'] && !file_exists($plan['path']) && !is_link($plan['path']) && $alias === FALSE && $after['state'] === 'none') {
    $out['tail'] = 'The build was deleted.';
    return $out;
  }
  if ($leg['ok'] && $alias === FALSE) {
    $out['tail'] = dt('Its record was deleted, and !path is still there (see the warnings above): remove it with the account\'s own login over SSH or SFTP before that name is used again.', $args);
    return $out;
  }
  if (!$leg['ok']) {
    $left = _provision_tasks_extra_upgrade_step_why($leg, $leg['errors'] ? trim(preg_replace('/\s+/', ' ', (string) reset($leg['errors']))) : 'its Delete ended with no result');
  }
  elseif (file_exists($plan['path']) || is_link($plan['path'])) {
    $left = 'its folder is still there';
  }
  else {
    $left = 'its alias is still there';
  }
  $args['!left'] = _provision_tasks_extra_no_markup($left);
  $out['tail'] = dt('!title at !path could not be deleted (!left); the account\'s main login can delete it.', $args);
  return $out;
}

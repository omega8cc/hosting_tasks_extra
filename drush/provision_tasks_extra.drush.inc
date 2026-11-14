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
 * @return array
 *   'lines': the report's lines, FALSE when no audit finished. 'why': the
 *   reason when it did not. 'lock': the path of the lock that was read.
 */
function _provision_tasks_extra_lock_advisories($root) {
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
function _provision_tasks_extra_upgrade_refuse($why) {
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
 * The names under a folder that are way-back copies of a site.
 */
function _provision_tasks_extra_upgrade_copies($dir, $uri, $suffix = '') {
  $found = array();
  $names = @scandir($dir);
  if (!is_array($names)) {
    return $found;
  }
  $tail = '.temp.' . $uri . $suffix;
  foreach ($names as $name) {
    if (strncmp($name, 'back-', 5) === 0 && strlen($name) > strlen($tail) + 5 && substr($name, -strlen($tail)) === $tail) {
      $found[] = substr($name, 0, strlen($name) - strlen($suffix));
    }
  }
  return $found;
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
  $left = _provision_tasks_extra_upgrade_copies($source_root . '/sites', $uri);
  if ($left) {
    return 'an earlier upgrade left the copy ' . $left[0] . ' on ' . $source_title . '. Delete that site in the panel, or its folder ' . $source_root . '/sites/' . $left[0] . ' when the panel does not list it, then start again';
  }
  foreach (_provision_tasks_extra_upgrade_copies(drush_server_home() . '/.drush', $uri, '.alias.drushrc.php') as $name) {
    $root = _provision_tasks_extra_upgrade_alias_root($name);
    if ($root === '' || ($root !== FALSE && _provision_tasks_extra_upgrade_same_root($root, $source_root))) {
      return 'an earlier upgrade left the copy ' . $name . ' on ' . $source_title . '. Delete that site in the panel, then start again';
    }
  }

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

  // V10: no advisory the site does not have now.
  $source_audit = _provision_tasks_extra_lock_advisories($source_root);
  $target_audit = _provision_tasks_extra_lock_advisories($target_root);
  $sides = array('current' => $source_audit, 'new' => $target_audit);
  foreach ($sides as $side => $audit) {
    if ($audit['lines'] === FALSE && $audit['lock'] !== '') {
      drush_log(dt('UPGRADE/CHECK the security audit of the @side platform did not finish (@why): advisories were not compared.', array('@side' => $side, '@why' => $audit['why'])), 'ok');
    }
  }
  $diff = _provision_tasks_extra_upgrade_release_step($source_release, $target_release, _provision_tasks_extra_lock_diff($source_root, $target_root, $source_audit, $target_audit));
  if ($diff['added']) {
    $added = array();
    foreach ($diff['added'] as $pair) {
      $cols = explode("\t", $pair);
      $added[] = $cols[0] . ' on ' . (isset($cols[1]) ? $cols[1] : '?');
    }
    return $target_title . ' has security advisories the current platform does not have (' . _provision_tasks_extra_upgrade_names($added) . '). Build it again from current releases';
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
  $stays = ($platform_modes['state'] === 'absent') ? '' : ' The platform\'s list belongs to ' . $source_title . ' and does not follow the site to ' . $target_title . '.';
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
  drush_log(dt('UPGRADE/CHECK passed: the two releases, the site\'s database, the new platform\'s Drush and PHP, leftovers, the queue, free space, security advisories and both lists.'), 'ok');

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
    'stays' => $stays,
    'return' => (bool) drush_get_option('upgrade_return', FALSE),
  );
}

/**
 * Implements drush_hook_COMMAND_validate() for provision-drupal-upgrade.
 *
 * Every check runs before anything is made, fails closed, and relies on
 * nothing the panel's form did.
 */
function drush_provision_tasks_extra_provision_drupal_upgrade_validate() {
  $plan = _provision_tasks_extra_upgrade_plan();
  if (!is_array($plan)) {
    return _provision_tasks_extra_upgrade_refuse($plan);
  }
  drush_set_context('PROVISION_UPGRADE_PLAN', $plan);
}

/**
 * Runs one nested task and says how it went.
 *
 * A leg is judged by what its own invoke returned, never by this process's
 * error flag, which stays set from the first failed leg on. A leg that made
 * no task row returns an error like any other.
 *
 * @return array
 *   'ok', 'codes' (its error codes), 'errors' (their messages) and 'log'
 *   (what it logged, as this process received it).
 */
function _provision_tasks_extra_upgrade_leg($context, $type, $args = array()) {
  $before = count(drush_get_log());
  $result = provision_backend_invoke('@hostmaster', 'hosting-task', array_merge(array($context, $type), $args), array('force' => TRUE));
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
  return array(
    'ok' => is_array($result) && isset($result['error_status']) && (int) $result['error_status'] === 0,
    'codes' => $codes,
    'errors' => $errors,
    'log' => array_slice(drush_get_log(), $before),
  );
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
  $text = implode(' ', $leg['errors']);
  if (in_array('PROVISION_UPGRADE_UPDATEDB', $leg['codes'], TRUE)) {
    if (strpos($text, 'could not be run') !== FALSE) {
      return "the new platform's own Drush could not be run";
    }
    if (strpos($text, 'cannot start') !== FALSE) {
      return "the new platform's own Drush cannot start";
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
  // Only a copy on the site's own platform is this run's.
  if ($root !== FALSE && $root !== '' && _provision_tasks_extra_upgrade_same_root($root, $plan['source_root'])) {
    $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'delete', array($plan['marker']));
    $no_task = !$leg['ok'] && in_array('HOSTING_TASK_LOAD_FAILED', $leg['codes'], TRUE);
    if ($no_task && !empty($plan['recorded'])) {
      // The panel made its record of the copy when the Clone ended well,
      // and still made no task now (it is moving to its new version, say).
      // Removing the copy here would leave that record with nothing behind
      // it: the copy stays, for a Delete in the panel.
      drush_log(dt('UPGRADE/UNDO the panel could not start the Delete of @copy now: the copy is left as it is. Delete it in the panel.', array('@copy' => $copy)), 'warning');
    }
    elseif ($no_task) {
      // A Clone that failed after its deploy keeps the copy and makes no
      // panel record of it: no task row can delete it, the backend can.
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
  return drush_set_error('PROVISION_UPGRADE_NOT_UPGRADED', dt('UPGRADE/RESULT !line', array('!line' => _provision_tasks_extra_no_markup($line))));
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
  $plan = drush_get_context('PROVISION_UPGRADE_PLAN');
  if (!is_array($plan)) {
    return _provision_tasks_extra_upgrade_refuse('its checks did not run');
  }
  $uri = $plan['uri'];
  $copy = $plan['copy'];
  $copy_dir = $plan['source_root'] . '/sites/' . $copy;
  // This process holds the site as it was before the move: nothing of it
  // is saved when the command ends.
  drush_set_option('provision_save_config', FALSE);
  // The name is free now, so whatever holds it after leg 1 is this run's.
  clearstatcache();
  if (_provision_tasks_extra_upgrade_alias_root($copy) !== FALSE || file_exists($copy_dir) || is_link($copy_dir)) {
    return _provision_tasks_extra_upgrade_refuse('the name of the way-back copy, ' . $copy . ', is taken');
  }
  $started = time();
  $when = date('Y-m-d H:i');

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
    return _provision_tasks_extra_upgrade_stop($plan, 'the copy of the site could not be made (see above)', FALSE);
  }
  // Leg 1a: the Import the Clone queued for the copy, run now. Left to the
  // queue it would run on the switched-off copy and mark it enabled.
  $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'import');
  if (!$leg['ok']) {
    return _provision_tasks_extra_upgrade_stop($plan, 'the copy of the site could not be recorded in the panel (see above)', FALSE);
  }
  // Leg 1b.
  $leg = _provision_tasks_extra_upgrade_leg('@' . $copy, 'disable', array($plan['marker']));
  if (!$leg['ok']) {
    return _provision_tasks_extra_upgrade_stop($plan, 'the copy of the site could not be switched off (see above)', FALSE);
  }
  drush_log(dt('UPGRADE/1/COPY done: @copy holds the site as it was at @when, switched off.', array('@copy' => $copy, '@when' => $when)), 'ok');

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
    return drush_set_error('PROVISION_UPGRADE_UNFINISHED', dt('UPGRADE/RESULT Not finished: the move ended without saying which platform @uri is on (see above). Nothing was removed: the copy @copy is kept. Ask support.', array('@uri' => $uri, '@copy' => $copy)));
  }

  $kept = _provision_tasks_extra_upgrade_kept_dump($uri, $started);
  if ($kept !== '') {
    drush_log(dt('UPGRADE/KEPT the database as it was before the move is kept in @path, for support; the panel\'s Restore does not use it.', array('@path' => $kept)), 'ok');
  }
  $base = $uri . ' runs Drupal ' . $plan['target_release'] . ' on ' . $plan['target_title'];
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
    return drush_set_error('PROVISION_UPGRADE_UNFINISHED', dt('UPGRADE/RESULT Not finished: the move of @uri onto @target stopped midway (see above), and the site may not be served. This task removed nothing: what @source holds of the site is as the move left it, and the copy @copy is kept. Ask support.', array(
      '@uri' => $uri,
      '@target' => $plan['target_title'],
      '@source' => $plan['source_title'],
      '@copy' => $copy,
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

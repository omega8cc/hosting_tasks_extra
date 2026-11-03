<?php

/**
 * @file
 * Provision/Drush hooks for the provision_tasks_extra module.
 *
 * These are the hooks that will be executed by the drush_invoke function.
 */

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
    'description' => dt('Reports the available updates of a site. Read-only: it changes nothing in the site or its platform.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
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
 * Report only: nothing here writes to the site or its platform, it never locks
 * a platform's local Drush, and a report that cannot be made is a note in the
 * task log, never a failed task.
 */
function drush_provision_tasks_extra_provision_updates_report() {
  drush_errors_on();
  // These come first: drush_drupal_major_version() reads all of them as older
  // than Drupal 8, and they would inherit the Drupal 7 path.
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
  $major = drush_drupal_major_version(d()->root);
  if ($major < 8) {
    _provision_tasks_extra_updates_report_ups($major);
  }
  else {
    _provision_tasks_extra_updates_report_audit();
  }
}

/**
 * Drupal 6 and 7: Drush 8's own update backend.
 *
 * It asks the release feed directly, so the update module stays as it is,
 * enabled or not, and the site's database is not written to.
 */
function _provision_tasks_extra_updates_report_ups($major) {
  drush_log(dt('Available updates ###---### drush8 pm-updatestatus ###---### start'), 'notice');
  $output = array();
  $ran = _provision_tasks_extra_exec('drush8 ' . drush_escapeshellarg(d()->name) . ' ups --update-backend=drush --format=json 2>/dev/null', $output);
  $out = trim(implode("\n", $output));
  $doc = ($out === '') ? array() : json_decode($out, TRUE);
  if (!$ran || !is_array($doc)) {
    drush_log(dt('Available updates ###---### unknown: drush8 pm-updatestatus gave no report for this site.'), 'success');
    return;
  }
  $rows = array();
  $listed = 0;
  $security = 0;
  foreach ($doc as $project) {
    if (!is_array($project) || !isset($project['name'])) {
      continue;
    }
    $name = _provision_tasks_extra_audit_word($project['name']);
    $have = _provision_tasks_extra_audit_word(isset($project['existing_version']) ? $project['existing_version'] : '');
    $want = _provision_tasks_extra_audit_word(isset($project['candidate_version']) ? $project['candidate_version'] : '');
    $why = _provision_tasks_extra_audit_text(isset($project['status_msg']) ? $project['status_msg'] : '');
    // An extended-support core is newer than anything on drupal.org's feed, and
    // the feed then proposes a downgrade. That is no finding.
    if ($name === 'drupal' && $want !== '-' && version_compare($have, $want, '>')) {
      $rows[] = $name . ' ' . $have . ': newer than the release feed knows, an extended-support core takes its fixes with a BOA release';
      continue;
    }
    // 1 is DRUSH_UPDATESTATUS_NOT_SECURE. drupal.org marks every Drupal 7 project
    // unsupported, and Drush then stops short of that status, so the release's own
    // terms in the feed are read too.
    $terms = isset($project['releases'][$project['existing_version']]['terms']['Release type'])
      ? $project['releases'][$project['existing_version']]['terms']['Release type'] : array();
    $insecure = is_array($terms) && in_array('Insecure', $terms, TRUE);
    if ($insecure || (isset($project['status']) && (int) $project['status'] === 1)) {
      $security++;
      if ($insecure) {
        $why = 'SECURITY UPDATE available (the release feed marks ' . $have . ' insecure)';
      }
    }
    $listed++;
    $rows[] = $name . ' ' . $have . ' -> ' . $want . ': ' . $why;
  }
  if ($rows) {
    drush_log(implode("\n", $rows), 'notice');
  }
  if ((int) $major === 7) {
    drush_log(dt('Available updates ###---### drupal.org has published no Drupal 7 advisories since 5 January 2025, so no security flag means no published advisory, not a safe release.'), 'notice');
  }
  drush_log(dt('Available updates ###---### @count listed, @security of them security updates.', array(
    '@count' => $listed,
    '@security' => $security,
  )), 'success');
}

/**
 * Drupal 8 and newer: composer audit of the platform's lock.
 *
 * It works on a disposable copy, never in the platform: Composer runs a
 * composer.json's init and pre-command-run scripts even for audit, which would
 * run tenant code as this user, and a platform's config and repositories can
 * hide advisories. The copy's composer.json declares drupal.org's repository,
 * the only way its advisories reach composer audit.
 */
function _provision_tasks_extra_updates_report_audit() {
  drush_log(dt('Available updates ###---### composer audit --locked ###---### start'), 'notice');
  $app = '';
  $lock = FALSE;
  foreach (array(d()->root, dirname(d()->root)) as $dir) {
    $lock = _provision_tasks_extra_read_plain($dir . '/composer.lock');
    if ($lock !== FALSE) {
      $app = $dir;
      break;
    }
  }
  if ($app === '') {
    drush_log(dt('Available updates ###---### advisories: unknown (no composer.lock at the platform root)'), 'success');
    return;
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
  $scratch = drush_tempdir();
  $copy = json_encode(array(
    'name' => 'boa/advisory-audit',
    'repositories' => array(array('type' => 'composer', 'url' => 'https://packages.drupal.org/8')),
  ));
  if (file_put_contents($scratch . '/composer.json', $copy) === FALSE
    || file_put_contents($scratch . '/composer.lock', $lock) === FALSE) {
    drush_log(dt('Available updates ###---### advisories: unknown (cannot write the audit copy)'), 'success');
    return;
  }
  // A home of its own, so no global Composer config (ignore lists,
  // ignore-unreachable) reaches the audit. The exit code is not the verdict:
  // composer audit exits non-zero for findings and abandoned packages alike.
  putenv('COMPOSER_HOME=' . $scratch . '/.home');
  putenv('COMPOSER_CACHE_DIR=' . $scratch . '/.cache');
  putenv('COMPOSER_NO_INTERACTION=1');
  $output = array();
  $cwd = getcwd();
  if (chdir($scratch)) {
    _provision_tasks_extra_exec('composer audit --locked --format=json --no-plugins --no-scripts' . $dev . ' > .audit.json 2> .audit.err', $output);
    if ($cwd !== FALSE) {
      chdir($cwd);
    }
  }
  putenv('COMPOSER_HOME');
  putenv('COMPOSER_CACHE_DIR');
  putenv('COMPOSER_NO_INTERACTION');
  $lines = _provision_tasks_extra_audit_lines((string) @file_get_contents($scratch . '/.audit.json'), $lock, $dev !== '');
  if ($lines === FALSE) {
    drush_log(dt('Available updates ###---### advisories: unknown (composer audit did not finish: @why)', array(
      '@why' => _provision_tasks_extra_audit_why((string) @file_get_contents($scratch . '/.audit.err')),
    )), 'success');
    return;
  }
  $lines[] = ($dev !== '') ? '# require-dev: not installed in the platform, not audited' : '# require-dev: audited (installed, or no installed.json to tell)';
  drush_log(dt('Available updates ###---### read from @path: the whole platform, not only what this site installed.', array('@path' => $app . '/composer.lock')), 'notice');
  drush_log(implode("\n", $lines), 'notice');
  drush_log(dt('Available updates ###---### @first', array('@first' => $lines[0])), 'success');
}

/**
 * Runs a command line through /bin/sh, keeping its own redirections.
 *
 * drush_shell_exec() appends 2>&1 to every command, which rebinds whatever
 * stderr the line set and lands Drush's and Composer's messages in the JSON.
 * The leading word stays first so that websh dispatches it as usual.
 */
function _provision_tasks_extra_exec($command, &$output) {
  $rc = 1;
  exec($command, $output, $rc);
  return (int) $rc === 0;
}

/**
 * The bytes of a plain file in a codebase the account can write, or FALSE.
 *
 * This runs as the backend user on a tenant's tree: a link or a FIFO planted
 * under the name is never followed, and the size is bounded.
 */
function _provision_tasks_extra_read_plain($path, $max = 33554432) {
  if (is_link($path) || !is_file($path)) {
    return FALSE;
  }
  $size = filesize($path);
  if ($size === FALSE || $size > $max) {
    return FALSE;
  }
  return @file_get_contents($path);
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
  return substr(trim(preg_replace('/[^\x20-\x7e]+/', ' ', $why)), 0, 200);
}

/**
 * Strips a field to printable non-space ASCII: it comes from a remote feed.
 */
function _provision_tasks_extra_audit_word($s) {
  $s = preg_replace('/[^\x21-\x7e]/', '', (string) $s);
  return $s === '' ? '-' : $s;
}

/**
 * Strips a field to one line of printable ASCII: it comes from a remote feed.
 */
function _provision_tasks_extra_audit_text($s) {
  $s = trim(preg_replace('/[^\x20-\x7e]+/', ' ', (string) $s));
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
    $st = isset($c['status']) ? (string) $c['status'] : '';
    if ($st === 'covered') {
      return '';
    }
    return _provision_tasks_extra_audit_text(isset($c['message']) && is_string($c['message']) && $c['message'] !== '' ? $c['message'] : 'coverage status: ' . $st);
  }
  $t = isset($p['type']) ? (string) $p['type'] : '';
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
        $name = isset($x['packageName']) ? $x['packageName'] : $pkg;
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
        if (isset($p['name'], $p['version'])) {
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

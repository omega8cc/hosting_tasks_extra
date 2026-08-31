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
  $items['provision-update_translations'] = array(
    'description' => dt('Update translations on a site'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['provision-flush_drush_cache'] = array(
    'description' => 'Flushes the Drush cache on the server',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  $items['provision-site_health_check'] = array(
    'description' => 'Runs health check on a site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH
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
 * Implements the provision-site_health_check command.
 */
function drush_provision_tasks_extra_provision_site_health_check() {
  drush_errors_on();
  // SECURITY: d()->name (Drush alias name) flows from the Hostmaster site
  // node title, which is not validated against shell-unsafe characters at
  // the UI layer. These two values are interpolated into drush_shell_exec()
  // calls below, so escape them at the definition site. They are not used
  // anywhere else in this function. See DECISIONS.md Decision 002.
  // (string) casts guard against PHP 8.1+ null-to-escapeshellarg
  // deprecation in the unlikely event preg_replace() returns null on
  // a regex compilation edge case.
  $sitename = escapeshellarg((string) d()->name);
  $dnameten = escapeshellarg((string) preg_replace('/\.(?![^.]*$)/', '-', d()->name));
  $dmv = DRUSH_MAJOR_VERSION;
  drush_log(dt("This is Drush major version: %dmv", array('%dmv' => $dmv)), 'notice');

  if (drush_drupal_major_version(d()->root) < 8) {
    drush_shell_exec("drush8 $sitename en update -y");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
    drush_log(dt('Health Check ###---### update module enabled temporarily'), 'success');

    drush_log(dt('Health Check ###---### drush8 clean-modules ###---### start'), 'notice');
    drush_shell_exec("drush8 $sitename clean-modules -y 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');

    drush_log(dt('Health Check ###---### drush8 pm-updatestatus ###---### start'), 'notice');
    drush_shell_exec("drush8 $sitename ups --field-labels 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }
  else {
    drush_log(dt('Health Check ###---### drush11 pm:security ###---### start'), 'notice');
    drush_shell_exec("drush11 $dnameten pm:security 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }

  if (drush_drupal_major_version(d()->root) < 8) {
    drush_log(dt('Health Check ###---### drush8 status-report ###---### start'), 'notice');
    drush_shell_exec("drush8 $sitename status-report 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }
  else {
    drush_log(dt('Health Check ###---### drush11 status ###---### start'), 'notice');
    drush_shell_exec("drush11 $dnameten status 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }

  if (drush_drupal_major_version(d()->root) < 8) {
    drush_log(dt('Health Check ###---### drush8 updatedb-status ###---### start'), 'notice');
    drush_shell_exec("drush8 $sitename updatedb-status 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }
  else {
    drush_log(dt('Health Check ###---### drush11 updatedb:status ###---### start'), 'notice');
    drush_shell_exec("drush11 $dnameten updatedb:status 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }

  if (drush_drupal_major_version(d()->root) == 7) {
    drush_log(dt('Health Check ###---### drush8 security-review ###---### start'), 'notice');
    drush_shell_exec("drush8 $sitename secrev --check=input_formats,field,error_reporting,query_errors,failed_logins,upload_extensions,admin_permissions,untrusted_php,temporary_files 2>&1");
    $array = drush_shell_exec_output();
    drush_log(dt($array), 'notice');
  }

  drush_log(dt('Health Check ###---### FIN'), 'success');

  provision_file()->chmod(d()->site_path, 0755)
    ->succeed('Changed permissions of <code>@path</code> to @perm')
    ->fail('Could not change permissions of <code>@path</code> to @perm');
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
  if (drush_drupal_major_version(d()->root) <= 7) {
    provision_backend_invoke(d()->name, 'cache-clear all');
    provision_backend_invoke(d()->name, 'cache-clear all');
  }
  elseif (drush_drupal_major_version(d()->root) >= 10 && function_exists('_provision_drupal_rebuild_d8plus')) {
    // D10/D11 cannot be bootstrapped by Aegir's own Drush 8, so the
    // 'cache-rebuild' invoke below silently no-ops there. Use the shared
    // provision helper, which runs 'cr' via the site-local Drush. Non-fatal.
    _provision_drupal_rebuild_d8plus();
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
  // No registry and no service container on a Grav site; the frontend hides
  // the task there, and this guard keeps a directly-invoked run honest
  // instead of misrouting through the Drupal version probe.
  if (function_exists('provision_platform_is_grav')
    && provision_platform_is_grav(d())) {
    drush_log(dt('Registry rebuild does not apply to Grav sites; nothing to do.'), 'warning');
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
      // and flushing all caches (drush cr). The shared provision helper runs it
      // via the site-local Drush on D10+ (where Aegir Drush 8 cannot bootstrap
      // the site) and via Aegir Drush 8 'cache-rebuild' on D8/D9. Non-fatal.
      _provision_drupal_rebuild_d8plus();
      drush_log(dt('D8+ service container and all caches rebuilt'));
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
  /* Set offline mode to true and re-generate the settings.php. This will write a
   * $conf['site_offline'] = 1; to the settings.php
   */

  $list = drush_commandfile_list();
  $provision_dir = dirname($list['provision']);
  include_once($provision_dir . '/platform/backup.provision.inc');

  drush_log(dt("Putting the site under maintenance"));
  d()->site_enabled = FALSE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();

  $backup_file = drush_provision_drupal_provision_backup_suggest_filename();
  drush_set_option('pre_update_backup_file', $backup_file);

  drush_set_option('force', TRUE);
  $success = drush_invoke_process('@self', 'provision-backup', array($backup_file));
  if (!$success) {
    // If the backup failed, we don't want to try restoring it in the rollback.
    drush_set_option('pre_update_backup_file', FALSE);
  }
}

/**
 * Implements drush_hook_COMMAND_rollback().
 */
function drush_provision_tasks_extra_provision_update_rollback() {
  $restore_file = drush_get_option('pre_update_backup_file', FALSE);

  if ($restore_file) {
    drush_set_option('force', TRUE);
    drush_invoke_process('@self', 'provision-restore', array($restore_file));
    drush_unset_option('force');
  }

  // Set site_offline to false and regenerate the config
  drush_log(dt("Bringing the site out of maintenance"));
  d()->site_enabled = TRUE;
  _provision_drupal_create_settings_file();
  provision_drupal_push_site();
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
  $success = drush_invoke_process('@self', 'provision-backup', array($backup_file));
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

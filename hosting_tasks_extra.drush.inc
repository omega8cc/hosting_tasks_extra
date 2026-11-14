<?php

/**
 * Implements drush_hook_pre_hosting_task()
 *
 * Runs before the command "hosting-task" is run. Passes "task_args" from
 * the task node into "$task->options" which become drush options.
 */
function drush_hosting_tasks_extra_pre_hosting_task() {
  $task =& drush_get_context('HOSTING_TASK');
  if ($task->task_type == 'features_update_all' || $task->task_type == 'features_revert_all') {
    $task->options['features'] = $task->task_args['features'];
    $task->options['revert'] = $task->task_args['revert'];
    $task->options['force_revert'] = $task->task_args['force_revert'];
  }
  if ($task->task_type == 'export_backup' || $task->task_type == 'remove_export_backup') {
    hosting_tasks_extra_export_path($task);
  }
  if ($task->ref->type == 'site' && $task->task_type == 'drupal_upgrade') {
    hosting_tasks_extra_drupal_upgrade_start($task);
  }
  // The Upgrade's move: a Migrate that is undone when the database updates
  // do not finish or a page no longer answers as it did.
  if ($task->ref->type == 'site' && $task->task_type == 'migrate' && !empty($task->task_args['upgrade_strict'])) {
    $task->options['upgrade_strict'] = 1;
  }
}

/**
 * Refuses an Upgrade when its task starts, before the backend is asked.
 */
function hosting_tasks_extra_drupal_upgrade_refuse($why) {
  return drush_set_error('HOSTING_UPGRADE_NOT_STARTED', dt('UPGRADE/RESULT Not started: !why. Nothing was changed.', array('!why' => str_replace(array('<', '>'), '', $why))));
}

/**
 * Checks an Upgrade once more as it starts and tells the backend what to run.
 *
 * The row may have been queued without the form, so nothing the form
 * checked is taken as checked; and the question is asked for the user who
 * queued the task, not for the runner.
 */
function hosting_tasks_extra_drupal_upgrade_start($task) {
  $site = $task->ref;
  $target_nid = isset($task->task_args['target_platform']) ? (int) $task->task_args['target_platform'] : 0;
  if (function_exists('hosting_panel_is_moving') && hosting_panel_is_moving()) {
    return hosting_tasks_extra_drupal_upgrade_refuse('the control panel is moving to its new version; start again in a few minutes');
  }
  // A standby copy of a server takes no writes of its own.
  if (db_driver() === 'mysql' && (int) db_query('SELECT @@global.read_only')->fetchField() === 1) {
    return hosting_tasks_extra_drupal_upgrade_refuse('this server is a standby copy, where sites are not changed');
  }
  $account = user_load($task->uid);
  if (!is_object($account) || empty($account->status)) {
    return hosting_tasks_extra_drupal_upgrade_refuse('the user who queued this task is not known or is blocked');
  }
  if ($target_nid < 1) {
    return hosting_tasks_extra_drupal_upgrade_refuse('the task names no platform to upgrade onto');
  }
  if (isset($task->task_args['new_db_server']) && $task->task_args['new_db_server'] != $site->db_server) {
    return hosting_tasks_extra_drupal_upgrade_refuse('an Upgrade keeps the site on its database server, and this task names another one');
  }
  $busy = hosting_tasks_extra_drupal_upgrade_busy($site->nid, $task->nid);
  if ($busy !== '') {
    return hosting_tasks_extra_drupal_upgrade_refuse('a ' . $busy . ' task is waiting or running on this site');
  }
  $entry = hosting_tasks_extra_drupal_upgrade_target($site, $account, $target_nid, TRUE);
  if (!$entry['ready']) {
    return hosting_tasks_extra_drupal_upgrade_refuse(($entry['title'] !== '' ? $entry['title'] . ': ' : '') . $entry['why']);
  }
  // The copy's name: nothing but a site or an alias that holds it can
  // refuse it, and a nested task checks no name on its own.
  $copy = hosting_site_clean_domain('back-' . $task->nid . '.temp.' . hosting_tasks_extra_drupal_upgrade_site_name($site));
  if (!_hosting_valid_fqdn($copy) || !hosting_domain_allowed($copy) || hosting_get_site_by_url($copy)) {
    return hosting_tasks_extra_drupal_upgrade_refuse('the name the way-back copy would get, ' . $copy . ', is not valid or is taken');
  }
  $source = node_load($site->platform);
  $target = node_load($target_nid);
  $tasks = hosting_available_tasks('site');
  // Kept on the row for what comes after: the platform the site left.
  $task->task_args['source_platform'] = (int) $site->platform;
  $task->options['upgrade_target'] = hosting_context_name($target_nid);
  $task->options['upgrade_target_nid'] = $target_nid;
  $task->options['upgrade_source_nid'] = (int) $site->platform;
  $task->options['upgrade_db_nid'] = (int) $site->db_server;
  $task->options['upgrade_task_nid'] = (int) $task->nid;
  $task->options['upgrade_copy'] = $copy;
  $task->options['upgrade_source_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $source->title);
  $task->options['upgrade_target_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $target->title);
  $task->options['upgrade_copy_secs'] = hosting_tasks_extra_drupal_upgrade_copy_seconds($site->nid);
  $task->options['upgrade_ceiling'] = (int) variable_get('hosting_task_runner_lock_max_hold', 28800);
  if (isset($tasks['drupal_upgrade_return'])) {
    $task->options['upgrade_return'] = 1;
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_tasks_extra_post_hosting_drupal_upgrade_task($task, $data) {
  hosting_tasks_extra_drupal_upgrade_end($task);
}

/**
 * Implements hook_hosting_TASK_TYPE_task_rollback().
 */
function hosting_tasks_extra_hosting_drupal_upgrade_task_rollback($task, $data) {
  hosting_tasks_extra_drupal_upgrade_end($task);
}

/**
 * Tidies the panel's records after an Upgrade, however it ended.
 *
 * The site node is read again and never saved from the task's own copy of
 * it, which is as it was before the move. A step that was refused before it
 * ran leaves a queued row the queue would run alone later: such rows are
 * cancelled. Where the site moved, the two backups the run registered say
 * what they hold.
 */
function hosting_tasks_extra_drupal_upgrade_end($task) {
  $rows = db_query("SELECT DISTINCT t.nid FROM {hosting_task} t
      INNER JOIN {node} n ON n.vid = t.vid
      INNER JOIN {hosting_task_arguments} a ON a.vid = t.vid
    WHERE a.name = :name AND a.value = :run AND t.task_status = :queued AND t.nid <> :self", array(
      ':name' => 'upgrade_run',
      ':run' => (string) $task->nid,
      ':queued' => HOSTING_TASK_QUEUED,
      ':self' => $task->nid,
    ))->fetchCol();
  foreach ($rows as $nid) {
    $left = node_load($nid, NULL, TRUE);
    if (is_object($left) && $left->type == 'task' && $left->task_status == HOSTING_TASK_QUEUED) {
      $left->task_status = HOSTING_TASK_WARNING;
      node_save($left);
      hosting_task_log($left->vid, 'warning', t('Task was cancelled: the Upgrade it was a step of has ended.'));
    }
  }
  $site = node_load($task->rid, NULL, TRUE);
  $target = isset($task->task_args['target_platform']) ? (int) $task->task_args['target_platform'] : 0;
  if (!is_object($site) || !$target || $site->platform != $target || empty($task->executed)) {
    return;
  }
  $tasks = hosting_available_tasks('site');
  // Each as the nested task stored it: its text, or that text translated.
  // A backup of this run is stamped by a process started after this task
  // was: nothing older is one of them.
  $labels = array(
    array(
      array('Pre-clone backup', t('Pre-clone backup')),
      t('Full copy before the upgrade, @date (files and database)', array('@date' => format_date($task->executed, 'custom', 'Y-m-d H:i'))),
    ),
    array(
      array('Pre-migration backup', t('Pre-migration backup')),
      isset($tasks['drupal_upgrade_return'])
        ? t('Taken during the upgrade: no database and no uploaded files. To undo the upgrade, use Go back')
        : t('Taken during the upgrade: no database and no uploaded files'),
    ),
  );
  foreach ($labels as $label) {
    db_update('hosting_site_backups')
      ->fields(array('description' => $label[1]))
      ->condition('site', $site->nid)
      ->condition('timestamp', (int) $task->executed, '>=')
      ->condition('description', $label[0], 'IN')
      ->execute();
  }
}

/**
 * Sends a backup export where the panel will look for it.
 *
 * On a box that keeps exports in a private folder the panel's variable names
 * it, and the backend has no other way to learn it. The folder is made here
 * when a move left only the variable: Provision would open a missing parent to
 * the group, and the parent must not list for anyone else.
 */
function hosting_tasks_extra_export_path($task) {
  $path = variable_get('aegir_backup_export_path', '');
  if (!preg_match('#/backup-exports/[0-9a-f]{32}$#', $path)) {
    return;
  }
  $parent = dirname($path);
  if (!is_dir($parent)) {
    @mkdir($parent, 0751);
    @chmod($parent, 0751);
  }
  if (!is_dir($path)) {
    @mkdir($path, 0711);
    @chmod($path, 0711);
  }
  $task->options['provision_backup_export_path'] = $path;
}

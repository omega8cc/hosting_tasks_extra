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

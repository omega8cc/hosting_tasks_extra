<?php

/**
 * @file hosting_sync.drush.inc
 * Keeping things organized.  All code that is really run via drush commands
 * will go here.
 */

/**
 * Implements drush_HOOK_pre_COMMAND()
 *
 * This runs for each tasks during the command
 *    drush @hostmaster hosting-tasks
 *
 * NOTE: This ONLY runs when being called from a hostmaster task.
 * This hook should ONLY be used to pass Options from a hostmaster task form to
 * the $task object, or if you don't need this functionality from the command
 * line.
 */
function drush_hosting_sync_pre_hosting_task() {
  $task =& drush_get_context('HOSTING_TASK');

  // Sync
  if ($task->ref->type == 'site' && $task->task_type == 'sync') {
    $task->args[] = $task->task_args['source'];
    $task->args[] = $task->task_args['destination'];
    $task->options['database'] = $task->task_args['database'];
    $task->options['files'] = $task->task_args['files'];
    $task->options['updatedb'] = $task->task_args['updatedb'];
    $task->options['features-revert-all'] = $task->task_args['features_revert_all'];
    $task->options['cache-clear'] = $task->task_args['cache_clear'];
  }
}


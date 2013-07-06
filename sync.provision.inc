<?php

/**
 * Implements hook_TASK_NAME_validate().
 */
function drush_provision_sync_validate($source = NULL, $destination = NULL) {
  // Fail if no source
  if (empty($source) && !($source = drush_get_option('source'))) {
    return drush_set_error('DRUSH_TARGET_NOT_FOUND', dt('Source not found. You must enter a source alias to sync from'));
  }

  // Fail if no destination
  if (empty($destination) && !($destination = drush_get_option('destination'))) {
    // Default to the current site if no destination is given
    $destination = drush_get_option('#name');
    if (empty($destination) || !drush_get_option('db_name')) {
      return drush_set_error('DRUSH_TARGET_NOT_FOUND', dt('Destination not found. You must enter a destination alias to sync to.'));
    }
  }

  $source = '@' . ltrim($source, '@');
  $destination = '@' . ltrim($destination, '@');

  // @todo: validate that these aliases exist, and that we can connect to their databases!

  drush_set_option('source', $source);
  drush_set_option('destination', $destination);
}

/**
 * Implements the provision-sync command.
 *
 * Now expects environment names for source and destination
 */
function drush_provision_sync($source = NULL, $destination = NULL) {
  $source = drush_get_option('source', $source);
  $destination = drush_get_option('destination', $destination);

  $source_site = ltrim($source, '@');
  $destination_site = ltrim($destination, '@');

  // Make a backup before making any changes.
  $result = provision_backend_invoke($destination, 'provision-backup');
  if ($result['error_status'] !== DRUSH_SUCCESS) {
    return drush_set_error('PROVISION_BACKUP_FAILED', dt("Unable to proceed with sync because backup of destination failed."));
  }
  drush_set_option('backup_file', $result['context']['backup_file']);
  drush_set_option('backup_file_size', $result['context']['backup_file_size']);

  foreach (drush_command_implements('provision_sync_before') as $command) {
    $func = $command . '_provision_sync_before';
    if ($func($source_settings, $destination_settings) === FALSE) {
      // Allow the before hook to abort the sync!
      return;
    }
  }

  if (drush_get_option('database')) {
    // Drop the database
    drush_log(dt('Dropping tables for destination database (!dest)', array('!dest' => $destination)), 'ok');
    provision_backend_invoke($destination, 'sql-drop');
    
    // Sync the databases
    drush_log(dt('Syncing databases...'), 'ok');
    // @todo: update to drush_invoke_process('@self', ...)
    $result = drush_backend_invoke('sql-sync', array($source, $destination));
    if ($result['error_status'] !== DRUSH_SUCCESS) {
      return drush_set_error('PROVISION_SQL_SYNC_FAILED', dt("Unable to sync from @source to @destination.", array($source, $destination)));
    }
  }
  else {
    drush_log(dt('Skipped syncing databases...'), 'ok');
  }

  // Sync files via rsync
  if (drush_get_option('files')) {
    drush_log(dt('Syncing file contents from !source to !destination', array('!source' => $source, '!destination' => $destination)), 'ok');
    
    // Sync the files
    // @todo: should include the private files too!
    // @todo: update to drush_invoke_process('@self', ...)
    drush_backend_invoke('rsync', array("$source:sites/$source_site/files/", "$destination:sites/$destination_site/files/"));
  }
  else {
    drush_log(dt('Skipped syncing files...'), 'ok');
  }

  // @todo: Debugging! Remove this line.
  //return drush_set_error('DEBUG', dt("Error so we can roll back!"));

  // update db, unless option is false.
  if (drush_get_option('updatedb')) {
    drush_log(dt('Updating database...'), 'ok');
    provision_backend_invoke($destination, 'updb');
  }
  else {
    drush_log(dt('Skipped updating database...'), 'ok');
  }

  // Revert All Features, unless option is false
  if (drush_get_option('features-revert-all')) {
    drush_log(dt('Reverting all features...'), 'ok');
    provision_backend_invoke($destination, 'features-revert-all');
  }
  else {
    drush_log(dt('Skipped reverting all features...'), 'ok');
  }

  // Clear the whole cache, unless option is false
  // Seriously, lets do this twice.  Go Drupal!
  if (drush_get_option('cache-clear')) {
    drush_log(dt('Clearing all caches...'), 'ok');
    provision_backend_invoke($destination, 'cc all');
    provision_backend_invoke($destination, 'cc all');
  }
  else {
    drush_log(dt('Skipped clearing all caches...'), 'ok');
  }

  drush_command_invoke_all('provision_sync_after', $source_settings, $destination_settings);
}

/**
 * Rollback in the case of failure.
 */
function drush_provision_sync_rollback($source = NULL, $destination = NULL) {
  $source = drush_get_option('source', $source);
  $destination = drush_get_option('destination', $destination);

  $backup_file = drush_get_option('backup_file');
  if (empty($backup_file)) {
    return drush_set_error('ROLLBACK_FAILED', dt('Unable to rollback because backup file could not be found.'));
  }

  // Attempt to rollback
  drush_log(dt('Restoring from backup made before sync.'));
  $result = provision_backend_invoke($destination, 'provision-restore', array($backup_file));

  // Remove the backup made before the restore -- we don't ever need it!
  unlink($result['context']['backup_file']);

  // Clean-up
  drush_provision_sync_post_provision_sync($source, $destination);

  if ($result['error_status'] !== DRUSH_SUCCESS) {
    return drush_set_error('ROLLBACK_FAILED', dt('Unable to rollback because we failed to restore from backup.'));
  }
}

/**
 * After the command is complete.
 */
function drush_provision_sync_post_provision_sync($source = NULL, $destination = NULL) {
  // If the user didn't want a backup, we discard it.
  if (!drush_get_option('backup')) {
    unlink(drush_get_option('backup_file'));
    drush_set_option('backup_file', NULL);
    drush_set_option('backup_file_size', NULL);
  }
}


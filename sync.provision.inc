<?php

/**
 * Implements hook_TASK_NAME_validate().
 *
 * @todo: Do we really need this?
 */
function drush_provision_sync_validate($source = NULL, $destination = NULL) {
  drush_set_arguments(array($source, $destination));
}

/**
 * Implements the provision-sync command.
 *
 * Now expects environment names for source and destination
 */
function drush_provision_sync($source = NULL, $destination = NULL) {
  // Fail if no source
  if (empty($source) && !($source = drush_get_option('source'))) {
    return drush_set_error('DRUSH_TARGET_NOT_FOUND', dt('Source not found.  You must enter a source alias to sync from'));
  }

  // Fail if no destination
  if (empty($destination) && !($destination = drush_get_option('destination'))) {
    return drush_set_error('DRUSH_TARGET_NOT_FOUND', dt('Destination not found.  You must enter a source alias to sync to.'));
  }

  // @todo: validate that these aliases exist, and that we can connect to their databases!

  $source_site = ltrim($source, '@');
  $source_alias = '@' . $source_site;

  $destination_site = ltrim($destination, '@');
  $destination_alias = '@' . $destination_site;

  if (drush_get_option('backup')) {
    // Make a backup before making any changes.
    // @todo: How can we tell if the backup failed? Is there a way to pass this stuff through automatically?
    $result = provision_backend_invoke($destination_alias, 'provision-backup');
    drush_set_option('backup_file', $result['context']['backup_file']);
    drush_set_option('backup_file_size', $result['context']['backup_file_size']);
    // @todo: We should rollback in the case of failure to fully sync!
  }

  foreach (drush_command_implements('provision_sync_before') as $command) {
    $func = $command . '_provision_sync_before';
    if ($func($source_settings, $destination_settings) === FALSE) {
      // Allow the before hook to abort the sync!
      return;
    }
  }

  if (drush_get_option('database')) {
    // Drop the database
    drush_log(dt('Dropping tables for destination database (!dest)', array('!dest' => $destination_alias)), 'ok');
    provision_backend_invoke($destination_alias, 'sql-drop');
    
    // Sync the databases
    drush_log(dt('Syncing databases...'), 'ok');
    // @todo: update to drush_invoke_process('@self', ...)
    drush_backend_invoke('sql-sync', array($source_alias, $destination_alias));
  }
  else {
    drush_log(dt('Skipped syncing databases...'), 'ok');
  }

  // Sync files via rsync
  if (drush_get_option('files')) {
    drush_log(dt('Syncing file contents from !source to !destination', array('!source' => $source_alias, '!destination' => $destination_alias)), 'ok');
    
    // Sync the files
    // @todo: should include the private files too!
    // @todo: update to drush_invoke_process('@self', ...)
    drush_backend_invoke('rsync', array("$source_alias:sites/$source_site/files/", "$destination_alias:sites/$destination_site/files/"));
  }
  else {
    drush_log(dt('Skipped syncing files...'), 'ok');
  }

  // update db, unless option is false.
  if (drush_get_option('updatedb')) {
    drush_log(dt('Updating database...'), 'ok');
    provision_backend_invoke($destination_alias, 'updb');
  }
  else {
    drush_log(dt('Skipped updating database...'), 'ok');
  }

  // Revert All Features, unless option is false
  if (drush_get_option('features-revert-all')) {
    drush_log(dt('Reverting all features...'), 'ok');
    provision_backend_invoke($destination_alias, 'features-revert-all');
  }
  else {
    drush_log(dt('Skipped reverting all features...'), 'ok');
  }

  // Clear the whole cache, unless option is false
  // Seriously, lets do this twice.  Go Drupal!
  if (drush_get_option('cache-clear')) {
    drush_log(dt('Clearing all caches...'), 'ok');
    provision_backend_invoke($destination_alias, 'cc all');
    provision_backend_invoke($destination_alias, 'cc all');
  }
  else {
    drush_log(dt('Skipped clearing all caches...'), 'ok');
  }

  drush_command_invoke_all('provision_sync_after', $source_settings, $destination_settings);
}


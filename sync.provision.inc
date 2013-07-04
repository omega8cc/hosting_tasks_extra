<?php

require_once DRUSH_BASE_PATH . '/commands/sql/sync.sql.inc';

/**
 * Implements drush_hook_init
 * drush_sql_sync_init() does what we want!
 */
function drush_provision_sync_init($source = NULL, $destination = NULL) {
  return drush_sql_sync_init($source, $destination);
}

/**
 * Pre provision-sync
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

  $source_settings = drush_sitealias_get_record($source);
  $source_alias = $source_settings['#name'];
  $source_site = $source_settings['uri'];

  $destination_settings = drush_sitealias_get_record($destination);
  $destination_alias = $destination_settings['#name'];
  $destination_site = $destination_settings['uri'];

  // @todo: we should optionally run the provision backups!

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
    provision_backend_invoke($destination_alias, 'sql-drop');
    
    // Sync the databases
    drush_log(dt('Syncing databases...'), 'ok');
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


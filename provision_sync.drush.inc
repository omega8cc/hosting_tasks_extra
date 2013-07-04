<?php

/**
 * @file
 * Drush commands for Provision Sync
 */

/**
 * Implementation of hook_drush_command().
 * Provides provision commands.
 */
function provision_sync_drush_command() {
  $items = array();
  $items['provision-sync'] = array(
    'description' => 'Sync database and files from a chosen source site.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'options' => array(
      'update' => 'Run update.php after content sync.',
      'revert' => 'Revert all features after content sync.',
      'cache' => 'Clear all caches after content sync.',
      'files' => 'Sync site files.',
    ),
    'arguments' => array(
      'from' => 'Environment to sync from.',
      'to' => 'Environment to sync to.',
    ),
    'examples' => array(
      'drush @project_mysite provision-sync live dev' => 'Syncs the database from @live to @dev.',
    ),
    'aliases' => array('ps'),
  );
  return $items;
}


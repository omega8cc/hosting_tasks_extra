<?php

/**
 * @file
 *   The hosting feature definition for the migration source task.
 */

/**
 * Register a hosting feature with Aegir.
 *
 * @return
 *  associative array indexed by feature key.
 */
function hosting_migrate_source_hosting_feature() {
  $features['migrate_source'] = array(
    'title' => t('Migration source'),
    'description' => t('Lets a site owner give one of their sites read-only access to another of their sites database, for Drupal migration tooling.'),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'hosting_migrate_source',
    'group' => 'advanced',
    // role_permissions only apply at feature-enable time; hosting_migrate_source
    // ships hook_update_N() as well so already-installed instances get the same
    // grants. Keep the two lists identical.
    'role_permissions' => array(
      'aegir client' => array(
        'create migrate_source task',
      ),
      'aegir platform manager' => array(
        'create migrate_source task',
      ),
    ),
  );
  return $features;
}

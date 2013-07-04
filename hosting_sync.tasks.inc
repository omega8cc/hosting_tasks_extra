<?php

/**
 * @file hosting_sync.tasks.inc
 *
 * Implements a new Aegir task.
 */

/**
 * Implementation of hook_hosting_tasks()
 */
function hosting_sync_hosting_tasks() {
  $tasks = array();
  $tasks['site']['sync'] = array(
    'title' => t('Sync'),
    'description' => t('Synchronize database and files from another site and (optionally) run update.php, clear cache, and revert features.'),
    'access callback' => 'hosting_sync_hosting_task_menu_access',
    'dialog' => TRUE,
  );
  return $tasks;
}

/**
 * Helper to determine if a particular site has a module installed.
 */
function _hosting_sync_site_has_module($node, $module) {
  $package = hosting_package_instance_load(array('rid' => $node->nid, 'p.short_name' => $module));
  return $package->status;
}

/**
 * Helper to get the list of sites.
 *
 * Similar to hosting_get_sites_by_status() but it calls db_rewrite_sql() so that the
 * node access rules are applied (adding support for the 'hosting_client' module).
 */
function _hosting_sync_get_sites($platform = NULL, $profile = NULL, $exclude_nid = NULL, $status = HOSTING_SITE_ENABLED) {
  $sql = "SELECT n.nid FROM {node} n LEFT JOIN {hosting_site} s ON n.vid = s.vid WHERE n.type = 'site' AND s.status = %d";
  $args = array($status);

  if (!is_null($platform)) {
    $sql .= " AND s.platform = %d";
    $args[] = $platform;
  }

  if (!is_null($profile)) {
    $sql .= " AND s.profile = %d";
    $args[] = $profile;
  }

  if (!is_null($exclude_nid)) {
    $sql .= " AND n.nid != %d";
    $args[] = $exclude_nid;
  }

  $result = db_query(db_rewrite_sql($sql), $args);

  $nodes = array();
  while ($nid = db_fetch_object($result)) {
    $nodes[$nid->nid] = node_load($nid->nid);
  }
  return $nodes;
}

/**
 * Implementation of hook_hosting_task_TASK_TYPE_form().
 *
 * For "Sync Content" task.
 */
function hosting_task_sync_form($node) {
  $form = array();

  // @todo: we should be able to allow sites with the same profile on different platforms
  //        but then we'll need a check like the migrate task does, where it makes sure
  //        all the modules are available there...

  // Get a list of the sites on the same platform and profile, removing the the current
  // site itself and allowing other modules to modify this list.
  $sites = _hosting_sync_get_sites($node->platform, $node->profile, $node->nid);
  drupal_alter('hosting_sync_destinations', $sites);

  if (count($sites) == 0) {
    $form['destination'] = array(
      '#type' => 'item',
      '#title' => t('Destination'),
      '#value' => t('No destinations are currently available.'),
    );
    return $form;
  }

  $form['source'] = array(
    '#type' => 'value',
    '#value' => '@' . $node->hosting_name,
  );

  $options = array();
  foreach ($sites as $site) {
    $options['@' . $site->hosting_name] = $site->title;
  }

  $form['destination'] = array(
    '#type' => 'radios',
    '#title' => t('Destination'),
    '#options' => $options,
    '#required' => TRUE,
    '#description' => t('Choose the destination site.  <em>Data from this environment will be DESTROYED.</em>'),
  );

  $form['database'] = array(
    '#title' => t('Copy database from source to destination.'),
    '#type' => 'checkbox',
    '#default_value' => 1,
    '#prefix' => '<fieldset><legend>What to sync</legend>',
  );

  $form['files'] = array(
    '#title' => t('Copy files folder from source to destination.'),
    '#type' => 'checkbox',
    '#default_value' => 0,
    '#suffix' => '</fieldset>',
  );

  $form['updatedb'] = array(
    '#title' => t('Run update.php on Destination'),
    '#type' => 'checkbox',
    '#default_value' => 1,
  );
  if (_hosting_sync_site_has_module($node, 'features')){
    $form['features_revert_all'] = array(
      '#title' => t('Revert all features on Destination'),
      '#type' => 'checkbox',
      '#default_value' => $has_features,
      '#access' => $has_features,
    );
  } else{
    // I think its better UI just to hide it? If they need features it will be enabled!
    //$form['actions']['revert'] = array(
    //  '#title' => t('<em>Revert all features on Destination after content sync?</em>'),
    //  '#type' => 'checkbox',
    //  '#description' => t('This site doesn\'t have features.module enabled. Please enable and then "Verify" the site.'),
    //  '#default_value' => FALSE,
    //  '#disabled' => TRUE,
    //  );
  }
  $form['cache_clear'] = array(
    '#title' => t('Clear cache on Destination'),
    '#type' => 'checkbox',
    '#default_value' => 1,
    '#suffix' => '</fieldset>',
  );
  $form['#validate'][] = 'hosting_task_sync_form_validate';
  return $form;
}

/**
 * Validation for hosting_task_hosting_sync_form()
 */
function hosting_task_sync_form_validate(&$form, &$form_state){
  // Can't sync to self
  if ($form_state['values']['parameters']['source'] == $form_state['values']['parameters']['destination']){
    form_set_error('destination', t('The source cannot be the same as the destination.'));
  }
}


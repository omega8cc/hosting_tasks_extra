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
 * Implementation of hook_hosting_task_TASK_TYPE_form().
 *
 * For "Sync Content" task.
 */
function hosting_task_sync_form($node) {
  $form = array();

  $form['source'] = array(
    '#type' => 'value',
    '#value' => '@' . $node->hosting_name,
  );

  // Get a list of the sites on the current platform (filtering out the current site) and
  // allowing other modules to modify this list.
  $sites = hosting_get_sites_by_status($node->platform, HOSTING_SITE_ENABLED);
  foreach ($sites as $index => $site) {
    if ($site->nid == $node->nid) {
      unset($sites[$index]);
    }
  }
  drupal_alter('hosting_sync_destinations', $sites);

  // Convert to a format that works for the Form API
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

  $form['source']['#prefix'] = '<div class="sync-envs"><div class="source">';
  $form['source']['#suffix'] = '</div>';
  $form['destination']['#prefix'] = '<div class="destination">';
  $form['destination']['#suffix'] = '</div></div>';

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


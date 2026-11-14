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
  if ($task->ref->type == 'site' && $task->task_type == 'drupal_upgrade_return') {
    hosting_tasks_extra_drupal_upgrade_return_start($task);
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
  // Kept on the row for what comes after: the platform the site left. It
  // is written now and not only by the save that ends the run, so a runner
  // that dies after the move does not lose it.
  $task->task_args['source_platform'] = (int) $site->platform;
  if (!empty($task->vid)) {
    db_delete('hosting_task_arguments')
      ->condition('vid', $task->vid)
      ->condition('name', 'source_platform')
      ->execute();
    db_insert('hosting_task_arguments')
      ->fields(array(
        'vid' => $task->vid,
        'nid' => $task->nid,
        'name' => 'source_platform',
        'value' => (int) $site->platform,
      ))
      ->execute();
  }
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
 * Cancels the steps of a run that are still queued when it ends.
 *
 * A step that was refused before it ran leaves a queued row the queue would
 * run alone later.
 */
function hosting_tasks_extra_drupal_upgrade_cancel_steps($task, $what) {
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
      hosting_task_log($left->vid, 'warning', t('Task was cancelled: the @what it was a step of has ended.', array('@what' => $what)));
    }
  }
}

/**
 * Tidies the panel's records after an Upgrade, however it ended.
 *
 * The site node is read again and never saved from the task's own copy of
 * it, which is as it was before the move. Steps that are still queued are
 * cancelled. Where the site moved, the two backups the run registered say
 * what they hold.
 */
function hosting_tasks_extra_drupal_upgrade_end($task) {
  hosting_tasks_extra_drupal_upgrade_cancel_steps($task, 'Upgrade');
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
 * Refuses a Go back when its task starts, before the backend is asked.
 */
function hosting_tasks_extra_drupal_upgrade_return_refuse($why) {
  return drush_set_error('HOSTING_UPGRADE_RETURN_NOT_STARTED', dt('GOBACK/RESULT Not started: !why. Nothing was changed.', array('!why' => str_replace(array('<', '>'), '', $why))));
}

/**
 * Writes arguments into a running task's own row, at once.
 *
 * The runner saves a row's arguments only after the backend has returned,
 * which a runner that is killed never reaches: what a later run needs is
 * written while this one starts.
 */
function hosting_tasks_extra_task_args_keep($task, $args) {
  foreach ($args as $name => $value) {
    $task->task_args[$name] = (string) $value;
    if (empty($task->vid)) {
      continue;
    }
    db_delete('hosting_task_arguments')
      ->condition('vid', $task->vid)
      ->condition('name', $name)
      ->execute();
    db_insert('hosting_task_arguments')
      ->fields(array(
        'vid' => $task->vid,
        'nid' => $task->nid,
        'name' => $name,
        'value' => (string) $value,
      ))
      ->execute();
  }
}

/**
 * Checks a Go back as it starts, records it and tells the backend what to run.
 *
 * As for an Upgrade, nothing a form checked is taken as checked, and the
 * question is asked for the user who queued the task. A first run records
 * its names and the settings of the live site in its own row before anything
 * runs; a run that finds an unfinished earlier one on the node takes them
 * from that row, because by then the node is renamed and its settings may
 * be gone from it.
 */
function hosting_tasks_extra_drupal_upgrade_return_start($task) {
  $site = $task->ref;
  if (function_exists('hosting_panel_is_moving') && hosting_panel_is_moving()) {
    return hosting_tasks_extra_drupal_upgrade_return_refuse('the control panel is moving to its new version; start again in a few minutes');
  }
  // A standby copy of a server takes no writes of its own.
  if (db_driver() === 'mysql' && (int) db_query('SELECT @@global.read_only')->fetchField() === 1) {
    return hosting_tasks_extra_drupal_upgrade_return_refuse('this server is a standby copy, where sites are not changed');
  }
  $account = user_load($task->uid);
  if (!is_object($account) || empty($account->status)) {
    return hosting_tasks_extra_drupal_upgrade_return_refuse('the user who queued this task is not known or is blocked');
  }
  $state = hosting_tasks_extra_drupal_upgrade_return_state($site, $account, $task->nid);
  if ($state['state'] === '') {
    return hosting_tasks_extra_drupal_upgrade_return_refuse($state['why'] !== '' ? $state['why'] : 'there is nothing to go back to for this site');
  }
  $copy_node = node_load($state['copy_nid'], NULL, TRUE);
  foreach (array($site->nid, $state['copy_nid']) as $nid) {
    $busy = hosting_tasks_extra_drupal_upgrade_busy($nid, $task->nid);
    if ($busy !== '') {
      return hosting_tasks_extra_drupal_upgrade_return_refuse('a ' . $busy . ' task is waiting or running on ' . ($nid == $site->nid ? 'this site' : 'its copy'));
    }
  }
  if ($state['state'] === 'resume') {
    $record = $state['record'];
  }
  else {
    // The name the upgraded site is kept under: only a site or an alias
    // that holds it can refuse it, and a nested task checks no name itself.
    $aside = hosting_site_clean_domain('upgraded-' . $task->nid . '.temp.' . $state['domain']);
    if (!_hosting_valid_fqdn($aside) || !hosting_domain_allowed($aside) || hosting_get_site_by_url($aside)) {
      return hosting_tasks_extra_drupal_upgrade_return_refuse('the name the upgraded site would be kept under, ' . $aside . ', is not valid or is taken');
    }
    $record = array(
      'return_domain' => $state['domain'],
      'return_copy' => $state['copy'],
      'return_copy_nid' => (int) $state['copy_nid'],
      'return_aside' => $aside,
      'return_upgrade' => (int) $state['upgrade'],
      'return_when' => $state['date'] ? format_date($state['date'], 'custom', 'Y-m-d H:i') : '',
    ) + hosting_tasks_extra_drupal_upgrade_return_settings($site);
  }
  hosting_tasks_extra_task_args_keep($task, $record);

  $new = node_load($site->platform);
  $old = node_load($copy_node->platform);
  $task->options['return_domain'] = $record['return_domain'];
  $task->options['return_copy'] = $record['return_copy'];
  $task->options['return_aside'] = $record['return_aside'];
  $task->options['return_new'] = hosting_context_name($site->platform);
  $task->options['return_new_nid'] = (int) $site->platform;
  $task->options['return_old'] = hosting_context_name($copy_node->platform);
  $task->options['return_old_nid'] = (int) $copy_node->platform;
  $task->options['return_new_db_nid'] = (int) $site->db_server;
  $task->options['return_old_db_nid'] = (int) $copy_node->db_server;
  $task->options['return_task_nid'] = (int) $task->nid;
  $task->options['return_new_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $new->title);
  $task->options['return_old_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $old->title);
  $task->options['return_when'] = $record['return_when'];
  $task->options['return_copy_secs'] = hosting_tasks_extra_drupal_upgrade_copy_seconds($site->nid);
  $task->options['return_ceiling'] = (int) variable_get('hosting_task_runner_lock_max_hold', 28800);
  // Which of the two the panel holds as switched on: a step that is done
  // is not run again.
  if ($state['state'] === 'resume' && $site->site_status == HOSTING_SITE_ENABLED) {
    $task->options['return_aside_on'] = 1;
  }
  if ($copy_node->site_status == HOSTING_SITE_ENABLED) {
    $task->options['return_copy_on'] = 1;
  }
}

/**
 * The settings of a live site that a rename drops or leaves behind.
 *
 * A rename moves a redirection only when it names the old name or its www
 * or bare companion, leaves custom aliases on the renamed site and switches
 * HTTPS off; the copy never had any of them.
 *
 * @return array
 *   Task arguments, each a plain string.
 */
function hosting_tasks_extra_drupal_upgrade_return_settings($site) {
  $aliases = array();
  if (!empty($site->aliases) && is_array($site->aliases)) {
    foreach ($site->aliases as $alias) {
      $alias = hosting_site_clean_domain($alias);
      if ($alias !== '') {
        $aliases[$alias] = $alias;
      }
    }
  }
  return array(
    'return_aliases' => implode(',', $aliases),
    'return_redirection' => empty($site->redirection) ? '' : hosting_site_clean_domain($site->redirection),
    'return_ssl_enabled' => isset($site->ssl_enabled) ? (int) $site->ssl_enabled : '',
    'return_ssl_key' => isset($site->ssl_key) ? (int) $site->ssl_key : '',
    'return_cron' => (isset($site->cron_interval) && is_numeric($site->cron_interval)) ? (int) $site->cron_interval : '',
  );
}

/**
 * Gives a site the settings a Go back recorded.
 *
 * @return bool
 *   Whether anything was changed and saved.
 */
function hosting_tasks_extra_drupal_upgrade_return_apply($node, $record, $bare = FALSE) {
  $aliases = array();
  if (!$bare && isset($record['return_aliases']) && $record['return_aliases'] !== '') {
    $aliases = array_values(array_filter(explode(',', $record['return_aliases']), 'strlen'));
  }
  $redirection = (!$bare && isset($record['return_redirection'])) ? (string) $record['return_redirection'] : '';
  $changed = FALSE;
  $now = (!empty($node->aliases) && is_array($node->aliases)) ? array_values($node->aliases) : array();
  sort($now);
  $want = $aliases;
  sort($want);
  if ($now !== $want) {
    $node->aliases = $aliases;
    $changed = TRUE;
  }
  if ((empty($node->redirection) ? '' : (string) $node->redirection) !== $redirection) {
    $node->redirection = $redirection;
    $changed = TRUE;
  }
  // Cron off on the site that is set aside; as it was on the one that serves.
  $cron = $bare ? 0 : ((isset($record['return_cron']) && $record['return_cron'] !== '') ? (int) $record['return_cron'] : NULL);
  if ($cron !== NULL && (!isset($node->cron_interval) || (int) $node->cron_interval !== $cron)) {
    $node->cron_interval = $cron;
    $changed = TRUE;
  }
  if (!$bare && isset($node->ssl_enabled) && isset($record['return_ssl_enabled']) && $record['return_ssl_enabled'] !== ''
    && (int) $node->ssl_enabled !== (int) $record['return_ssl_enabled']) {
    $node->ssl_enabled = (int) $record['return_ssl_enabled'];
    // The key the live site had, while the panel still holds it; a key
    // that is gone is made again under the site's name.
    $key = isset($record['return_ssl_key']) ? (int) $record['return_ssl_key'] : 0;
    if ($node->ssl_enabled && $key > 0 && function_exists('hosting_ssl_get_key') && hosting_ssl_get_key($key)) {
      $node->ssl_key = $key;
    }
    elseif ($node->ssl_enabled && defined('HOSTING_SSL_CUSTOM_KEY')) {
      $node->ssl_key = HOSTING_SSL_CUSTOM_KEY;
      $node->ssl_key_new = hosting_site_clean_domain($node->title);
    }
    $changed = TRUE;
  }
  if ($changed) {
    $node->no_verify = TRUE;
    node_save($node);
  }
  return $changed;
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_tasks_extra_post_hosting_drupal_upgrade_return_task($task, $data) {
  hosting_tasks_extra_drupal_upgrade_return_end($task);
}

/**
 * Implements hook_hosting_TASK_TYPE_task_rollback().
 */
function hosting_tasks_extra_hosting_drupal_upgrade_return_task_rollback($task, $data) {
  hosting_tasks_extra_drupal_upgrade_return_end($task);
}

/**
 * Settles the panel's records after a Go back, however it ended.
 *
 * Both nodes are read again and judged by the name each holds now. Where
 * the copy holds the site's name, the site that was set aside loses its
 * custom aliases, its redirection and its cron first, and the restored site
 * then gets what the first run recorded; where the upgraded site holds its
 * name again, it gets its own settings back. A Verify of each site that was
 * changed writes it out. A run that left the name with neither changes
 * nothing here: the next run finishes it.
 */
function hosting_tasks_extra_drupal_upgrade_return_end($task) {
  hosting_tasks_extra_drupal_upgrade_cancel_steps($task, 'Go back');
  $record = isset($task->task_args) && is_array($task->task_args) ? $task->task_args : array();
  if (empty($record['return_domain']) || empty($record['return_aside']) || empty($record['return_copy_nid'])) {
    return;
  }
  $domain = $record['return_domain'];
  $site = node_load($task->rid, NULL, TRUE);
  $copy = node_load($record['return_copy_nid'], NULL, TRUE);
  if (!is_object($site) || $site->type != 'site') {
    return;
  }
  $site_name = hosting_site_clean_domain($site->title);
  if (is_object($copy) && $copy->type == 'site' && hosting_site_clean_domain($copy->title) == $domain && $site_name == $record['return_aside']) {
    // Gone back. The site that is set aside first: while it still holds an
    // alias, the restored site cannot take it.
    hosting_tasks_extra_drupal_upgrade_return_apply($site, $record, TRUE);
    hosting_add_task($site->nid, 'verify');
    hosting_tasks_extra_drupal_upgrade_return_apply($copy, $record);
    hosting_add_task($copy->nid, 'verify');
    return;
  }
  if ($site_name == $domain && $site->site_status == HOSTING_SITE_ENABLED) {
    // Not gone back: the upgraded site holds its name, after a rename away
    // and back when a step ran. What that dropped is put back.
    if (hosting_tasks_extra_drupal_upgrade_return_apply($site, $record)) {
      hosting_add_task($site->nid, 'verify');
    }
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

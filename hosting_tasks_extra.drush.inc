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
  if ($task->ref->type == 'platform' && $task->task_type == 'drupal_upgrade_all') {
    hosting_tasks_extra_drupal_upgrade_all_start($task);
  }
  if ($task->ref->type == 'platform' && $task->task_type == 'platform_successor') {
    hosting_tasks_extra_platform_successor_start($task);
  }
  // The Upgrade's move: a Migrate that is undone when the database updates
  // do not finish or a page no longer answers as it did.
  if ($task->ref->type == 'site' && $task->task_type == 'migrate' && !empty($task->task_args['upgrade_strict'])) {
    $task->options['upgrade_strict'] = 1;
  }
  if ($task->ref->type == 'site' && $task->task_type == 'migrate') {
    hosting_tasks_extra_drupal_upgrade_migrate_start($task);
  }
}

/**
 * Refuses a Migrate that would move an upgraded site back, however queued.
 *
 * The site's own Migrate form leaves out a platform an Upgrade moved the
 * site away from, and refuses it. A Migrate of every site of a platform, or
 * a row queued without a form, comes through here instead. A rename stays
 * on the site's own platform and is never such a move.
 */
function hosting_tasks_extra_drupal_upgrade_migrate_start($task) {
  $target = isset($task->task_args['target_platform']) ? (int) $task->task_args['target_platform'] : 0;
  if (!$target || $target == $task->ref->platform) {
    return;
  }
  if (in_array($target, hosting_tasks_extra_drupal_upgrade_left_platforms($task->ref))) {
    $platform = node_load($target);
    return drush_set_error('HOSTING_UPGRADE_MIGRATE_BACK', dt('Not migrated: @site was upgraded away from @platform, and a Migrate back would put its upgraded database under the older code. Nothing was changed.', array(
      '@site' => $task->ref->title,
      '@platform' => is_object($platform) ? $platform->title : 'that platform',
    )));
  }
}

/**
 * Refuses an Upgrade when its task starts, before the backend is asked.
 */
function hosting_tasks_extra_drupal_upgrade_refuse($why) {
  return drush_set_error('HOSTING_UPGRADE_NOT_STARTED', dt('UPGRADE/RESULT Not started: !why. Nothing was changed.', array('!why' => str_replace(array('<', '>'), '', $why))));
}

/**
 * The users a task is checked for as it starts.
 *
 * The row's author, and the user who queued this row when that is another
 * one: Retry and Run Again copy a row with its author and record who pressed
 * them, and a role that may press them need not hold what the task asks.
 *
 * @return array|string
 *   The accounts, the author first, or why the task is refused.
 */
function hosting_tasks_extra_task_accounts($task) {
  $account = user_load($task->uid);
  if (!is_object($account) || empty($account->status)) {
    return 'the user who queued this task is not known or is blocked';
  }
  $accounts = array($account);
  $name = isset($task->queued_by) ? (string) $task->queued_by : '';
  if ($name !== '' && $name !== $account->name) {
    // A name no account holds any more is not checked (the author's own
    // name before a rename, or a user deleted since): the author's rights
    // still apply.
    $queuer = user_load_by_name($name);
    if (is_object($queuer) && (int) $queuer->uid !== (int) $account->uid) {
      if (empty($queuer->status)) {
        return 'the user who queued this task again, ' . $name . ', is blocked';
      }
      $accounts[] = $queuer;
    }
  }
  return $accounts;
}

/**
 * Checks an Upgrade once more as it starts and tells the backend what to run.
 *
 * The row may have been queued without the form, so nothing the form
 * checked is taken as checked; and the question is asked for the users who
 * queued the task (hosting_tasks_extra_task_accounts()), not for the runner.
 */
function hosting_tasks_extra_drupal_upgrade_start($task) {
  $site = $task->ref;
  $target_nid = isset($task->task_args['target_platform']) ? (int) $task->task_args['target_platform'] : 0;
  // A site of an Upgrade all sites run: only one its run vouches for, still
  // on the platform the run was for, and only while no other site of the run
  // ended in error or is still processing (a runner that was killed cancels
  // nothing, and the queue does not promise the run's order).
  $member = hosting_tasks_extra_drupal_upgrade_all_membership($task);
  if ($member['member']) {
    if ($member['why'] !== '') {
      return hosting_tasks_extra_drupal_upgrade_refuse($member['why']);
    }
    if ((int) $site->platform !== $member['platform']) {
      return hosting_tasks_extra_drupal_upgrade_refuse('this site is no longer on the platform its Upgrade all sites was for');
    }
    $blocked = hosting_tasks_extra_drupal_upgrade_all_blocked($member['token'], $task->nid);
    if ($blocked !== '') {
      return hosting_tasks_extra_drupal_upgrade_refuse($blocked);
    }
  }
  if (function_exists('hosting_panel_is_moving') && hosting_panel_is_moving()) {
    return hosting_tasks_extra_drupal_upgrade_refuse('the control panel is moving to its new version; start again in a few minutes');
  }
  // A standby copy of a server takes no writes of its own.
  if (db_driver() === 'mysql' && (int) db_query('SELECT @@global.read_only')->fetchField() === 1) {
    return hosting_tasks_extra_drupal_upgrade_refuse('this server is a standby copy, where sites are not changed');
  }
  $accounts = hosting_tasks_extra_task_accounts($task);
  if (!is_array($accounts)) {
    return hosting_tasks_extra_drupal_upgrade_refuse($accounts);
  }
  $account = $accounts[0];
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
  if (isset($accounts[1])) {
    $other = hosting_tasks_extra_drupal_upgrade_target($site, $accounts[1], $target_nid);
    if (!$other['ready']) {
      return hosting_tasks_extra_drupal_upgrade_refuse('for the user who queued it again, ' . ($other['title'] !== '' ? $other['title'] . ': ' : '') . $other['why']);
    }
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
  // A run removes the copy that an earlier run of this site left when it was
  // cut off before its move; the variable set to 0 leaves it for a Delete in
  // the panel.
  if (variable_get('hosting_tasks_extra_upgrade_tidy', TRUE)) {
    $task->options['upgrade_tidy'] = 1;
  }
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
  hosting_tasks_extra_drupal_upgrade_keep_size($task);
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
 * Keeps on the Upgrade's row what its way-back copy uses on disk.
 *
 * The backend says it in its log, which the panel purges after a time, and
 * the copy is kept until the user deletes it: the two figures go into the
 * row's arguments, as the platform it left does.
 */
function hosting_tasks_extra_drupal_upgrade_keep_size($task) {
  if (empty($task->vid)) {
    return;
  }
  $line = db_query_range("SELECT message FROM {hosting_task_log} WHERE vid = :vid AND message LIKE :size ORDER BY lid DESC", 0, 1, array(
    ':vid' => $task->vid,
    ':size' => db_like('UPGRADE/1/COPY size: ') . '%',
  ))->fetchField();
  if (!is_string($line)) {
    return;
  }
  $keep = array();
  $m = array();
  if (preg_match('/\bfiles ([0-9]+) bytes\b/', $line, $m)) {
    $keep['copy_files'] = $m[1];
  }
  if (preg_match('/\bdatabase ([0-9]+) bytes\b/', $line, $m)) {
    $keep['copy_database'] = $m[1];
  }
  foreach ($keep as $name => $value) {
    $task->task_args[$name] = $value;
    db_delete('hosting_task_arguments')
      ->condition('vid', $task->vid)
      ->condition('name', $name)
      ->execute();
    db_insert('hosting_task_arguments')
      ->fields(array('vid' => $task->vid, 'nid' => $task->nid, 'name' => $name, 'value' => $value))
      ->execute();
  }
}

/**
 * Refuses the last row of an Upgrade all sites run as it starts.
 */
function hosting_tasks_extra_drupal_upgrade_all_refuse($why) {
  return drush_set_error('HOSTING_UPGRADE_ALL_NOT_STARTED', dt('UPGRADE-ALL/RESULT Not started: !why. Nothing was changed.', array('!why' => str_replace(array('<', '>'), '', $why))));
}

/**
 * Checks the last row of an Upgrade all sites run as it starts.
 *
 * Only the row the run itself queued goes on to its Verify: a row of this
 * type queued any other way ends here.
 */
function hosting_tasks_extra_drupal_upgrade_all_start($task) {
  $token = isset($task->task_args['batch']) ? (string) $task->task_args['batch'] : '';
  $parts = hosting_tasks_extra_drupal_upgrade_all_token($token);
  $rollup = $parts ? hosting_tasks_extra_drupal_upgrade_all_rollup($token) : FALSE;
  if (!$parts || $parts['platform'] !== (int) $task->rid || empty($task->created) || (int) $task->created !== $parts['time']
    || !$rollup || (int) $rollup->nid !== (int) $task->nid) {
    return hosting_tasks_extra_drupal_upgrade_all_refuse('start this from Upgrade all sites');
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 *
 * The row rides a Verify of the old platform, and the panel's own hook for a
 * Verify is keyed by its task type: it is called here first, so the
 * platform's packages are recorded as any Verify records them.
 */
function hosting_tasks_extra_post_hosting_drupal_upgrade_all_task($task, $data) {
  if (function_exists('hosting_platform_post_hosting_verify_task')) {
    hosting_platform_post_hosting_verify_task($task, $data);
  }
  hosting_tasks_extra_drupal_upgrade_all_end($task, TRUE);
}

/**
 * Implements hook_hosting_TASK_TYPE_task_rollback().
 *
 * Also called when the post hook ends the row in error.
 */
function hosting_tasks_extra_hosting_drupal_upgrade_all_task_rollback($task, $data) {
  hosting_tasks_extra_drupal_upgrade_all_end($task, FALSE);
}

/**
 * Says what each site of an Upgrade all sites run did, and what now.
 *
 * Read from the sites' rows and their nodes, never from the order the queue
 * ran them in: a site is upgraded when its node is on the target. A run that
 * moved no site unlocks the platform it locked.
 *
 * @param object $task
 *   The run's last row.
 * @param bool $verified
 *   The Verify of the old platform ended well.
 */
function hosting_tasks_extra_drupal_upgrade_all_end($task, $verified) {
  $done = &drupal_static(__FUNCTION__, array());
  if (isset($done[$task->nid])) {
    return;
  }
  $done[$task->nid] = TRUE;
  $token = isset($task->task_args['batch']) ? (string) $task->task_args['batch'] : '';
  $rollup = hosting_tasks_extra_drupal_upgrade_all_rollup($token);
  if (!$rollup || (int) $rollup->nid !== (int) $task->nid) {
    return;
  }
  $target_nid = (int) $rollup->target;
  // Free text: it goes into the run's log lines as plain text.
  $target = _hosting_tasks_extra_plain(hosting_tasks_extra_node_title($target_nid));
  $platform = node_load($task->rid, NULL, TRUE);
  $title = is_object($platform) ? $platform->title : hosting_tasks_extra_node_title($task->rid);
  if (!$verified) {
    drush_log(dt('UPGRADE-ALL/VERIFY the Verify of !platform did not finish (see above): its packages are recorded as they were.', array('!platform' => _hosting_tasks_extra_plain($title))), 'warning');
  }
  // 'moved': sites this run's rows moved; 'there': sites of the run that
  // are on the target now, by whatever task.
  $count = array('moved' => 0, 'there' => 0, 'error' => 0, 'cancelled' => 0, 'hand' => 0, 'open' => 0);
  $members = hosting_tasks_extra_drupal_upgrade_all_members($token);
  $sites = array();
  $rows = array();
  foreach ($members as $member) {
    $sites[] = (int) $member->rid;
    $rows[] = (int) $member->nid;
    drush_log(dt('UPGRADE-ALL/SITE @site: !line', array(
      '@site' => hosting_tasks_extra_node_title($member->rid),
      '!line' => _hosting_tasks_extra_drupal_upgrade_all_site_line($member, $target_nid, $target, $count),
    )), 'ok');
  }
  $left = 0;
  $items = isset($task->task_args['left_out']) ? explode("\n", (string) $task->task_args['left_out']) : array();
  foreach ($items as $item) {
    $pair = explode("\t", $item, 2);
    if (count($pair) == 2 && ctype_digit($pair[0])) {
      $left++;
      // Plain text for the task log: dt() would escape an apostrophe.
      drush_log(dt('UPGRADE-ALL/SITE @site: left out at the start (!why).', array('@site' => hosting_tasks_extra_node_title($pair[0]), '!why' => _hosting_tasks_extra_plain($pair[1]))), 'ok');
    }
  }

  // What now.
  if (is_object($platform) && $platform->platform_status == HOSTING_PLATFORM_LOCKED) {
    if (!$count['moved'] && !$count['there'] && !$count['open']) {
      // The run took the lock from an enabled platform, and no site of it
      // left the platform, by this run or another task.
      $platform->platform_status = HOSTING_PLATFORM_ENABLED;
      $platform->no_verify = TRUE;
      node_save($platform);
      drush_log(dt('UPGRADE-ALL/NEXT !platform is unlocked again: no site moved off it.', array('!platform' => _hosting_tasks_extra_plain($title))), 'ok');
    }
    else {
      $copies = 0;
      $stay = 0;
      $names = db_query("SELECT n.title FROM {hosting_site} s INNER JOIN {node} n ON n.vid = s.vid WHERE s.platform = :platform AND s.status <> :deleted", array(
        ':platform' => (int) $platform->nid,
        ':deleted' => HOSTING_SITE_DELETED,
      ))->fetchCol();
      foreach ($names as $name) {
        if (preg_match('/^back-[0-9]+\.temp\./', hosting_site_clean_domain($name))) {
          $copies++;
        }
        else {
          $stay++;
        }
      }
      drush_log(dt('UPGRADE-ALL/NEXT !platform stays locked: it holds @copies and @sites still on it, which cannot be moved or cloned from the panel until it is unlocked. When the upgraded sites are checked, delete their way-back copies; Delete the platform when no site is left on it.', array(
        '!platform' => _hosting_tasks_extra_plain($title),
        '@copies' => format_plural($copies, '1 way-back copy', '@count way-back copies'),
        '@sites' => format_plural($stay, '1 site', '@count sites'),
      )), 'ok');
    }
  }
  // What the sites' own runs queued after them, such as their Verifies.
  if ($sites) {
    $after = db_query("SELECT t.task_type, t.rid FROM {hosting_task} t INNER JOIN {node} n ON n.vid = t.vid
      WHERE t.rid IN (:sites) AND t.nid NOT IN (:rows) AND t.task_status = :queued ORDER BY t.nid", array(
        ':sites' => $sites,
        ':rows' => $rows,
        ':queued' => HOSTING_TASK_QUEUED,
      ))->fetchAll();
    $queued = array();
    foreach ($after as $row) {
      $queued[] = $row->task_type . ' of ' . hosting_tasks_extra_node_title($row->rid);
    }
    if ($queued) {
      drush_log(dt('UPGRADE-ALL/NEXT still queued after this run: !rows.', array('!rows' => _hosting_tasks_extra_plain(implode(', ', $queued)))), 'ok');
    }
  }

  $text = $count['moved'] . ' of ' . count($members) . ' sites upgraded onto ' . $target;
  $said = array(
    'error' => '@count ended in error',
    'cancelled' => '@count cancelled after it',
    'hand' => '@count cancelled by hand',
    'open' => '@count not finished',
  );
  foreach ($said as $key => $words) {
    if ($count[$key]) {
      $text .= '; ' . str_replace('@count', $count[$key], $words);
    }
  }
  if ($left) {
    $text .= '; ' . $left . ' left out at the start';
  }
  if ($count['error']) {
    drush_set_error('HOSTING_UPGRADE_ALL_FAILED', dt('UPGRADE-ALL/RESULT !text.', array('!text' => $text)));
  }
  elseif ($count['hand'] || $count['open']) {
    drush_log(dt('UPGRADE-ALL/RESULT !text.', array('!text' => $text)), 'warning');
  }
  else {
    drush_log(dt('UPGRADE-ALL/RESULT !text.', array('!text' => $text)), 'success');
  }
}

/**
 * Text for a task log line, as plain text: no markup, nothing escaped.
 */
function _hosting_tasks_extra_plain($text) {
  return str_replace(array('<', '>'), '', (string) $text);
}

/**
 * One site's line of an Upgrade all sites run's result.
 *
 * @param array $count
 *   The run's counts, added to.
 */
function _hosting_tasks_extra_drupal_upgrade_all_site_line($member, $target_nid, $target, &$count) {
  $messages = db_query("SELECT message FROM {hosting_task_log} WHERE vid = :vid
      AND (message LIKE :result OR message LIKE :cancel OR message LIKE :hand OR message LIKE :lost)
    ORDER BY lid DESC", array(
      ':vid' => (int) $member->vid,
      ':result' => db_like('UPGRADE/RESULT ') . '%',
      ':cancel' => db_like('Cancelled: the upgrade of ') . '%',
      ':hand' => db_like('Task was cancelled') . '%',
      ':lost' => db_like('Task runner process ') . '%',
    ))->fetchCol();
  $found = array('result' => '', 'cancel' => '', 'hand' => '', 'lost' => '');
  $starts = array('result' => 'UPGRADE/RESULT ', 'cancel' => 'Cancelled: the upgrade of ', 'hand' => 'Task was cancelled', 'lost' => 'Task runner process ');
  foreach ($messages as $message) {
    $message = trim(str_replace(array('<', '>'), '', (string) $message));
    foreach ($starts as $key => $start) {
      if ($found[$key] === '' && strpos($message, $start) === 0) {
        $found[$key] = ($key === 'result') ? substr($message, strlen($start)) : $message;
      }
    }
  }
  $result = $found['result'];
  if ($member->task_status == HOSTING_TASK_QUEUED) {
    $count['open']++;
    return 'not finished (still queued)';
  }
  if ($member->task_status == HOSTING_TASK_PROCESSING) {
    $count['open']++;
    return 'not finished (still running)';
  }
  // What this row did, by the rule the site page reads an Upgrade's rows
  // with: a site on the target that another task moved there (a Retry) is
  // not this row's doing.
  $moved = FALSE;
  $site = node_load($member->rid, NULL, TRUE);
  if (is_object($site) && $site->type == 'site') {
    foreach (hosting_tasks_extra_drupal_upgrade_runs($site) as $run) {
      if ((int) $run['task'] === (int) $member->nid) {
        $moved = TRUE;
      }
    }
  }
  $there = ($member->platform && (int) $member->platform === (int) $target_nid);
  if ($there) {
    $count['there']++;
  }
  // That rule counts a row cut off with its copy kept as one that may have
  // moved the site, which the site page needs; here the site's place says
  // whether it did.
  if ($moved && !$there && $member->task_status != HOSTING_TASK_ERROR) {
    $count['moved']++;
    return 'upgraded onto ' . $target . ($result !== '' ? ' (' . $result . ')' : '') . '; the site has left it since';
  }
  if (!$moved || !$there) {
    $line = _hosting_tasks_extra_drupal_upgrade_all_unmoved($member, $found, $count);
    return $there ? $line . '; the site is on ' . $target . ' now, moved there by another task' : $line;
  }
  $count['moved']++;
  if ($member->task_status == HOSTING_TASK_ERROR) {
    $count['error']++;
    return 'upgraded, last step unfinished (' . ($result !== '' ? $result : 'see its task log') . ')';
  }
  return 'upgraded onto ' . $target . ($result !== '' ? ' (' . $result . ')' : '');
}

/**
 * The line of a site whose row of the run did not move it.
 *
 * @param array $found
 *   The row's result, cancel, hand and lost log lines.
 * @param array $count
 *   The run's counts, added to.
 */
function _hosting_tasks_extra_drupal_upgrade_all_unmoved($member, $found, &$count) {
  $result = $found['result'];
  if ($member->task_status == HOSTING_TASK_ERROR) {
    $count['error']++;
    if ($result === '' && $found['lost'] !== '') {
      return 'failed: its runner was lost';
    }
    if (strpos($result, 'Not started: ') === 0) {
      return 'not started (' . rtrim(preg_replace('/\.? Nothing was changed\.$/', '', substr($result, strlen('Not started: '))), '.') . ')';
    }
    return 'failed (' . ($result !== '' ? $result : 'see its task log') . ')';
  }
  if ($member->task_status == HOSTING_TASK_WARNING) {
    $m = array();
    if ($found['cancel'] !== '' && preg_match('/^Cancelled: the upgrade of (.+?)(?:, before this one| in the same Upgrade all sites)/', $found['cancel'], $m)) {
      $count['cancelled']++;
      return 'cancelled after ' . $m[1] . ' did not upgrade';
    }
    $count['hand']++;
    if ($found['hand'] === '' || $found['hand'] === 'Task was cancelled.') {
      return 'cancelled by hand';
    }
    return 'cancelled (' . rtrim(preg_replace('/^Task was cancelled: */', '', $found['hand']), '.') . ')';
  }
  $count['error']++;
  return 'ended without moving the site (' . ($result !== '' ? $result : 'see its task log') . ')';
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
  $accounts = hosting_tasks_extra_task_accounts($task);
  if (!is_array($accounts)) {
    return hosting_tasks_extra_drupal_upgrade_return_refuse($accounts);
  }
  $state = hosting_tasks_extra_drupal_upgrade_return_state($site, $accounts[0], $task->nid);
  if ($state['state'] === '') {
    return hosting_tasks_extra_drupal_upgrade_return_refuse($state['why'] !== '' ? $state['why'] : 'there is nothing to go back to for this site');
  }
  if (isset($accounts[1])) {
    $other = hosting_tasks_extra_drupal_upgrade_return_state($site, $accounts[1], $task->nid);
    if ($other['state'] !== $state['state']) {
      return hosting_tasks_extra_drupal_upgrade_return_refuse('for the user who queued it again, ' . ($other['why'] !== '' ? $other['why'] : 'there is nothing to go back to for this site'));
    }
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
    if (hosting_site_clean_domain($site->title) === $record['return_domain']) {
      // Under its own name again: the renames drop HTTPS, nothing else that
      // stays with the node. The rest is the live site's own, which may have
      // been changed since the run that stopped.
      $live = hosting_tasks_extra_drupal_upgrade_return_settings($site);
      $https = array();
      if (!empty($record['return_ssl_enabled']) && empty($live['return_ssl_enabled'])) {
        $https = array('return_ssl_enabled' => $record['return_ssl_enabled'], 'return_ssl_key' => isset($record['return_ssl_key']) ? $record['return_ssl_key'] : '');
      }
      $record = $https + $live + $record;
    }
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
      // A run that switches the copy on for its rename switches it off again
      // when the rename fails, unless it was on before Go back began.
      'return_copy_was_on' => ($copy_node->site_status == HOSTING_SITE_ENABLED) ? 1 : 0,
    ) + hosting_tasks_extra_drupal_upgrade_return_settings($site);
  }
  // Retry and Run Again copy a row's arguments into a row that never ran:
  // the record counts only on the row that wrote it.
  $record['return_run'] = (int) $task->nid;
  // Until the run's own end says how it ended: a runner that is killed never
  // gets there (hosting_tasks_extra_drupal_upgrade_return_end()).
  $record['return_end'] = 'pending';
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
  if (!empty($record['return_copy_was_on'])) {
    $task->options['return_copy_was_on'] = 1;
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
  // The backend ended well, so the copy holds the site's name on disk. Where
  // the panel's records do not show it, the task says so.
  if (hosting_tasks_extra_drupal_upgrade_return_end($task, $data, TRUE) !== 'back') {
    drush_log(dt('GOBACK/PANEL the copy was put back, and the panel\'s record of it did not follow: the restored site may still be listed under the copy\'s name, without the settings of the live site. Ask support.'), 'warning');
  }
}

/**
 * Implements hook_hosting_TASK_TYPE_task_rollback().
 */
function hosting_tasks_extra_hosting_drupal_upgrade_return_task_rollback($task, $data) {
  hosting_tasks_extra_drupal_upgrade_return_end($task, $data);
}

/**
 * Settles the panel's records after a Go back, however it ended.
 *
 * Both nodes are read again and judged by the name each holds now. Where
 * the copy holds the site's name, the site that was set aside loses its
 * custom aliases, its redirection and its cron first, and the restored site
 * then gets what the first run recorded; where the upgraded site holds its
 * name again, it gets its own settings back, also when the step that was to
 * switch it on failed. A Verify of each site that was changed writes it
 * out. A run that left the name with neither changes nothing here: the next
 * run finishes it.
 *
 * Every end leaves its mark on the run's own row (return_end), which is
 * what tells a later run whether this one finished
 * (_provision_tasks_extra_return_offer()): 'stuck' where the backend ended
 * in a state no run can go on from, 'panel' where it ended well and the
 * panel's records did not follow (a later run would only say so again),
 * else 'back', 'kept' or 'open'.
 *
 * @param object $task
 *   The Go back task.
 * @param mixed $data
 *   What the backend returned.
 * @param bool $ended_well
 *   Called from the post hook: the backend ended well.
 *
 * @return string
 *   'back' where the copy holds the name, 'kept' where the upgraded site
 *   does, '' otherwise.
 */
function hosting_tasks_extra_drupal_upgrade_return_end($task, $data = NULL, $ended_well = FALSE) {
  hosting_tasks_extra_drupal_upgrade_cancel_steps($task, 'Go back');
  $record = isset($task->task_args) && is_array($task->task_args) ? $task->task_args : array();
  if (empty($record['return_domain']) || empty($record['return_aside']) || empty($record['return_copy_nid'])) {
    return '';
  }
  $end = hosting_tasks_extra_drupal_upgrade_return_settle($task, $record);
  $mark = ($end !== '') ? $end : ($ended_well ? 'panel' : 'open');
  if (is_array($data) && !empty($data['error_log']) && is_array($data['error_log']) && isset($data['error_log']['PROVISION_UPGRADE_RETURN_STUCK'])) {
    $mark = 'stuck';
  }
  hosting_tasks_extra_task_args_keep($task, array('return_end' => $mark));
  return $end;
}

/**
 * Puts the recorded settings on the site that holds the name now.
 *
 * @return string
 *   As hosting_tasks_extra_drupal_upgrade_return_end().
 */
function hosting_tasks_extra_drupal_upgrade_return_settle($task, $record) {
  $domain = $record['return_domain'];
  $site = node_load($task->rid, NULL, TRUE);
  $copy = node_load($record['return_copy_nid'], NULL, TRUE);
  if (!is_object($site) || $site->type != 'site') {
    return '';
  }
  $site_name = hosting_site_clean_domain($site->title);
  if (is_object($copy) && $copy->type == 'site' && hosting_site_clean_domain($copy->title) == $domain && $site_name == $record['return_aside']) {
    // Gone back. The site that is set aside first: while it still holds an
    // alias, the restored site cannot take it.
    hosting_tasks_extra_drupal_upgrade_return_apply($site, $record, TRUE);
    hosting_add_task($site->nid, 'verify');
    hosting_tasks_extra_drupal_upgrade_return_apply($copy, $record);
    hosting_add_task($copy->nid, 'verify');
    return 'back';
  }
  if ($site_name == $domain) {
    // Not gone back: the upgraded site holds its name, after a rename away
    // and back when a step ran. What that dropped is put back, also on a
    // site the panel holds as switched off (the Enable after the rename back
    // failed): its Verify writes the settings and leaves the site as served
    // as its own record says, and an Enable saves none of them.
    if (hosting_tasks_extra_drupal_upgrade_return_apply($site, $record)) {
      hosting_add_task($site->nid, 'verify');
    }
    return 'kept';
  }
  return '';
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

/**
 * Refuses a Build newer version when its task starts.
 */
function hosting_tasks_extra_platform_successor_refuse($why, $tail = 'Nothing was changed.') {
  return drush_set_error('HOSTING_SUCCESSOR_NOT_STARTED', dt('SUCCESSOR/RESULT Not started: !why. !tail', array(
    '!why' => _hosting_tasks_extra_plain($why),
    '!tail' => _hosting_tasks_extra_plain($tail),
  )));
}

/**
 * Starts a Build newer version: every check again, then the new platform.
 *
 * The new node is made here, under the queue's lock and after every check
 * the panel can make, and Locked: no site, Migrate or Upgrade takes it
 * before the backend has judged the build. Its own Verify, run by the
 * backend as a step of this task, builds it.
 */
function hosting_tasks_extra_platform_successor_start($task) {
  if (!hosting_tasks_extra_successor_include()) {
    return hosting_tasks_extra_platform_successor_refuse('the control panel is being updated; start again in a few minutes');
  }
  $platform = node_load($task->rid, NULL, TRUE);
  $accounts = hosting_tasks_extra_task_accounts($task);
  if (is_string($accounts)) {
    return hosting_tasks_extra_platform_successor_refuse($accounts);
  }
  foreach ($accounts as $account) {
    $why = hosting_tasks_extra_platform_successor_why($platform, $account, $task->nid);
    if ($why !== '') {
      return hosting_tasks_extra_platform_successor_refuse($why);
    }
  }
  $why = hosting_tasks_extra_drupal_upgrade_all_refusal();
  if ($why !== '') {
    return hosting_tasks_extra_platform_successor_refuse($why);
  }
  // A Retry keeps the arguments of the row it copies, the platform that
  // row made among them.
  if (!empty($task->task_args['successor'])) {
    $made = node_load((int) $task->task_args['successor'], NULL, TRUE);
    if (is_object($made) && $made->type == 'platform' && (int) $made->platform_status !== HOSTING_PLATFORM_DELETED) {
      return hosting_tasks_extra_platform_successor_refuse('an earlier run of this task built ' . $made->title . '; start a new Build newer version from the platform\'s page');
    }
  }
  if (file_exists('/run/boa_queue_stop.pid')) {
    return hosting_tasks_extra_platform_successor_refuse('the task queue is paused on this server for maintenance; start again later');
  }
  if (file_exists('/run/octopus_install_run.pid')) {
    return hosting_tasks_extra_platform_successor_refuse('a system upgrade of this account is running; start again when it has ended');
  }
  $plan = hosting_tasks_extra_platform_successor_plan($platform, $task->task_args);
  if ($plan['why'] !== '') {
    return hosting_tasks_extra_platform_successor_refuse($plan['why']);
  }
  $lock = _provision_tasks_extra_lock_load(_provision_tasks_extra_docroot($platform->publish_path));
  if (empty($lock['ok'])) {
    return hosting_tasks_extra_platform_successor_refuse('the platform it is built from has ' . $lock['why']);
  }
  $made = hosting_tasks_extra_platform_successor_make($platform, $plan);
  if (is_string($made)) {
    return hosting_tasks_extra_platform_successor_refuse($made);
  }
  // Written at once: a runner killed from here on leaves the row naming
  // the platform it made, which the next run refuses beside.
  hosting_tasks_extra_task_args_keep($task, array(
    'successor' => (int) $made->nid,
    'successor_name' => $plan['title'],
    'successor_path' => $plan['folder'],
    'successor_run' => (int) $task->nid,
  ));
  $task->options['successor_context'] = '@platform_' . $plan['context'];
  $task->options['successor_nid'] = (int) $made->nid;
  $task->options['successor_path'] = $plan['folder'];
  $task->options['successor_root'] = $plan['publish_path'];
  $task->options['successor_source_path'] = $plan['source']['path'];
  $task->options['successor_major'] = (int) $plan['major'];
  $task->options['successor_task_nid'] = (int) $task->nid;
  $task->options['successor_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $plan['title']);
  $task->options['source_title'] = preg_replace('/[^A-Za-z0-9._ -]/', '', $platform->title);
}

/**
 * Makes the new platform's node: the source's record with the new name,
 * folder and version, Locked, never verified, its Verify not queued.
 *
 * @return object|string
 *   The node, loaded fresh, or why it was not made.
 */
function hosting_tasks_extra_platform_successor_make($platform, $plan) {
  $source = $plan['source'];
  $new = clone node_load($platform->nid, NULL, TRUE);
  // The source's own: its ids, its context name (which a save of the clone
  // would not change in memory), its Git row and its recorded packages.
  foreach (array('nid', 'vid', 'created', 'changed', 'old_vid', 'is_new', 'original', 'revision', 'hosting_name', 'git', 'release', 'profiles', 'context_report_only', 'path') as $key) {
    unset($new->{$key});
  }
  $new->title = $plan['title'];
  $new->publish_path = $plan['publish_path'];
  $new->platform_status = HOSTING_PLATFORM_LOCKED;
  $new->verified = 0;
  $new->makefile = '';
  $new->frommakefile = array('makefile' => '', 'make_working_copy' => 0);
  $new->make_working_copy = 0;
  $new->no_verify = TRUE;
  $fields = hosting_tasks_extra_platform_successor_fields();
  $mine = $fields[$source['strategy']];
  $new->{$mine['version']} = array(LANGUAGE_NONE => array(array('value' => $plan['constraint'])));
  $new->{$mine['path']} = array(LANGUAGE_NONE => array(array('value' => $plan['folder'])));
  // Every other build's fields empty, with no default either: the backend
  // runs every build whose fields reach it.
  $others = array('field_git_repository_url', 'field_git_repository_path', 'field_git_reference', 'field_git_docroot');
  foreach ($fields as $strategy => $parts) {
    if ($strategy !== $source['strategy']) {
      $others = array_merge($others, array_values($parts));
    }
  }
  foreach ($others as $field) {
    $new->{$field} = array();
  }
  $transaction = db_transaction();
  try {
    node_save($new);
    $nid = isset($new->nid) ? (int) $new->nid : 0;
    if (!$nid) {
      throw new Exception('the panel made no record of it');
    }
    // Who may see it is who may see the platform it is built from.
    if (!empty($platform->clients) && is_array($platform->clients) && db_table_exists('hosting_platform_client_access')) {
      db_delete('hosting_platform_client_access')->condition('pid', $nid)->execute();
      foreach (array_keys($platform->clients) as $cid) {
        db_insert('hosting_platform_client_access')->fields(array('pid' => $nid, 'cid' => (int) $cid))->execute();
      }
    }
    $check = node_load($nid, NULL, TRUE);
    if (!is_object($check) || !isset($check->hosting_name) || $check->hosting_name !== 'platform_' . $plan['context']
      || (int) $check->platform_status !== HOSTING_PLATFORM_LOCKED) {
      throw new Exception('its record did not come out as asked');
    }
  }
  catch (Exception $e) {
    $transaction->rollback();
    entity_get_controller('node')->resetCache();
    return 'the new platform could not be made: ' . $e->getMessage();
  }
  // An Error left uncaught would end the transaction's object, which
  // commits what was written so far. PHP 5.6 has no such class and never
  // matches it.
  catch (Throwable $e) {
    $transaction->rollback();
    entity_get_controller('node')->resetCache();
    return 'the new platform could not be made: ' . $e->getMessage();
  }
  unset($transaction);
  return $check;
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_tasks_extra_post_hosting_platform_successor_task($task, $data) {
  hosting_tasks_extra_platform_successor_end($task, TRUE);
}

/**
 * Implements hook_hosting_TASK_TYPE_task_rollback().
 *
 * Also called when the post hook ends the row in error.
 */
function hosting_tasks_extra_hosting_platform_successor_task_rollback($task, $data) {
  hosting_tasks_extra_platform_successor_end($task, FALSE);
}

/**
 * Records how a Build newer version left the platform it made.
 *
 * The backend judges a removal by the disk and its own step; the panel's
 * record is read only here. A run that ended before it said anything (its
 * backend never started, say) says here what it left.
 */
function hosting_tasks_extra_platform_successor_end($task, $ok) {
  $done = &drupal_static(__FUNCTION__, array());
  if (isset($done[$task->nid])) {
    return;
  }
  $done[$task->nid] = TRUE;
  hosting_tasks_extra_drupal_upgrade_cancel_steps($task, 'Build newer version');
  if (empty($task->task_args['successor']) || empty($task->task_args['successor_run']) || (int) $task->task_args['successor_run'] !== (int) $task->nid) {
    return;
  }
  $made = node_load((int) $task->task_args['successor'], NULL, TRUE);
  $state = !is_object($made) ? 'gone' : (((int) $made->platform_status === HOSTING_PLATFORM_DELETED) ? 'deleted' : (((int) $made->platform_status === HOSTING_PLATFORM_ENABLED) ? 'enabled' : 'locked'));
  hosting_tasks_extra_task_args_keep($task, array('successor_end' => $state));
  if ($ok || $state === 'deleted' || $state === 'gone') {
    return;
  }
  $said = db_query_range("SELECT 1 FROM {hosting_task_log} WHERE vid = :vid AND message LIKE :result", 0, 1, array(
    ':vid' => (int) $task->vid,
    ':result' => '%SUCCESSOR/RESULT%',
  ))->fetchField();
  // A runner in backend mode keeps an error it was sent as an error alone,
  // with no line of the log yet.
  foreach (drush_get_context('DRUSH_ERROR_LOG', array()) as $messages) {
    foreach ((array) $messages as $message) {
      if (strpos((string) $message, 'SUCCESSOR/RESULT') !== FALSE) {
        $said = TRUE;
      }
    }
  }
  if ($said) {
    return;
  }
  // The backend never reached its plan (an overloaded server ends it at its
  // start, say): the platform it was to build was never verified and holds
  // nothing on disk, so its record goes as a Delete of its own.
  $folder = isset($task->task_args['successor_path']) ? (string) $task->task_args['successor_path'] : '';
  $marker = ($folder === '') ? '' : dirname($folder) . '/.' . basename($folder) . '.unfinished-build';
  $alias = drush_server_home() . '/.drush/' . $made->hosting_name . '.alias.drushrc.php';
  if ($state === 'locked' && empty($made->verified) && $folder !== '' && !file_exists($folder) && !is_link($folder)
    && !file_exists($marker) && !is_link($marker) && !file_exists($alias)) {
    $delete = hosting_add_task($made->nid, 'delete', array('upgrade_run' => (int) $task->nid));
    drush_log(dt('SUCCESSOR/RESULT Not started: the run ended before its checks. The platform record made for it, @title, was never built and holds nothing on disk: @next', array(
      '@title' => _hosting_tasks_extra_plain($made->title),
      '@next' => is_object($delete) ? 'its Delete is queued.' : 'the account\'s main login can delete it.',
    )), 'error');
    return;
  }
  drush_log(dt('SUCCESSOR/RESULT Not kept: the run ended before it said what happened. @title at @path is left, @state; the account\'s main login can delete it.', array(
    '@title' => _hosting_tasks_extra_plain($made->title),
    '@path' => _hosting_tasks_extra_plain($made->publish_path),
    '@state' => ($state === 'enabled') ? 'enabled' : 'Locked',
  )), 'error');
}

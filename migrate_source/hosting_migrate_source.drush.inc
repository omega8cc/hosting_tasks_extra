<?php

/**
 * @file
 * Frontend-side backend plumbing for the migration source association.
 */

/**
 * Implements hook_hosting_site_context_options().
 *
 * Only verify and install carry provision_save for sites, and the
 * migrate_source task rides provision-verify, so this fires exactly when the
 * emitted settings block is regenerated.
 *
 * On the migrate_source run itself the association has not been persisted yet
 * (that happens in the post-task hook), so the task's own argument wins; every
 * later verify uses the stored association.
 */
function hosting_migrate_source_hosting_site_context_options(&$task) {
  $source_nid = 0;

  if (isset($task->task_type) && $task->task_type == 'migrate_source'
    && isset($task->task_args['source'])) {
    $source_nid = intval($task->task_args['source']);
  }
  elseif (isset($task->ref->migrate_source)) {
    $source_nid = intval($task->ref->migrate_source);
  }

  // Re-validate on every push: the association is long-lived and the client's
  // sites change under it. A source that is no longer eligible is cleared
  // rather than carried. Structural check only -- this runs inside a drush task
  // with no interactive user, where node_access() would evaluate against the
  // anonymous user and clear every association.
  if ($source_nid && !hosting_migrate_source_source_eligible($task->ref, $source_nid)) {
    $source_nid = 0;
  }

  $db_name = '';
  $uri = '';
  if ($source_nid) {
    $source = node_load($source_nid);
    // hosting_site.db_name is the account-owned record of the source database,
    // refreshed from the backend on every verify/install/import. It is NOT
    // authoritative on its own -- the backend re-checks it against MySQL's own
    // privilege tables before granting anything.
    if (!empty($source->db_name)) {
      $db_name = $source->db_name;
      $uri = !empty($source->hosting_name) ? $source->hosting_name : $source->title;
    }
  }

  // 'null' is the house idiom for "unset this in the backend context"; an empty
  // string would be retained.
  $task->context_options['migrate_source_db'] = ($db_name === '') ? 'null' : $db_name;
  $task->context_options['migrate_source_uri'] = ($uri === '') ? 'null' : $uri;
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_migrate_source_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    if (isset($context->migrate_source_db) && !empty($context->migrate_source_db)) {
      $node->migrate_source_db = $context->migrate_source_db;
    }
  }
}

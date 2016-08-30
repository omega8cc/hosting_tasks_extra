<?php
/**
 * @file
 *   Expose the fix-ownership feature to hostmaster.
 */

/**
 * Implements hook_hosting_feature().
 */
function hosting_fix_ownership_hosting_feature() {
  $features['fix_ownership'] = array(
    'title' => t('Fix Ownership'),
    'description' => t("Task that fixes file ownership of Drupal platforms and sites."),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'fix_ownership',
    'group' => 'experimental',
  );
  return $features;
}


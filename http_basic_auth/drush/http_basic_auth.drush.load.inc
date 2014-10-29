<?php

/**
 * Implements hook_drush_load().
 *
 * In a drush contrib check if the frontend part (hosting_hook variant) is enabled.
 */
function http_basic_auth_drush_load() {
  $features = drush_get_option('hosting_features', array());
  return array_key_exists('http_basic_auth', $features) && $features['http_basic_auth'];
}


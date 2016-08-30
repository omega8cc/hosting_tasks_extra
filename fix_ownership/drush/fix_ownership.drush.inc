<?php
/**
 * @file Drush hooks for the fix_ownership extension.
 */

function drush_fix_ownership_pre_provision_verify() {
  if (d()->type === 'platform') {
    drush_shell_exec("sudo /usr/local/bin/fix-ownership.sh --drupal_path=%s --drupal_user=%s --httpd_group=%s", d()->root, d()->server->script_user, d()->server->web_group);
    $result = drush_shell_exec_output();
    foreach ($result as $line) {
      drush_log($line);
    }
  }
}

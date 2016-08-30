<?php
/**
 * @file Drush hooks for the fix_ownership extension.
 */

function drush_fix_ownership_pre_provision_verify() {
  if (d()->type === 'platform') {
    $success = drush_shell_exec("sudo /usr/local/bin/fix-ownership.sh --root=%s --script-user=%s --web-group=%s", d()->root, d()->server->script_user, d()->server->web_group);
    $result = drush_shell_exec_output();
    foreach ($result as $index => $line) {
      if (!$success) { // Log the entire error as a warning.
        $line_status = 'warning';
      }
      else { // Only log the last line as success.
        $line_status = $index+1 == count($result) ? 'success' : 'notice';
      }
      drush_log($line, $line_status);
    }
  }
}

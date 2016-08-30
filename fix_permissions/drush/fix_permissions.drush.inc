<?php
/**
 * @file Drush hooks for the fix_permissions extension.
 */

function drush_fix_permissions_pre_provision_verify() {
  if (d()->type === 'platform') {
    $success = drush_shell_exec("sudo /usr/local/bin/fix-permissions.sh --root=%s", d()->root);
  }
  elseif (d()->type === 'site') {
    $success = drush_shell_exec("sudo /usr/local/bin/fix-permissions.sh --root=%s", d()->root);
  }
  else {
    return TRUE;
  }

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

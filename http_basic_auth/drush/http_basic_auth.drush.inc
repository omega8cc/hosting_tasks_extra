<?php

/**
 * Implements hook_drush_init().
 */
function http_basic_auth_drush_init() {
  http_basic_auth_provision_register_autoload();
}

/**
 * Register our directory as a place to find provision classes.
 */
function http_basic_auth_provision_register_autoload() {
  static $loaded = FALSE;
  if (!$loaded) {
    $loaded = TRUE;
    $list = drush_commandfile_list();
    $provision_dir = dirname($list['provision']);
    if (is_readable($provision_dir . '/provision.inc')) {
      include_once($provision_dir . '/provision.inc');
      include_once($provision_dir . '/provision.service.inc');
      if (function_exists('provision_autoload_register_prefix')) {
        provision_autoload_register_prefix('Provision_', dirname(__FILE__));
      }
    }
  }
}

/**
 * Expose the service type this extension defines to provision.
 *
 * @return
 *   An array with the service type the key, and the default implementation the
 *   value.
 */
function http_basic_auth_provision_services() {
  http_basic_auth_provision_register_autoload();
  return array('http_basic_auth' => NULL);
}

/**
 * The password file of a site, or FALSE when the web server's config
 * directory cannot be resolved.
 */
function _http_basic_auth_file_path($vhostd_path, $uri) {
  $app_dir = realpath($vhostd_path . '/..');
  if (empty($app_dir)) {
    return FALSE;
  }
  return "{$app_dir}/passwords.d/{$uri}";
}

/**
 * A SHA-512 crypt hash with a random salt, or FALSE.
 *
 * nginx checks the password of every request inside its worker, so the
 * scheme has to be cheap: SHA-512 costs about a millisecond, bcrypt at cost
 * 10 blocks the worker for tens of milliseconds. $6$ is also the scheme every
 * libcrypt nginx may link against verifies. PHP ships its own SHA-512 crypt,
 * so 5.6 and 8.x write the same hash for the same salt.
 */
function _http_basic_auth_hash($pass) {
  $bytes = FALSE;
  if (function_exists('random_bytes')) {
    try {
      $bytes = random_bytes(12);
    }
    catch (Exception $e) {
      $bytes = FALSE;
    }
  }
  if ($bytes === FALSE && function_exists('openssl_random_pseudo_bytes')) {
    $strong = FALSE;
    $bytes = openssl_random_pseudo_bytes(12, $strong);
    if (!$strong) {
      $bytes = FALSE;
    }
  }
  if ($bytes === FALSE || strlen($bytes) != 12) {
    return FALSE;
  }
  // 12 bytes make 16 base64 characters; '+' is the one outside crypt's
  // alphabet.
  $salt = '$6$' . strtr(base64_encode($bytes), '+', '.') . '$';
  $hash = crypt($pass, $salt);
  if (!is_string($hash) || strpos($hash, $salt) !== 0) {
    return FALSE;
  }
  return $hash;
}

/**
 * Push a password file, or its removal, out to the site's web servers.
 */
function _http_basic_auth_sync($path, $context = NULL) {
  $context = $context ? $context : d();
  // Logic added to handle pack web server setups
  $site_platform = $context->platform->name;
  $web_server = d("$site_platform")->web_server->name;
  $http_service_type = d("$web_server")->http_service_type;
  if ($http_service_type == 'pack') {
    $slave_servers = d("$web_server")->slave_web_servers;
    foreach ($slave_servers as $slave_server) {
      d("$slave_server")->service('http')->sync($path);
    }
  }
  else {
    $context->service('http')->sync($path);
  }
}

/**
 * Write the password file so that only its owner and the web server can read
 * it.
 *
 * On BOA root keeps passwords.d setgid in the web server's own group and
 * closed to other accounts: the file takes that group from the directory and
 * is 0640. Anywhere else, or while the directory is not in that group, the
 * file stays 0644, readable by everyone, as it always was (the group Aegir
 * knows can be the PHP pool's, apache on RHEL, while nginx runs outside it).
 * The file is written aside and renamed into place, so nginx never reads a
 * partial file and the hash is never readable beyond these rights, not even
 * between the write and the mode change.
 */
function _http_basic_auth_write_file($path, $contents) {
  $dir = dirname($path);
  // tempnam() creates the file 0600; it falls back to the system temp
  // directory when $dir is not writable, which must not pass.
  $tmp = @tempnam($dir, '.new.');
  if ($tmp === FALSE || dirname($tmp) !== $dir) {
    if ($tmp !== FALSE) {
      @unlink($tmp);
    }
    return drush_set_error('HTTP_BASIC_AUTH_WRITE', dt('Could not create a file in @dir for the HTTP basic authentication password.', array('@dir' => $dir)));
  }
  if (@file_put_contents($tmp, $contents) !== strlen($contents)) {
    @unlink($tmp);
    return drush_set_error('HTTP_BASIC_AUTH_WRITE', dt('Could not write the HTTP basic authentication password to @path.', array('@path' => $tmp)));
  }

  // The web server's own group, never the per-account group the PHP pools
  // may use as web_group: provision's probe of the usual names, which leaves
  // out the caller's own group.
  $group = function_exists('_provision_default_web_group') ? _provision_default_web_group() : 'www-data';
  $gid = NULL;
  if (!empty($group) && function_exists('posix_getgrnam')) {
    $info = posix_getgrnam($group);
    $gid = $info ? $info['gid'] : NULL;
  }
  clearstatcache();
  $dir_stat = @stat($dir);
  $tmp_stat = @stat($tmp);
  $in_group = $gid !== NULL && $dir_stat && $tmp_stat && ($dir_stat['mode'] & 02000)
    && $dir_stat['gid'] == $gid && $tmp_stat['gid'] == $gid;
  // A directory outside that group (an upgrade's chown of config puts it in
  // the account group for a moment) must still let the web server through:
  // others may traverse, and the file's own mode keeps them out of it. Root
  // puts the directory back in the group on its next pass.
  if ($dir_stat && ($gid === NULL || $dir_stat['gid'] != $gid)) {
    provision_file()->chmod($dir, ($dir_stat['mode'] & 07777) | 0711);
  }
  provision_file()->chmod($tmp, $in_group ? 0640 : 0644);
  if (!$in_group) {
    drush_log(dt('The HTTP basic authentication password file @path is readable by everyone: its directory is not setgid in the web server group.', array('@path' => $path)), 'notice');
  }

  if (!@rename($tmp, $path)) {
    @unlink($tmp);
    return drush_set_error('HTTP_BASIC_AUTH_WRITE', dt('Could not move the HTTP basic authentication password file into place at @path.', array('@path' => $path)));
  }
  return TRUE;
}

function _http_basic_auth_create_file($uri, $data, $user, $pass) {
  $path = _http_basic_auth_file_path($data['http_vhostd_path'], $uri);
  if ($path === FALSE) {
    drush_set_error('HTTP_BASIC_AUTH_PATH', dt('Could not resolve the web server configuration directory above @dir.', array('@dir' => $data['http_vhostd_path'])));
    return FALSE;
  }
  // htpasswd ends the user name at the first ':' and skips a line that
  // starts with '#'.
  if (strpos($user, ':') !== FALSE || strpos($user, '#') === 0 || preg_match('/[\x00-\x1F\x7F]/', $user)) {
    drush_set_error('HTTP_BASIC_AUTH_USER', dt('The HTTP basic authentication username may not contain a colon or control characters, nor start with #.'));
    return $path;
  }
  $app_dir = dirname(dirname($path));

  // The web server workers only need to traverse this directory to reach
  // passwords.d: 0711, the mode the server verify gives it. Nothing here may
  // add group write: the root web server master includes vhost.d below it,
  // and the account's shell user shares the account group.
  // Note that this is not recursive, and the sub directories should keep
  // prying eyes from things they shouldn't be seeing.
  provision_file()->chmod($app_dir, 0711);

  // Make sure the directory exists. One that does keeps its mode: on BOA
  // root keeps it setgid in the web server group, closed to other accounts.
  if (!is_dir(dirname($path))) {
    provision_file()->create_dir(dirname($path), dt('Passwords'), 0711);
  }

  // Generate the pass using php because nginx server may not have apache tools.
  $hash = _http_basic_auth_hash($pass);
  if ($hash === FALSE) {
    drush_set_error('HTTP_BASIC_AUTH_HASH', dt('Could not hash the HTTP basic authentication password: no random source for the salt.'));
    return $path;
  }
  if (!_http_basic_auth_write_file($path, "$user:$hash\n")) {
    return $path;
  }

  // Sync the password out to the server.
  _http_basic_auth_sync($path);

  return $path;
}

/**
 * Remove a site's password file once its basic authentication is off or the
 * site is gone under that name, so no stale hash is left behind.
 */
function _http_basic_auth_remove_file($vhostd_path, $uri, $context = NULL) {
  $path = _http_basic_auth_file_path($vhostd_path, $uri);
  if ($path === FALSE || !is_file($path)) {
    return;
  }
  if (@unlink($path)) {
    drush_log(dt('Removed the HTTP basic authentication password file @path.', array('@path' => $path)), 'info');
    _http_basic_auth_sync($path, $context);
  }
  else {
    drush_log(dt('Could not remove the HTTP basic authentication password file @path.', array('@path' => $path)), 'warning');
  }
}

/**
 * The realm for the vhost. A value nginx or Apache could read as more than
 * text, or nginx's 'off' (which turns the check off), gets the default.
 */
function _http_basic_auth_realm() {
  $message = d()->http_basic_auth_message;
  if (empty($message) || $message === 'off' || preg_match('/[\x00-\x1F\x7F"\\\\$;{}]/', $message)) {
    return dt('Restricted access');
  }
  return $message;
}

/**
 * Remove the password file of a site name that is being retired, deleted or
 * renamed away, unless a site vhost still serves that name. A subdirectory
 * parent's placeholder vhost carries no authentication, so it does not keep
 * the file. A pack or cluster web server has no vhost directory of its own,
 * and its members keep their copies.
 *
 * Cleanup only: the task's error state is kept as it was around it.
 */
function _http_basic_auth_retire_file($context) {
  if (!is_object($context) || $context->type != 'site' || empty($context->uri) || !is_object($context->platform)) {
    return;
  }
  $server = $context->platform->web_server;
  $vhostd_path = is_object($server) ? (string) $server->http_vhostd_path : '';
  if ($vhostd_path === '') {
    return;
  }
  if (function_exists('_provision_vhost_kind')) {
    $kind = _provision_vhost_kind($server, $context->uri);
    if ($kind === NULL || $kind === 'site') {
      return;
    }
  }
  elseif (file_exists("{$vhostd_path}/{$context->uri}")) {
    return;
  }
  $pre_error = drush_get_context('DRUSH_ERROR_CODE', DRUSH_SUCCESS);
  _http_basic_auth_remove_file($vhostd_path, $context->uri, $context);
  drush_set_context('DRUSH_ERROR_CODE', $pre_error);
}

/**
 * Implements drush_hook_post_COMMAND() for provision-delete.
 */
function drush_http_basic_auth_post_provision_delete() {
  _http_basic_auth_retire_file(d());
}

/**
 * Implements drush_hook_post_COMMAND() for provision-migrate.
 *
 * A migrate that renames the site leaves the old name's file behind.
 */
function drush_http_basic_auth_post_provision_migrate($platform, $new_name = NULL) {
  if (!is_null($new_name) && ($new_name != d()->name) && d($new_name)->uri != d()->uri) {
    _http_basic_auth_retire_file(d());
  }
}

/**
 * Implements drush_hook_pre_COMMAND() for provision-save.
 *
 * provision-save --delete retires a context: the Hosting delete task, the
 * Grav and Textpattern renames and the cleanup after a failed copy all end
 * there. This runs before the alias goes, while the site still loads.
 */
function drush_http_basic_auth_pre_provision_save($alias = NULL) {
  if (empty($alias) || !drush_get_option('delete', FALSE)) {
    return;
  }
  $record = drush_sitealias_get_record($alias);
  if (empty($record['context_type']) || $record['context_type'] != 'site' || empty($record['platform'])) {
    return;
  }
  // A site whose platform, web server or database server alias is already
  // gone cannot load (a missing platform is fatal), and its alias must still
  // go.
  $platform = drush_sitealias_get_record($record['platform']);
  if (empty($platform['web_server']) || !drush_sitealias_get_record($platform['web_server'])) {
    return;
  }
  if (!empty($record['db_server']) && !drush_sitealias_get_record($record['db_server'])) {
    return;
  }
  // Cleanup only: neither loading the site nor removing its file may veto
  // the delete.
  $pre_error = drush_get_context('DRUSH_ERROR_CODE', DRUSH_SUCCESS);
  _http_basic_auth_retire_file(d($alias));
  drush_set_context('DRUSH_ERROR_CODE', $pre_error);
}

/*
 * Implementation of hook_provision_apache_vhost_config()
 */
function http_basic_auth_provision_apache_vhost_config($uri, $data) {
  $lines = array();
  $user = d()->http_basic_auth_username;
  $pass = d()->http_basic_auth_password;

  if (!empty($user) && !empty($pass)) {
    // Create the password file.
    $path = _http_basic_auth_create_file($uri, $data, $user, $pass);
    // Fail closed: a task that could not write the file still protects the
    // site, and /dev/null lets no one in until the task is run again.
    if ($path === FALSE) {
      $path = '/dev/null';
    }

    // Now add the file to the vhost:
    $root = d()->root;
    $message = _http_basic_auth_realm();
    $lines[] = "  <Directory \"$root\">";
    $lines[] = "    # HTTP Basic Authentication added by Aegir";
    $lines[] = "    AuthType Basic";
    $lines[] = "    AuthName \"$message\"";

    $lines[] = "    AuthUserFile $path";
    $lines[] = "    Require valid-user";

    if (!empty(d()->http_basic_auth_whitelist)) {

      $lines[] = "    Order deny,allow";
      $lines[] = "    Deny from all";
      $lines[] = "    Allow from " . d()->http_basic_auth_whitelist;
      $lines[] = "    Satisfy Any";
    }
    else {
      $lines[] = "    Satisfy All";
    }
    $lines[] = "  </Directory>";
    $lines[] = "";
  }
  else {
    _http_basic_auth_remove_file($data['http_vhostd_path'], $uri);
  }

  return implode("\n", $lines);
}

/*
 * Implementation of hook_provision_nginx_vhost_config()
 */
function http_basic_auth_provision_nginx_vhost_config($uri, $data) {
  $lines = array();
  $user = d()->http_basic_auth_username;
  $pass = d()->http_basic_auth_password;

  if (!empty($user) && !empty($pass)) {
    // Create the password file.
    $path = _http_basic_auth_create_file($uri, $data, $user, $pass);
    // Fail closed, as in the Apache hook above.
    if ($path === FALSE) {
      $path = '/dev/null';
    }

    // Now add the file to the vhost:
    $message = _http_basic_auth_realm();
    $lines[] = "auth_basic \"$message\";";
    $lines[] = "auth_basic_user_file $path;";
    $lines[] = "";
  }
  else {
    _http_basic_auth_remove_file($data['http_vhostd_path'], $uri);
  }

  return implode("\n", $lines);
}

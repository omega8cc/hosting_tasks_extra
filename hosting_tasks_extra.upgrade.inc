<?php

/**
 * @file
 * What kind of upgrade a move between two codebases is, and what a site allows.
 *
 * Plain PHP with no Drupal and no Drush call, PHP 5.6 syntax: the panel loads
 * this file with module_load_include() and the backend command file with
 * include_once, so both judge a move with the same code. Nothing here runs
 * Composer, opens the network or loads code from a tenant's tree.
 */

/**
 * The bytes of a plain file in a codebase the account can write, or FALSE.
 *
 * This runs as the backend user on a tenant's tree: a link or a FIFO planted
 * under the name is never followed, also when it is swapped in between the check
 * and the open, and the size is bounded. "n" opens without blocking on a FIFO.
 */
function _provision_tasks_extra_read_plain($path, $max = 33554432) {
  $before = @lstat($path);
  if ($before === FALSE || ($before['mode'] & 0170000) !== 0100000 || $before['size'] > $max) {
    return FALSE;
  }
  $handle = @fopen($path, 'rn');
  if ($handle === FALSE) {
    return FALSE;
  }
  $now = fstat($handle);
  if ($now === FALSE || $now['ino'] !== $before['ino'] || $now['dev'] !== $before['dev']) {
    fclose($handle);
    return FALSE;
  }
  $bytes = stream_get_contents($handle, $max);
  fclose($handle);
  return $bytes;
}

/**
 * Whether a path that lstat() does not find is known to be missing.
 *
 * FALSE when a folder on the way cannot be searched: the file may be there.
 */
function _provision_tasks_extra_known_missing($path) {
  $dir = dirname($path);
  while (@lstat($dir) === FALSE && $dir !== '/' && $dir !== '.' && $dir !== '' && dirname($dir) !== $dir) {
    $dir = dirname($dir);
  }
  return @is_dir($dir) && @is_executable($dir);
}

/**
 * The folder holding the platform's composer.lock, or ''.
 *
 * The docroot when it holds the lock and vendor, or the folder above it when the
 * docroot's own autoload.php says so, as a Composer project's scaffold writes it. A
 * lock anywhere else is not this platform's, whatever folder it lies in.
 */
function _provision_tasks_extra_app_root($root) {
  $root = rtrim($root, '/');
  if (is_dir($root . '/vendor') && is_file($root . '/composer.lock')) {
    return $root;
  }
  $autoload = _provision_tasks_extra_read_plain($root . '/autoload.php', 65536);
  if ($autoload !== FALSE && strpos($autoload, '/../vendor/autoload.php') !== FALSE && is_dir(dirname($root) . '/vendor')) {
    return dirname($root);
  }
  return '';
}

/**
 * One codebase's lock as the classifier reads it, or why it cannot be read.
 *
 * packages-dev is read unless the tree's installed.json says it was built
 * without them: the rule the advisory audit applies, so both judge the same
 * packages.
 *
 * @return array
 *   'ok' TRUE with 'packages' (name => lock entry), 'patches' (one entry per
 *   place a patch definition can live) and 'requires' (the names the root
 *   composer.json asks for, NULL when that file cannot be read); or 'ok'
 *   FALSE with 'why'.
 */
function _provision_tasks_extra_lock_load($root) {
  $app = _provision_tasks_extra_app_root($root);
  $text = ($app === '') ? FALSE : _provision_tasks_extra_read_plain($app . '/composer.lock');
  if ($text === FALSE) {
    return array('ok' => FALSE, 'why' => 'no composer.lock');
  }
  $lock = json_decode($text, TRUE);
  if (!is_array($lock) || !isset($lock['packages']) || !is_array($lock['packages'])) {
    return array('ok' => FALSE, 'why' => 'a composer.lock that cannot be parsed');
  }
  $dev = TRUE;
  $installed = _provision_tasks_extra_read_plain($app . '/vendor/composer/installed.json');
  if ($installed !== FALSE) {
    $info = json_decode($installed, TRUE);
    if (is_array($info) && array_key_exists('dev', $info) && $info['dev'] === FALSE) {
      $dev = FALSE;
    }
  }
  $json = NULL;
  $raw = _provision_tasks_extra_read_plain($app . '/composer.json', 4194304);
  if ($raw !== FALSE) {
    $json = json_decode($raw, TRUE);
    if (!is_array($json)) {
      $json = NULL;
    }
  }
  return _provision_tasks_extra_lock_shape($lock, $dev, $json, array(
    'patches.json' => _provision_tasks_extra_lock_patch_file($app, 'patches.json'),
    'patches.lock.json' => _provision_tasks_extra_lock_patch_file($app, 'patches.lock.json'),
    'patches-file' => _provision_tasks_extra_lock_patch_file($app, _provision_tasks_extra_lock_patches_file_name($json)),
  ));
}

/**
 * The same shape from data already in hand.
 *
 * @param array $lock
 *   A decoded composer.lock.
 * @param bool $dev
 *   Whether packages-dev take part.
 * @param array|null $json
 *   The decoded root composer.json, NULL when it could not be read.
 * @param array $files
 *   Patch files by label, each the file's text, '' when there is none, FALSE
 *   when it is there and cannot be read.
 */
function _provision_tasks_extra_lock_shape($lock, $dev, $json, $files = array()) {
  $packages = array();
  $groups = $dev ? array('packages', 'packages-dev') : array('packages');
  foreach ($groups as $group) {
    if (isset($lock[$group]) && is_array($lock[$group])) {
      foreach ($lock[$group] as $p) {
        if (is_array($p) && isset($p['name'], $p['version']) && is_string($p['name']) && is_string($p['version'])) {
          $packages[strtolower($p['name'])] = $p;
        }
      }
    }
  }
  $patches = array();
  if ($json === NULL) {
    $patches['composer.json'] = array('text' => FALSE, 'map' => NULL);
  }
  else {
    $inline = (isset($json['extra']) && is_array($json['extra']) && isset($json['extra']['patches'])) ? $json['extra']['patches'] : array();
    $patches['composer.json'] = array('text' => json_encode($inline), 'map' => is_array($inline) ? $inline : NULL);
  }
  foreach ($files as $label => $text) {
    $map = array();
    if ($text === FALSE) {
      $map = NULL;
    }
    elseif ($text !== '') {
      $doc = json_decode($text, TRUE);
      $map = (is_array($doc) && isset($doc['patches']) && is_array($doc['patches'])) ? $doc['patches'] : NULL;
    }
    $patches[$label] = array('text' => $text, 'map' => $map);
  }
  $requires = NULL;
  if ($json !== NULL) {
    $requires = array();
    foreach (array('require', 'require-dev') as $key) {
      if (isset($json[$key]) && is_array($json[$key])) {
        foreach ($json[$key] as $name => $constraint) {
          $requires[strtolower($name)] = TRUE;
        }
      }
    }
  }
  return array('ok' => TRUE, 'packages' => $packages, 'patches' => $patches, 'requires' => $requires);
}

/**
 * The name of the patches file a root composer.json points at, or ''.
 *
 * Only a plain file name in the project's own folder: a path could lead
 * through a link to a file outside it, so anything else is not read.
 */
function _provision_tasks_extra_lock_patches_file_name($json) {
  if (!is_array($json) || !isset($json['extra']) || !is_array($json['extra']) || !isset($json['extra']['patches-file']) || !is_string($json['extra']['patches-file'])) {
    return '';
  }
  $name = $json['extra']['patches-file'];
  if (!preg_match('/^[A-Za-z0-9_][A-Za-z0-9._-]*$/', $name) || $name === 'patches.json' || $name === 'patches.lock.json' || $name === 'composer.json' || $name === 'composer.lock') {
    return '';
  }
  return $name;
}

/**
 * A patch file's text: '' when there is none, FALSE when it cannot be read.
 */
function _provision_tasks_extra_lock_patch_file($app, $name) {
  if ($name === '' || @lstat($app . '/' . $name) === FALSE) {
    return '';
  }
  return _provision_tasks_extra_read_plain($app . '/' . $name, 4194304);
}

/**
 * Whether a package name is drupal/core or one of its companions.
 */
function _provision_tasks_extra_lock_is_core_name($name) {
  return $name === 'drupal/core' || strpos($name, 'drupal/core-') === 0;
}

/**
 * Whether a lock entry is a Drupal package: core, a companion, or Drupal code.
 */
function _provision_tasks_extra_lock_is_drupal($package) {
  $type = isset($package['type']) && is_string($package['type']) ? $package['type'] : '';
  return _provision_tasks_extra_lock_is_core_name(strtolower($package['name']))
    || in_array($type, array('drupal-module', 'drupal-theme', 'drupal-profile', 'drupal-library'), TRUE);
}

/**
 * For every name a locked package answers to, the packages that answer.
 *
 * A package answers to its own name and to what it provides or replaces.
 */
function _provision_tasks_extra_lock_providers($packages) {
  $index = array();
  foreach ($packages as $name => $p) {
    $index[$name][$name] = TRUE;
    foreach (array('provide', 'replace') as $key) {
      if (isset($p[$key]) && is_array($p[$key])) {
        foreach ($p[$key] as $other => $constraint) {
          $index[strtolower($other)][$name] = TRUE;
        }
      }
    }
  }
  return $index;
}

/**
 * The packages reached from some packages over require edges.
 *
 * @param array $packages
 *   name => lock entry.
 * @param array $from
 *   The names to start at; each is in the result when it is locked.
 * @param array $stop
 *   name => TRUE for packages the walk never enters.
 * @param array|null $index
 *   _provision_tasks_extra_lock_providers() of the same packages, when the
 *   caller walks them more than once.
 *
 * @return array
 *   name => TRUE.
 */
function _provision_tasks_extra_lock_reach($packages, $from, $stop = array(), $index = NULL) {
  if ($index === NULL) {
    $index = _provision_tasks_extra_lock_providers($packages);
  }
  $seen = array();
  $todo = array();
  foreach ($from as $name) {
    if (isset($packages[$name])) {
      $todo[] = $name;
    }
  }
  while ($todo) {
    $name = array_pop($todo);
    if (isset($seen[$name])) {
      continue;
    }
    $seen[$name] = TRUE;
    if (isset($packages[$name]['require']) && is_array($packages[$name]['require'])) {
      foreach ($packages[$name]['require'] as $wanted => $constraint) {
        $wanted = strtolower($wanted);
        if (isset($index[$wanted])) {
          foreach ($index[$wanted] as $holder => $true) {
            if (!isset($seen[$holder]) && !isset($stop[$holder])) {
              $todo[] = $holder;
            }
          }
        }
      }
    }
  }
  return $seen;
}

/**
 * The core set of one lock.
 *
 * drupal/core, its companions and everything they require, closed over require
 * edges; and the names those packages replace, which are no packages of the
 * lock and serve only to place advisories.
 *
 * @param array $packages
 *   name => lock entry.
 * @param bool $tooling
 *   FALSE leaves out what only the development companion (drupal/core-dev)
 *   brings: its advisories are no core advisories.
 * @param array|null $index
 *   See _provision_tasks_extra_lock_reach().
 *
 * @return array
 *   'packages' and 'names', each name => TRUE.
 */
function _provision_tasks_extra_lock_core_set($packages, $tooling = TRUE, $index = NULL) {
  $roots = array();
  $names = array();
  foreach ($packages as $name => $p) {
    if (!_provision_tasks_extra_lock_is_core_name($name)) {
      continue;
    }
    if (!$tooling && strpos($name, 'drupal/core-dev') === 0) {
      continue;
    }
    $roots[] = $name;
    if (isset($p['replace']) && is_array($p['replace'])) {
      foreach ($p['replace'] as $other => $constraint) {
        $names[strtolower($other)] = TRUE;
      }
    }
  }
  return array('packages' => _provision_tasks_extra_lock_reach($packages, $roots, array(), $index), 'names' => $names);
}

/**
 * Whether a locked version is a development version.
 */
function _provision_tasks_extra_lock_is_dev($version) {
  return strpos($version, 'dev-') === 0 || substr($version, -4) === '-dev';
}

/**
 * What a lock entry was built from: its tag or commit, '' when it says neither.
 */
function _provision_tasks_extra_lock_reference($package) {
  foreach (array('source', 'dist') as $key) {
    if (isset($package[$key]) && is_array($package[$key]) && isset($package[$key]['reference']) && is_string($package[$key]['reference']) && $package[$key]['reference'] !== '') {
      return $package[$key]['reference'];
    }
  }
  return '';
}

/**
 * The major and minor number of a locked version, or NULL.
 */
function _provision_tasks_extra_lock_numbers($version) {
  $m = array();
  if (!preg_match('/^[vV]?([0-9]+)(?:\.([0-9]+))?/', $version, $m)) {
    return NULL;
  }
  return array((int) $m[1], isset($m[2]) ? (int) $m[2] : 0);
}

/**
 * The step between two lock entries of one package.
 *
 * @return array
 *   'step': 'none', 'patch', 'minor', 'major' or 'unknown'; 'lower': whether
 *   the second is the older version.
 */
function _provision_tasks_extra_lock_step($a, $b) {
  $va = $a['version'];
  $vb = $b['version'];
  $ra = _provision_tasks_extra_lock_reference($a);
  $rb = _provision_tasks_extra_lock_reference($b);
  if (_provision_tasks_extra_lock_is_dev($va) || _provision_tasks_extra_lock_is_dev($vb)) {
    if ($va === $vb && $ra === $rb) {
      return array('step' => 'none', 'lower' => FALSE);
    }
    // The same commit under two names is the same code.
    if ($va !== $vb && $ra !== '' && $ra === $rb) {
      return array('step' => 'patch', 'lower' => FALSE);
    }
    return array('step' => 'unknown', 'lower' => FALSE);
  }
  if ($va === $vb) {
    return array('step' => ($ra === $rb) ? 'none' : 'patch', 'lower' => FALSE);
  }
  $na = _provision_tasks_extra_lock_numbers($va);
  $nb = _provision_tasks_extra_lock_numbers($vb);
  if ($na === NULL || $nb === NULL) {
    return array('step' => 'unknown', 'lower' => FALSE);
  }
  $lower = version_compare(ltrim($vb, 'vV'), ltrim($va, 'vV'), '<');
  if ($na[0] !== $nb[0]) {
    return array('step' => 'major', 'lower' => $lower);
  }
  return array('step' => ($na[1] !== $nb[1]) ? 'minor' : 'patch', 'lower' => $lower);
}

/**
 * Orders the steps: a larger number is the larger move.
 *
 * A step that cannot be told ranks above every known one where a list is
 * asked whether it admits it.
 */
function _provision_tasks_extra_lock_rank($step) {
  $rank = array('none' => 0, 'patch' => 1, 'minor' => 2, 'major' => 3, 'unknown' => 4);
  return isset($rank[$step]) ? $rank[$step] : 4;
}

/**
 * The larger of two steps for a move's level.
 *
 * A known major stands whatever else cannot be told: nothing is larger.
 */
function _provision_tasks_extra_lock_larger($one, $two) {
  if ($one === 'major' || $two === 'major') {
    return 'major';
  }
  return (_provision_tasks_extra_lock_rank($two) > _provision_tasks_extra_lock_rank($one)) ? $two : $one;
}

/**
 * The packages whose patch definitions differ between two codebases.
 *
 * @return array
 *   'packages' (name => TRUE) and 'unknown': TRUE when a definition changed
 *   in a place that cannot be read package by package.
 */
function _provision_tasks_extra_lock_patch_changes($source, $target) {
  $changed = array();
  $unknown = FALSE;
  $labels = array_unique(array_merge(array_keys($source['patches']), array_keys($target['patches'])));
  foreach ($labels as $label) {
    $s = isset($source['patches'][$label]) ? $source['patches'][$label] : array('text' => '', 'map' => array());
    $t = isset($target['patches'][$label]) ? $target['patches'][$label] : array('text' => '', 'map' => array());
    if ($s['text'] !== FALSE && $s['text'] === $t['text']) {
      continue;
    }
    if ($s['map'] === NULL || $t['map'] === NULL) {
      $unknown = TRUE;
      continue;
    }
    $names = array_unique(array_merge(array_keys($s['map']), array_keys($t['map'])));
    foreach ($names as $name) {
      $one = isset($s['map'][$name]) ? json_encode($s['map'][$name]) : '';
      $two = isset($t['map'][$name]) ? json_encode($t['map'][$name]) : '';
      if ($one !== $two) {
        $changed[strtolower($name)] = TRUE;
      }
    }
  }
  return array('packages' => $changed, 'unknown' => $unknown);
}

/**
 * The (advisory, package) pairs of one advisory report, or FALSE.
 *
 * The report is the one the updates report logs: "advisories: N", one
 * tab-separated row per finding, then lines that start with "#". A pair is a
 * row's first two columns, the list's prefix on the advisory id included, and
 * a finding listed twice is two pairs. FALSE when the rows do not add up to
 * the count: a report cut short is no report.
 */
function _provision_tasks_extra_audit_pairs($lines) {
  $m = array();
  if (!is_array($lines) || !isset($lines[0]) || !is_string($lines[0]) || !preg_match('/^advisories: ([0-9]+)$/', $lines[0], $m)) {
    return FALSE;
  }
  $pairs = array();
  foreach (array_slice($lines, 1) as $line) {
    if (!is_string($line) || $line === '' || $line[0] === '#') {
      continue;
    }
    $cols = explode("\t", $line);
    if (count($cols) < 2) {
      return FALSE;
    }
    $pairs[] = $cols[0] . "\t" . strtolower($cols[1]);
  }
  if (count($pairs) !== (int) $m[1]) {
    return FALSE;
  }
  sort($pairs);
  return $pairs;
}

/**
 * What is in the first list of pairs and not in the second, counted.
 */
function _provision_tasks_extra_audit_minus($one, $two) {
  $left = array_count_values($two);
  $out = array();
  foreach ($one as $pair) {
    if (!empty($left[$pair])) {
      $left[$pair]--;
    }
    else {
      $out[] = $pair;
    }
  }
  return $out;
}

/**
 * Compares two advisory reports.
 *
 * @param array|null $source
 *   The audit of the source ('lines', and 'why' when there are none), NULL
 *   when no audit is at hand.
 * @param array|null $target
 *   The same for the target.
 *
 * @return array
 *   'known' FALSE with 'why'; or 'known' TRUE with 'before' and 'after' (the
 *   two lists of pairs), 'added' and 'cleared'.
 */
function _provision_tasks_extra_audit_compare($source, $target) {
  $sides = array('the current platform' => $source, 'the new platform' => $target);
  $pairs = array();
  foreach ($sides as $side => $audit) {
    if ($audit === NULL) {
      return array('known' => FALSE, 'why' => 'no security audit of ' . $side . ' at hand');
    }
    if (!is_array($audit) || !isset($audit['lines']) || !is_array($audit['lines'])) {
      $why = (is_array($audit) && isset($audit['why']) && is_string($audit['why']) && $audit['why'] !== '') ? ': ' . $audit['why'] : '';
      return array('known' => FALSE, 'why' => 'the security audit of ' . $side . ' did not finish' . $why);
    }
    $list = _provision_tasks_extra_audit_pairs($audit['lines']);
    if ($list === FALSE) {
      return array('known' => FALSE, 'why' => 'the security audit of ' . $side . ' is not whole');
    }
    $pairs[] = $list;
  }
  return array(
    'known' => TRUE,
    'before' => $pairs[0],
    'after' => $pairs[1],
    'added' => _provision_tasks_extra_audit_minus($pairs[1], $pairs[0]),
    'cleared' => _provision_tasks_extra_audit_minus($pairs[0], $pairs[1]),
  );
}

/**
 * What kind of move it is from one loaded lock to another.
 *
 * @param array $source
 *   _provision_tasks_extra_lock_load() of the site's current codebase.
 * @param array $target
 *   The same for the codebase it would move onto.
 * @param array|null $source_audit
 *   The audit of the source, NULL when none is at hand.
 * @param array|null $target_audit
 *   The same for the target.
 *
 * @return array
 *   'scope': core, contrib, full, unchanged or unknown. 'level': patch, minor,
 *   major, unknown, '' when nothing changed. 'security': TRUE, FALSE, or NULL
 *   when it cannot be told, with 'security_why'. 'why' for an unknown scope
 *   or level. 'downgrades' and 'lower_libraries' (name => "a to b").
 *   'changed' (name => what happened to it). 'appeared' and 'gone' (Drupal
 *   packages on one side only). 'named' (what set the level). 'with' (core's
 *   own libraries that crossed a major inside a core move). 'fixed', 'of',
 *   'open', 'added', 'uncovered' (the security half). 'core_move'. 'locks'
 *   FALSE when either side has no lock to read. 'patch_change' when a patch
 *   definition is known to differ (read on both sides, not the same).
 */
function _provision_tasks_extra_lock_classify($source, $target, $source_audit = NULL, $target_audit = NULL) {
  $out = array(
    'scope' => 'unknown',
    'level' => 'unknown',
    'security' => NULL,
    'security_why' => '',
    'why' => '',
    'downgrades' => array(),
    'lower_libraries' => array(),
    'changed' => array(),
    'appeared' => array(),
    'gone' => array(),
    'named' => array(),
    'with' => array(),
    'fixed' => 0,
    'of' => 0,
    'open' => array(),
    'added' => array(),
    'uncovered' => array(),
    'core_move' => FALSE,
    'patches_ignored' => array(),
    'locks' => TRUE,
    'patch_change' => FALSE,
  );
  $missing = array();
  if (empty($source['ok'])) {
    $missing[] = 'the current platform has ' . (isset($source['why']) ? $source['why'] : 'no composer.lock');
  }
  if (empty($target['ok'])) {
    $missing[] = 'the new platform has ' . (isset($target['why']) ? $target['why'] : 'no composer.lock');
  }
  if ($missing) {
    $out['why'] = implode(' and ', $missing);
    $out['security_why'] = $out['why'];
    $out['locks'] = FALSE;
    return $out;
  }
  $S = $source['packages'];
  $T = $target['packages'];
  $sindex = _provision_tasks_extra_lock_providers($S);
  $tindex = _provision_tasks_extra_lock_providers($T);
  $sset = _provision_tasks_extra_lock_core_set($S, TRUE, $sindex);
  $tset = _provision_tasks_extra_lock_core_set($T, TRUE, $tindex);

  // Every changed package, placed and measured.
  $changed = array();
  $names = array_unique(array_merge(array_keys($S), array_keys($T)));
  sort($names);
  foreach ($names as $name) {
    $a = isset($S[$name]) ? $S[$name] : NULL;
    $b = isset($T[$name]) ? $T[$name] : NULL;
    $entry = array(
      'from' => $a ? $a['version'] : '',
      'to' => $b ? $b['version'] : '',
      'drupal' => _provision_tasks_extra_lock_is_drupal($b ? $b : $a),
      // A removed package is placed by the source's closure, any other by
      // the target's.
      'core' => $b ? isset($tset['packages'][$name]) : isset($sset['packages'][$name]),
      'side' => ($a && $b) ? 'both' : ($b ? 'target' : 'source'),
      'lower' => FALSE,
      'patched' => FALSE,
    );
    if ($a && $b) {
      $step = _provision_tasks_extra_lock_step($a, $b);
      if ($step['step'] === 'none') {
        continue;
      }
      $entry['step'] = $step['step'];
      $entry['lower'] = $step['lower'];
    }
    else {
      $entry['step'] = 'side';
    }
    $changed[$name] = $entry;
  }
  $patches = _provision_tasks_extra_lock_patch_changes($source, $target);
  // Known to differ: a place read on both sides whose text is not the same.
  // A place that cannot be read on a side says nothing either way.
  foreach ($source['patches'] as $label => $place) {
    if (isset($target['patches'][$label]) && is_string($place['text']) && is_string($target['patches'][$label]['text']) && $place['text'] !== $target['patches'][$label]['text']) {
      $out['patch_change'] = TRUE;
    }
  }
  foreach (array_keys($patches['packages']) as $name) {
    if (isset($changed[$name])) {
      $changed[$name]['patched'] = TRUE;
    }
    elseif (isset($S[$name]) && isset($T[$name])) {
      $changed[$name] = array(
        'from' => $S[$name]['version'],
        'to' => $T[$name]['version'],
        'drupal' => _provision_tasks_extra_lock_is_drupal($T[$name]),
        'core' => isset($tset['packages'][$name]),
        'side' => 'both',
        'lower' => FALSE,
        'patched' => TRUE,
        'step' => 'patch',
      );
    }
    else {
      $out['patches_ignored'][] = $name;
    }
  }
  ksort($changed);
  $out['changed'] = $changed;
  if (!$changed && !$patches['unknown']) {
    $out['scope'] = 'unchanged';
    $out['level'] = '';
    $out['security'] = FALSE;
    $out['security_why'] = 'nothing changed';
    return $out;
  }

  // Lower versions. Only a package entry has a version: a name core merely
  // replaces takes part in no comparison.
  foreach ($changed as $name => $c) {
    if ($c['lower']) {
      $out[$c['drupal'] ? 'downgrades' : 'lower_libraries'][$name] = $c['from'] . ' to ' . $c['to'];
    }
    if ($c['drupal'] && $c['side'] === 'target') {
      $out['appeared'][] = $name;
    }
    if ($c['drupal'] && $c['side'] === 'source') {
      $out['gone'][] = $name;
    }
  }

  // A core move: drupal/core's own major or minor changes.
  $core_step = '';
  if (isset($S['drupal/core'], $T['drupal/core'])) {
    $step = _provision_tasks_extra_lock_step($S['drupal/core'], $T['drupal/core']);
    $core_step = $step['step'];
    $out['core_move'] = ($core_step === 'minor' || $core_step === 'major');
  }

  // The security half.
  $sstop = array();
  foreach ($sset['packages'] as $name => $true) {
    if (_provision_tasks_extra_lock_is_drupal($S[$name])) {
      $sstop[$name] = TRUE;
    }
  }
  $tstop = array();
  foreach ($tset['packages'] as $name => $true) {
    if (_provision_tasks_extra_lock_is_drupal($T[$name])) {
      $tstop[$name] = TRUE;
    }
  }
  $advised = array();
  $compare = _provision_tasks_extra_audit_compare($source_audit, $target_audit);
  if (!$compare['known']) {
    $out['security'] = NULL;
    $out['security_why'] = $compare['why'];
  }
  else {
    $out['of'] = count($compare['before']);
    $out['fixed'] = count($compare['cleared']);
    $out['open'] = $compare['after'];
    $out['added'] = $compare['added'];
    if ($compare['added']) {
      $out['security'] = FALSE;
      $out['security_why'] = 'the new platform adds a security advisory';
    }
    elseif (!$compare['cleared']) {
      $out['security'] = FALSE;
      $out['security_why'] = 'it fixes no security advisory';
    }
    else {
      $sproper = _provision_tasks_extra_lock_core_set($S, FALSE, $sindex);
      $core_advisory = FALSE;
      foreach ($compare['cleared'] as $pair) {
        $cols = explode("\t", $pair);
        $name = $cols[1];
        // A name core replaces carries its advisory for drupal/core.
        if (isset($sset['names'][$name]) || isset($tset['names'][$name])) {
          $name = 'drupal/core';
        }
        $advised[$name] = TRUE;
        if ($name === 'drupal/core' || isset($sproper['packages'][$name])) {
          $core_advisory = TRUE;
        }
      }
      $covered = array();
      if ($core_advisory) {
        $covered = $sset['packages'] + $tset['packages'];
      }
      $covered += _provision_tasks_extra_lock_reach($T, array_keys($advised), $tstop, $tindex);
      // The source lock is walked only for what the move removes: a kept
      // package is covered by what the advised package reaches now.
      foreach (_provision_tasks_extra_lock_reach($S, array_keys($advised), $sstop, $sindex) as $name => $true) {
        if (!isset($T[$name])) {
          $covered[$name] = TRUE;
        }
      }
      foreach ($changed as $name => $c) {
        if (!isset($covered[$name])) {
          $out['uncovered'][] = $name;
        }
      }
      $out['security'] = !$out['uncovered'];
      $out['security_why'] = $out['uncovered'] ? 'it also changes what no fixed advisory covers: ' . _provision_tasks_extra_upgrade_names($out['uncovered']) : '';
    }
  }

  // The scope.
  $inside = FALSE;
  $outside = FALSE;
  $libs_outside = FALSE;
  foreach ($changed as $name => $c) {
    if ($c['drupal']) {
      if ($c['core']) {
        $inside = TRUE;
      }
      else {
        $outside = TRUE;
      }
    }
    elseif (!$c['core']) {
      $libs_outside = TRUE;
    }
  }
  if ($inside && $outside) {
    $out['scope'] = 'full';
  }
  elseif ($inside) {
    $out['scope'] = 'core';
  }
  elseif ($outside) {
    $out['scope'] = 'contrib';
  }
  else {
    $out['scope'] = $libs_outside ? 'contrib' : 'core';
  }

  // What each advised package that moved reaches in the target, and its step:
  // read once, for the modules a security release newly requires.
  $advised_reach = array();
  if ($out['security'] === TRUE) {
    foreach (array_keys($advised) as $from) {
      if (isset($S[$from], $T[$from])) {
        $its = _provision_tasks_extra_lock_step($S[$from], $T[$from]);
        $advised_reach[$from] = array('step' => $its['step'], 'reach' => _provision_tasks_extra_lock_reach($T, array($from), $tstop, $tindex));
      }
    }
  }

  // The level.
  $level = 'none';
  $library_level = 'none';
  foreach ($changed as $name => $c) {
    $step = $c['step'];
    if ($c['drupal']) {
      if ($step === 'side') {
        if ($name === 'drupal/core') {
          $step = 'unknown';
        }
        elseif ($out['core_move'] && _provision_tasks_extra_lock_is_core_name($name)) {
          // A companion that comes or goes with a core release takes core's step.
          $step = $core_step;
        }
        else {
          $step = 'major';
          // A module a security release newly requires takes that release's
          // step, when the move is a security move.
          if ($c['side'] === 'target' && (!is_array($target['requires']) || !isset($target['requires'][$name]))) {
            if ($out['security'] === NULL) {
              $step = 'unknown';
            }
            elseif ($out['security'] === TRUE) {
              $via = '';
              foreach ($advised_reach as $from => $its) {
                if ($from !== $name && isset($its['reach'][$name])) {
                  $via = ($via === '') ? $its['step'] : _provision_tasks_extra_lock_larger($via, $its['step']);
                }
              }
              if ($via !== '' && $via !== 'none') {
                $step = $via;
              }
            }
          }
        }
      }
      if ($step === 'major' || $step === 'unknown') {
        $out['named'][] = $name;
      }
      $level = _provision_tasks_extra_lock_larger($level, $step);
      continue;
    }
    // A library. On one side only it is listed and sets no level.
    if ($step === 'side') {
      continue;
    }
    $library_level = _provision_tasks_extra_lock_larger($library_level, $step);
    if ($step !== 'major' && $step !== 'unknown') {
      continue;
    }
    if ($out['core_move'] && $c['core']) {
      // One of core's own pins, moving with the core release.
      if ($step === 'major') {
        $out['with'][] = $name;
      }
      continue;
    }
    $out['named'][] = $name;
    $level = _provision_tasks_extra_lock_larger($level, $step);
  }
  if (!$inside && !$outside) {
    // Libraries only: their largest step, a patch when they only came or went.
    $level = ($library_level === 'none') ? 'patch' : $library_level;
  }
  $out['level'] = $level;

  if ($patches['unknown']) {
    // One change belongs to no package: what is known stands only where
    // nothing could be added to it.
    $out['why'] = 'a patch definition changed in a file that cannot be read package by package';
    if ($out['scope'] !== 'full') {
      $out['scope'] = 'unknown';
    }
    $out['level'] = _provision_tasks_extra_lock_larger($level, 'unknown');
    if ($out['security'] === TRUE) {
      $out['security'] = NULL;
      $out['security_why'] = $out['why'];
    }
  }
  elseif ($level === 'unknown') {
    $out['why'] = 'the size of the step cannot be told for ' . _provision_tasks_extra_upgrade_names($out['named']);
  }
  return $out;
}

/**
 * What kind of move it is from one codebase to another, read from their locks.
 *
 * @param string $source_root
 *   The docroot of the site's current platform.
 * @param string $target_root
 *   The docroot of the platform it would move onto.
 * @param array|null $source_audit
 *   The audit of the source, NULL when none is at hand.
 * @param array|null $target_audit
 *   The same for the target.
 *
 * @return array
 *   See _provision_tasks_extra_lock_classify().
 */
function _provision_tasks_extra_lock_diff($source_root, $target_root, $source_audit = NULL, $target_audit = NULL) {
  return _provision_tasks_extra_lock_classify(
    _provision_tasks_extra_lock_load($source_root),
    _provision_tasks_extra_lock_load($target_root),
    $source_audit,
    $target_audit
  );
}

/**
 * Up to five names, the rest counted.
 */
function _provision_tasks_extra_upgrade_names($names) {
  $names = array_values($names);
  $text = implode(', ', array_slice($names, 0, 5));
  if (count($names) > 5) {
    $text .= ' and ' . (count($names) - 5) . ' more';
  }
  return $text;
}

/**
 * One line that says what kind of move a classified pair is.
 *
 * "core only, a patch update; fixes 2 security advisories"
 */
function _provision_tasks_extra_upgrade_kind_text($diff) {
  if ($diff['scope'] === 'unchanged') {
    return 'no change';
  }
  $scopes = array('core' => 'core only', 'contrib' => 'modules only', 'full' => 'core and modules');
  $levels = array('patch' => 'a patch update', 'minor' => 'a minor update', 'major' => 'a major update');
  $line = isset($scopes[$diff['scope']]) ? $scopes[$diff['scope']] : 'what it changes cannot be told';
  $line .= ', ' . (isset($levels[$diff['level']]) ? $levels[$diff['level']] : 'an update whose size cannot be told');
  // Core is said by the scope, and so are its companions when they move with
  // a core release; the notes name the rest.
  $named = array();
  foreach ($diff['named'] as $name) {
    if ($name !== 'drupal/core' && !($diff['core_move'] && _provision_tasks_extra_lock_is_core_name($name))) {
      $named[] = $name;
    }
  }
  $new = array_intersect($named, $diff['appeared']);
  $old = array_intersect($named, $diff['gone']);
  $moved = array_diff($named, $new, $old);
  $notes = array();
  if ($new) {
    $notes[] = 'new: ' . _provision_tasks_extra_upgrade_names($new);
  }
  if ($old) {
    $notes[] = 'removed: ' . _provision_tasks_extra_upgrade_names($old);
  }
  if ($moved && ($diff['level'] === 'major' || $diff['level'] === 'unknown')) {
    $notes[] = _provision_tasks_extra_upgrade_names($moved);
  }
  if ($notes) {
    $line .= ' (' . implode('; ', $notes) . ')';
  }
  if ($diff['with']) {
    $line .= '; with it a major update of ' . _provision_tasks_extra_upgrade_names($diff['with']);
  }
  if ($diff['security'] === TRUE) {
    if ($diff['fixed'] === $diff['of']) {
      $line .= '; fixes ' . $diff['fixed'] . ' security ' . ($diff['fixed'] === 1 ? 'advisory' : 'advisories');
    }
    else {
      $open = array();
      foreach ($diff['open'] as $pair) {
        $cols = explode("\t", $pair);
        $open[] = $cols[0] . ' on ' . $cols[1];
      }
      $line .= '; fixes ' . $diff['fixed'] . ' of ' . $diff['of'] . ' security advisories; still open: ' . _provision_tasks_extra_upgrade_names($open);
    }
  }
  return $line;
}

/**
 * Reads the drupal_upgrade_modes line of a control file's text.
 *
 * Exactly one line of the form  drupal_upgrade_modes = "words"  (the quotes
 * optional), the key at the start of the line and nothing after the value.
 * Any other line that sets the key makes the text unreadable: a malformed
 * line is never read as an absent key. A line whose first character is ; or
 * # is a comment.
 *
 * @return array
 *   'state': 'absent', 'off' (an empty value, or none), 'list' with 'words',
 *   or 'unreadable' with 'why' and, for a wrong word, 'word'. 'line' is the
 *   line's number in every state but absent.
 */
function _provision_tasks_extra_upgrade_modes_parse($text) {
  $known = array('core', 'contrib', 'full', 'patch', 'minor', 'major', 'security', 'none');
  $found = NULL;
  $number = 0;
  // PHP's own reader skips a byte order mark and takes any line end.
  if (strncmp($text, "\xEF\xBB\xBF", 3) === 0) {
    $text = (string) substr($text, 3);
  }
  foreach (preg_split('/\r\n|\r|\n/', $text) as $line) {
    $number++;
    if ($line === '' || $line[0] === ';' || $line[0] === '#') {
      continue;
    }
    $eq = strpos($line, '=');
    $key = strtolower(($eq === FALSE) ? $line : substr($line, 0, $eq));
    // Any line whose key part holds the name as a whole word is taken as an
    // attempt to set it: indented, in capitals, an array form, or with bytes
    // before it that an editor does not show.
    if (!preg_match('/(?<![a-z0-9_])drupal_upgrade_modes(?![a-z0-9_])/', $key)) {
      continue;
    }
    $key = trim($key);
    if ($found !== NULL) {
      return array('state' => 'unreadable', 'line' => $number, 'why' => 'the key is set twice (also on line ' . $found['line'] . ')');
    }
    $m = array();
    if (!preg_match('/^drupal_upgrade_modes[ \t]*=[ \t]*(?:"([^"]*)"|([^"]*?))[ \t]*$/', $line, $m)) {
      if ($line[0] === ' ' || $line[0] === "\t") {
        $why = 'the line is indented';
      }
      elseif ($eq === FALSE) {
        $why = 'there is no = after the key';
      }
      elseif (strpos($line, 'drupal_upgrade_modes') !== 0 || $key !== 'drupal_upgrade_modes') {
        $why = 'the key must be written as drupal_upgrade_modes, in lower case';
      }
      elseif (substr_count($line, '"') === 1) {
        $why = 'a quote is not closed';
      }
      else {
        $why = 'there is text after the value';
      }
      return array('state' => 'unreadable', 'line' => $number, 'why' => $why);
    }
    $value = (isset($m[2]) && $m[2] !== '') ? $m[2] : $m[1];
    $words = array();
    if (trim($value) !== '') {
      foreach (explode(',', $value) as $word) {
        $word = strtolower(trim($word));
        if (!in_array($word, $known, TRUE)) {
          return array(
            'state' => 'unreadable',
            'line' => $number,
            'why' => "an unknown word '" . preg_replace('/[^\x20-\x7e]/', '?', substr($word, 0, 40)) . "'",
            'word' => $word,
          );
        }
        $words[$word] = TRUE;
      }
    }
    if (isset($words['none']) && count($words) > 1) {
      return array('state' => 'unreadable', 'line' => $number, 'why' => "'none' beside another word", 'word' => 'none');
    }
    if (!$words || isset($words['none'])) {
      $found = array('state' => 'off', 'line' => $number);
    }
    else {
      $found = array('state' => 'list', 'line' => $number, 'words' => array_keys($words));
    }
  }
  return ($found === NULL) ? array('state' => 'absent') : $found;
}

/**
 * Reads the drupal_upgrade_modes line of a control file.
 *
 * Never parse_ini_file(): one bad character elsewhere in the file makes that
 * return nothing, and a list that reads as absent accepts everything. A file
 * that is not there is an absent list; one that is there and cannot be read
 * as a plain file, or whose folder cannot be searched, is unreadable.
 *
 * @return array
 *   See _provision_tasks_extra_upgrade_modes_parse(); 'file' is added.
 */
function _provision_tasks_extra_upgrade_modes($file) {
  if (strpos($file, "\0") !== FALSE) {
    return array('state' => 'unreadable', 'line' => 0, 'why' => 'its name is no file name', 'file' => '');
  }
  clearstatcache(TRUE, $file);
  if (@lstat($file) === FALSE) {
    if (!_provision_tasks_extra_known_missing($file)) {
      return array('state' => 'unreadable', 'line' => 0, 'why' => 'its folder cannot be read', 'file' => $file);
    }
    return array('state' => 'absent', 'file' => $file);
  }
  $text = _provision_tasks_extra_read_plain($file, 1048576);
  if ($text === FALSE) {
    return array('state' => 'unreadable', 'line' => 0, 'why' => 'the file cannot be read', 'file' => $file);
  }
  $modes = _provision_tasks_extra_upgrade_modes_parse($text);
  $modes['file'] = $file;
  return $modes;
}

/**
 * What one list allows, as its two axes and the security filter.
 *
 * @return array
 *   'scope': NULL when unrestricted, else 'core' or 'contrib'. 'level': NULL
 *   when unrestricted, else the widest step allowed. 'security': whether only
 *   security moves pass.
 */
function _provision_tasks_extra_upgrade_modes_axes($words) {
  $has = array_flip($words);
  $scope = NULL;
  if (!isset($has['full']) && !(isset($has['core']) && isset($has['contrib']))) {
    if (isset($has['core'])) {
      $scope = 'core';
    }
    elseif (isset($has['contrib'])) {
      $scope = 'contrib';
    }
  }
  $level = NULL;
  foreach (array('patch', 'minor', 'major') as $word) {
    if (isset($has[$word])) {
      $level = $word;
    }
  }
  $security = isset($has['security']);
  if ($security && $level === NULL) {
    $level = 'minor';
  }
  // The widest word leaves the axis unrestricted.
  if ($level === 'major') {
    $level = NULL;
  }
  return array('scope' => $scope, 'level' => $level, 'security' => $security);
}

/**
 * Whether one list admits a kind of move.
 *
 * @param array $modes
 *   _provision_tasks_extra_upgrade_modes() of one control file.
 * @param array $diff
 *   _provision_tasks_extra_lock_classify() of the move.
 *
 * @return array
 *   'allowed'; when FALSE, 'axis' (off, unreadable, scope, level, security)
 *   and 'why', a clause that completes "this list ...".
 */
function _provision_tasks_extra_upgrade_modes_allow($modes, $diff) {
  if ($modes['state'] === 'absent') {
    return array('allowed' => TRUE);
  }
  if ($modes['state'] === 'off') {
    return array('allowed' => FALSE, 'axis' => 'off', 'why' => 'switches upgrades off');
  }
  if ($modes['state'] !== 'list') {
    return array('allowed' => FALSE, 'axis' => 'unreadable', 'why' => 'cannot be read: ' . (isset($modes['why']) ? $modes['why'] : 'unknown reason'));
  }
  $axes = _provision_tasks_extra_upgrade_modes_axes($modes['words']);
  if ($axes['scope'] !== NULL && $diff['scope'] !== 'unchanged' && $diff['scope'] !== $axes['scope']) {
    $what = array('core' => 'core', 'contrib' => 'modules', 'full' => 'core and modules', 'unknown' => 'something that cannot be told');
    return array(
      'allowed' => FALSE,
      'axis' => 'scope',
      'why' => 'accepts changes of ' . ($axes['scope'] === 'core' ? 'core' : 'modules') . ' only, and this one changes ' . (isset($what[$diff['scope']]) ? $what[$diff['scope']] : $diff['scope']),
    );
  }
  if ($axes['level'] !== NULL && $diff['level'] !== '' && _provision_tasks_extra_lock_rank($diff['level']) > _provision_tasks_extra_lock_rank($axes['level'])) {
    $size = array('minor' => 'a minor update', 'major' => 'a major update', 'unknown' => 'an update whose size cannot be told');
    return array(
      'allowed' => FALSE,
      'axis' => 'level',
      'why' => 'accepts nothing larger than a ' . $axes['level'] . ' update, and this is ' . (isset($size[$diff['level']]) ? $size[$diff['level']] : $diff['level']),
    );
  }
  if ($axes['security'] && $diff['security'] !== TRUE) {
    return array(
      'allowed' => FALSE,
      'axis' => 'security',
      'why' => 'accepts security fixes only, and ' . ($diff['security'] === NULL ? 'whether this is one could not be checked' : 'this is not one') . ($diff['security_why'] !== '' ? ' (' . $diff['security_why'] . ')' : ''),
    );
  }
  return array('allowed' => TRUE);
}

/**
 * What one list allows, in words, for the plan and the log.
 */
function _provision_tasks_extra_upgrade_modes_text($modes) {
  if ($modes['state'] === 'absent') {
    return 'no list: every upgrade is accepted';
  }
  if ($modes['state'] === 'off') {
    return 'upgrades are switched off';
  }
  if ($modes['state'] !== 'list') {
    return 'cannot be read (line ' . $modes['line'] . ': ' . $modes['why'] . ')';
  }
  $axes = _provision_tasks_extra_upgrade_modes_axes($modes['words']);
  $parts = array();
  $parts[] = ($axes['scope'] === NULL) ? 'core and modules' : ($axes['scope'] === 'core' ? 'core only' : 'modules only');
  $parts[] = ($axes['level'] === NULL) ? 'any size' : 'up to a ' . $axes['level'] . ' update';
  if ($axes['security']) {
    $parts[] = 'security fixes only';
  }
  return implode(', ', $parts);
}

/**
 * The number a module's install file gives as the last update it removed.
 *
 * Read as text, with the expression provision's platform scan uses: no code
 * from the tree is loaded.
 *
 * @return int|false|null
 *   The number; NULL when there is no such file or it names no such function;
 *   FALSE when the file cannot be read or reached, or names the function in a
 *   shape the expression does not match.
 */
function _provision_tasks_extra_update_last_removed($install_file, $module) {
  if (@lstat($install_file) === FALSE) {
    return _provision_tasks_extra_known_missing($install_file) ? NULL : FALSE;
  }
  $source = _provision_tasks_extra_read_plain($install_file, 8388608);
  if ($source === FALSE) {
    return FALSE;
  }
  if (strpos($source, $module . '_update_last_removed') === FALSE) {
    return NULL;
  }
  $m = array();
  if (preg_match('!function\s*&?' . preg_quote($module, '!') . '_update_last_removed\s*\(\s*\)[^{]*\{\s*return\s+([0-9]+)\s*;!', $source, $m)) {
    return (int) $m[1];
  }
  return FALSE;
}

/**
 * The modules whose installed updates are too old for a target's core.
 *
 * Drupal's own rule, the one Drush prints "too old to update" for and then
 * walks past: a module's installed update number must not be below what the
 * new code's hook_update_last_removed() returns. Only a module the site runs
 * from core, and that the target carries in core, is compared.
 *
 * @param array $modules
 *   The site's switched-on modules: name => array with 'schema_version' (the
 *   installed update number) and 'filename' (the real path of its info file).
 * @param string $source_root
 *   The docroot the site runs on.
 * @param string $target_root
 *   The docroot it would move onto.
 *
 * @return array
 *   'below' (name => array(installed, needed)), 'not_read' (names whose
 *   number could not be read) and 'compared' (how many were).
 */
function _provision_tasks_extra_database_floor($modules, $source_root, $target_root) {
  $out = array('below' => array(), 'not_read' => array(), 'compared' => 0);
  $prefixes = array(rtrim($source_root, '/') . '/core/modules/');
  $real = @realpath($source_root);
  if (is_string($real) && $real !== '') {
    $prefixes[] = rtrim($real, '/') . '/core/modules/';
  }
  $target_root = rtrim($target_root, '/');
  foreach ($modules as $name => $module) {
    if (!is_string($name) || !preg_match('/^[a-z][a-z0-9_]*$/', $name) || !is_array($module)) {
      continue;
    }
    $file = isset($module['filename']) && is_string($module['filename']) ? $module['filename'] : '';
    $from_core = FALSE;
    foreach ($prefixes as $prefix) {
      if (strpos($file, $prefix . $name . '/') === 0) {
        $from_core = TRUE;
      }
    }
    if (!$from_core) {
      continue;
    }
    $needed = _provision_tasks_extra_update_last_removed($target_root . '/core/modules/' . $name . '/' . $name . '.install', $name);
    if ($needed === NULL) {
      continue;
    }
    if ($needed === FALSE) {
      $out['not_read'][] = $name;
      continue;
    }
    $out['compared']++;
    $installed = isset($module['schema_version']) ? (int) $module['schema_version'] : 0;
    if ($installed < $needed) {
      $out['below'][$name] = array($installed, $needed);
    }
  }
  ksort($out['below']);
  sort($out['not_read']);
  return $out;
}

/**
 * The docroot of a platform recorded at its docroot or at its project folder.
 *
 * The order the backend's own correction of a platform path takes.
 */
function _provision_tasks_extra_docroot($root) {
  $root = rtrim((string) $root, '/');
  if ($root === '' || is_file($root . '/index.php')) {
    return $root;
  }
  foreach (array('docroot', 'html', 'web') as $sub) {
    if (is_file($root . '/' . $sub . '/index.php')) {
      return $root . '/' . $sub;
    }
  }
  return $root;
}

/**
 * The Drupal release a codebase carries, or ''.
 *
 * Read as text from core/lib/Drupal.php: no code from the tree is loaded.
 */
function _provision_tasks_extra_core_release($root) {
  $text = _provision_tasks_extra_read_plain(rtrim((string) $root, '/') . '/core/lib/Drupal.php', 1048576);
  if ($text === FALSE) {
    return '';
  }
  $m = array();
  if (preg_match('/^[ \t]*const[ \t]+VERSION[ \t]*=[ \t]*([\'"])([0-9]+\.[0-9]+(?:\.[0-9]+)?(?:-[A-Za-z0-9.]+)?)\1[ \t]*;/m', $text, $m)) {
    return $m[2];
  }
  return '';
}

/**
 * Whether a root is one of the platforms the system itself ships.
 */
function _provision_tasks_extra_upgrade_is_catalogue($root) {
  $root = rtrim((string) $root, '/');
  if (strpos($root . '/', '/distro/') !== FALSE) {
    return TRUE;
  }
  // A platform recorded at a link into such a tree is one too.
  $real = ($root === '') ? FALSE : @realpath($root);
  return is_string($real) && strpos($real . '/', '/distro/') !== FALSE;
}

/**
 * The major and the minor of a release, or FALSE.
 */
function _provision_tasks_extra_release_numbers($release) {
  $m = array();
  if (!preg_match('/^([0-9]+)\.([0-9]+)/', (string) $release, $m)) {
    return FALSE;
  }
  return array((int) $m[1], (int) $m[2]);
}

/**
 * Why a codebase is no newer version for a site to move onto, or ''.
 *
 * A newer core release, or the same release with a lock that raises or adds
 * a package or changes a patch; a site below 10.3.0 does not go to 11;
 * nothing goes to 12 yet, so a move is one major up at most.
 *
 * @param string $source
 *   The release the site runs.
 * @param string $target
 *   The release it would move onto.
 * @param array $diff
 *   _provision_tasks_extra_lock_classify() of the move.
 *
 * @return array
 *   'why' ('' when it is newer) and 'rule': release, major, floor, same or
 *   older.
 */
function _provision_tasks_extra_upgrade_newer($source, $target, $diff) {
  $s = _provision_tasks_extra_release_numbers($source);
  $t = _provision_tasks_extra_release_numbers($target);
  if ($s === FALSE || $t === FALSE) {
    return array('rule' => 'release', 'why' => 'the Drupal release of ' . ($s === FALSE ? 'the current platform' : 'the new platform') . ' cannot be read');
  }
  if ($s[0] < 10 || $t[0] < 10) {
    return array('rule' => 'release', 'why' => 'the Upgrade is for Drupal 10 and later, and this is Drupal ' . ($s[0] < 10 ? $source : $target));
  }
  if ($t[0] >= 12) {
    return array('rule' => 'major', 'why' => 'Drupal 12 needs Drush 14, which has no stable release yet');
  }
  if (version_compare($target, $source, '<')) {
    return array('rule' => 'older', 'why' => 'it carries Drupal ' . $target . ', older than the ' . $source . ' this site runs');
  }
  // One major up at most: with 10 as the lowest source and 12 refused above,
  // nothing else needs saying until 12 is let in.
  if ($t[0] === 11 && $s[0] === 10 && version_compare($source, '10.3.0', '<')) {
    return array('rule' => 'floor', 'why' => 'this site runs Drupal ' . $source . '; Drupal 11 takes a site from 10.3 or later. Move it to a Drupal 10.3 or later platform first (10.6 is the tested route), then upgrade');
  }
  if (version_compare($target, $source, '>')) {
    return array('rule' => '', 'why' => '');
  }
  // The same release: only the locks can show a newer codebase.
  if (empty($diff['locks'])) {
    return array('rule' => 'same', 'why' => 'it carries the same Drupal ' . $target . ', and nothing shows it is newer: ' . $diff['why']);
  }
  if ($diff['scope'] === 'unchanged') {
    return array('rule' => 'same', 'why' => 'it carries the same code as the platform this site runs on');
  }
  if (!empty($diff['patch_change'])) {
    return array('rule' => '', 'why' => '');
  }
  foreach ($diff['changed'] as $c) {
    if ($c['side'] === 'target' || ($c['side'] === 'both' && !$c['lower'])) {
      return array('rule' => '', 'why' => '');
    }
  }
  return array('rule' => 'same', 'why' => 'it carries the same Drupal ' . $target . ' and no package in it is newer');
}

/**
 * Why a codebase's own Drush is known to be unable to start, or ''.
 *
 * One case, read from what Composer recorded: consolidation/robo 5.2.1 to
 * 5.2.3 calls a Symfony Console 7.4 method and dies at start on an older
 * Console. Nothing is run.
 */
function _provision_tasks_extra_drush_cannot_start($root) {
  $app = _provision_tasks_extra_app_root($root);
  $text = ($app === '') ? FALSE : _provision_tasks_extra_read_plain($app . '/vendor/composer/installed.json');
  if ($text === FALSE) {
    return '';
  }
  $info = json_decode($text, TRUE);
  if (!is_array($info)) {
    return '';
  }
  $packages = (isset($info['packages']) && is_array($info['packages'])) ? $info['packages'] : $info;
  $have = array();
  foreach ($packages as $package) {
    if (is_array($package) && isset($package['name'], $package['version']) && is_string($package['name']) && is_string($package['version'])) {
      $have[$package['name']] = ltrim($package['version'], 'vV');
    }
  }
  if (!isset($have['consolidation/robo'], $have['symfony/console'])) {
    return '';
  }
  $robo = $have['consolidation/robo'];
  $console = $have['symfony/console'];
  if (!preg_match('/^[0-9]+\.[0-9]+\.[0-9]+$/', $robo) || !preg_match('/^[0-9]+\.[0-9]+/', $console)) {
    return '';
  }
  if (version_compare($robo, '5.2.1', '>=') && version_compare($robo, '5.2.3', '<=') && version_compare($console, '7.4', '<')) {
    return 'consolidation/robo ' . $robo . ' needs Symfony Console 7.4; this codebase has ' . $console;
  }
  return '';
}

/**
 * One list's refusal as a clause, with whose list it is and its file.
 */
function _provision_tasks_extra_upgrade_list_refusal($whose, $modes, $allow) {
  $file = (isset($modes['file']) && $modes['file'] !== '') ? basename($modes['file']) : 'its control file';
  if ($allow['axis'] === 'unreadable') {
    $line = empty($modes['line']) ? '' : ', line ' . $modes['line'];
    return 'upgrades are stopped: drupal_upgrade_modes in ' . $whose . ' ' . $file . $line . ' cannot be read (' . (isset($modes['why']) ? $modes['why'] : 'unknown reason') . ')';
  }
  if ($allow['axis'] === 'off') {
    return 'upgrades are switched off by drupal_upgrade_modes in ' . $whose . ' ' . $file;
  }
  return 'the list in ' . $whose . ' ' . $file . ' ' . $allow['why'];
}

/**
 * A classified move with the step between the two core releases counted in.
 *
 * The kind of a move is read from the two locks. Where the releases differ
 * and the locks do not show it (core is not in the lock, or the tree does
 * not match its lock), a list must still see a change of core of that size:
 * the level is the larger of the two, and the scope holds core. No advisory
 * read from the locks covers such a change, so it is no security move.
 */
function _provision_tasks_extra_upgrade_release_step($source_release, $target_release, $diff) {
  $s = _provision_tasks_extra_release_numbers($source_release);
  $t = _provision_tasks_extra_release_numbers($target_release);
  if ($s === FALSE || $t === FALSE || (string) $source_release === (string) $target_release) {
    return $diff;
  }
  $step = ($s[0] !== $t[0]) ? 'major' : (($s[1] !== $t[1]) ? 'minor' : 'patch');
  if (empty($diff['locks'])) {
    // No locks to read: scope and level stay unknown, as a list is told.
    return $diff;
  }
  $diff['level'] = ($diff['level'] === '') ? $step : _provision_tasks_extra_lock_larger($diff['level'], $step);
  if (empty($diff['changed']['drupal/core']) && $diff['security'] !== FALSE) {
    $diff['security'] = FALSE;
    $diff['security_why'] = 'the core release changes and the two locks do not show it, so no fixed advisory covers that change';
  }
  if ($diff['scope'] === 'unchanged') {
    $diff['scope'] = 'core';
  }
  elseif ($diff['scope'] === 'contrib' && empty($diff['changed']['drupal/core'])) {
    $diff['scope'] = 'full';
  }
  return $diff;
}

/**
 * Whether a site may move between two codebases, by what their files say.
 *
 * The two releases, the two locks and the two lists: the rules the panel
 * shows a target by and the backend refuses a run by, in one place and one
 * order. Each list is asked on its own; the word sets are never merged.
 *
 * @param string $source_release
 *   The release the site runs.
 * @param string $target_release
 *   The release it would move onto.
 * @param array $diff
 *   _provision_tasks_extra_lock_classify() of the move.
 * @param array $platform_modes
 *   _provision_tasks_extra_upgrade_modes() of the current platform's file.
 * @param array $site_modes
 *   The same for the site's file.
 * @param bool $best_effort
 *   TRUE where an early look must not refuse on what it cannot tell: an
 *   unknown on an axis a list restricts is then left to the run itself.
 *
 * @return array
 *   'ok'. When FALSE: 'rule' (release, major, floor, same, older, downgrade,
 *   off, unreadable, scope, level, security) and 'why', a clause with no
 *   full stop. 'pending' TRUE when best effort passed over an unknown.
 */
function _provision_tasks_extra_upgrade_judge($source_release, $target_release, $diff, $platform_modes, $site_modes, $best_effort = FALSE) {
  $newer = _provision_tasks_extra_upgrade_newer($source_release, $target_release, $diff);
  if ($newer['why'] !== '') {
    return array('ok' => FALSE, 'rule' => $newer['rule'], 'why' => $newer['why'], 'pending' => FALSE);
  }
  if ($diff['downgrades']) {
    $parts = array();
    foreach ($diff['downgrades'] as $name => $text) {
      $parts[] = $name . ' (' . $text . ')';
    }
    return array(
      'ok' => FALSE,
      'rule' => 'downgrade',
      'why' => 'it carries an older version of ' . _provision_tasks_extra_upgrade_names($parts) . ' than this site runs now',
      'pending' => FALSE,
    );
  }
  $diff = _provision_tasks_extra_upgrade_release_step($source_release, $target_release, $diff);
  $pending = FALSE;
  foreach (array("the platform's" => $platform_modes, "this site's" => $site_modes) as $whose => $modes) {
    $allow = _provision_tasks_extra_upgrade_modes_allow($modes, $diff);
    if ($allow['allowed']) {
      continue;
    }
    $unknown = ($allow['axis'] === 'scope' && $diff['scope'] === 'unknown')
      || ($allow['axis'] === 'level' && $diff['level'] === 'unknown')
      || ($allow['axis'] === 'security' && $diff['security'] === NULL);
    if ($best_effort && $unknown) {
      $pending = TRUE;
      continue;
    }
    return array(
      'ok' => FALSE,
      'rule' => $allow['axis'],
      'why' => _provision_tasks_extra_upgrade_list_refusal($whose, $modes, $allow),
      'pending' => FALSE,
    );
  }
  return array('ok' => TRUE, 'rule' => '', 'why' => '', 'pending' => $pending);
}

/**
 * Why a site's database is too old for a target, as one clause, or ''.
 *
 * @param array $floor
 *   _provision_tasks_extra_database_floor() of the site and the target.
 * @param string $target_release
 *   The release the site would move onto.
 */
function _provision_tasks_extra_database_floor_text($floor, $target_release) {
  if (empty($floor['below'])) {
    return '';
  }
  $parts = array();
  foreach ($floor['below'] as $name => $pair) {
    $parts[] = $name . ' is at update ' . (int) $pair[0] . ', the new code needs ' . (int) $pair[1];
  }
  $numbers = _provision_tasks_extra_release_numbers($target_release);
  return "this site's database is too old for Drupal " . ($numbers === FALSE ? $target_release : $numbers[0]) . ': '
    . _provision_tasks_extra_upgrade_names($parts)
    . '. Migrate the site onto another Drupal 10.3 or later platform first (10.6 is the tested route): the Migrate runs its database updates. Then upgrade. If the site changed since it was last verified (its updates were run, or a backup was restored), verify it and try again';
}

/**
 * A loaded lock with only what the classifier reads of each package.
 *
 * For a reader that keeps the reading between requests: the same verdicts
 * from a fraction of the bytes.
 */
function _provision_tasks_extra_lock_slim($lock) {
  if (empty($lock['ok']) || !isset($lock['packages']) || !is_array($lock['packages'])) {
    return $lock;
  }
  foreach ($lock['packages'] as $name => $package) {
    $keep = array();
    foreach (array('name', 'version', 'type', 'require', 'replace', 'provide') as $key) {
      if (isset($package[$key])) {
        $keep[$key] = $package[$key];
      }
    }
    foreach (array('source', 'dist') as $key) {
      if (isset($package[$key]) && is_array($package[$key]) && isset($package[$key]['reference'])) {
        $keep[$key] = array('reference' => $package[$key]['reference']);
      }
    }
    $lock['packages'][$name] = $keep;
  }
  return $lock;
}

/**
 * Whether an Upgrade's row counts as one that moved its site.
 *
 * A row that ended well moved it. One that ended in error moved it only
 * while its way-back copy is kept and its log names no ending in which the
 * site stayed where it was: the move went through and its last step did
 * not, or nothing says where the site is. A run that was not upgraded can
 * keep its copy too (its removal failed), and moved nothing.
 *
 * @param bool $failed
 *   The row ended in error.
 * @param bool $copy_kept
 *   The panel still holds the copy the run made.
 * @param array $codes
 *   The error codes the row's log holds.
 */
function _provision_tasks_extra_upgrade_run_moved($failed, $copy_kept, $codes) {
  if (!$failed) {
    return TRUE;
  }
  if (!$copy_kept) {
    return FALSE;
  }
  $stayed = array('PROVISION_UPGRADE_NOT_UPGRADED', 'PROVISION_UPGRADE_NOT_STARTED', 'HOSTING_UPGRADE_NOT_STARTED');
  return !array_intersect($stayed, (array) $codes);
}

/**
 * What a Go back row says a later run on a site node has to do.
 *
 * Read from the mark the run's own end left on its row (return_end: 'back',
 * 'kept', 'open' or 'stuck', and 'pending' from its start until then), not
 * from the row's status: a runner that is killed reaches no end, and the
 * panel's "Update status" sets a status from whatever its log holds by then.
 * A row from before the mark is read by its status, as it was.
 *
 * @param string $title
 *   The node's name now.
 * @param array $record
 *   The row's return_* arguments.
 * @param bool $under_way
 *   The row is queued or running.
 * @param bool $failed
 *   The row ended in error.
 * @param int $upgrade
 *   The node id of the newest Upgrade that moved the site and left it where
 *   it is, 0 for none.
 *
 * @return string
 *   'resume' (a later run finishes it from the row's record), 'busy', 'done',
 *   'stuck' (stopped where no run can go on), or '' when the row says
 *   nothing about the node as it is now.
 */
function _provision_tasks_extra_return_offer($title, $record, $under_way, $failed, $upgrade) {
  $aside = isset($record['return_aside']) ? (string) $record['return_aside'] : '';
  $domain = isset($record['return_domain']) ? (string) $record['return_domain'] : '';
  if ($aside === '' || $domain === '') {
    return '';
  }
  $end = isset($record['return_end']) ? (string) $record['return_end'] : NULL;
  if ($title === $aside) {
    if ($under_way) {
      return 'busy';
    }
    if ($end === NULL) {
      return $failed ? 'resume' : 'done';
    }
    if ($end === 'stuck') {
      return 'stuck';
    }
    return ($end === 'back' || $end === 'kept') ? 'done' : 'resume';
  }
  // Under its own name again, after a run that never reached its end (killed
  // once it had renamed the upgraded site back, say): the renames may have
  // dropped settings only that run's record still holds.
  if ($title === $domain && $end === 'pending' && !$under_way && (int) $upgrade > 0
    && isset($record['return_upgrade']) && (int) $record['return_upgrade'] === (int) $upgrade) {
    return 'resume';
  }
  return '';
}

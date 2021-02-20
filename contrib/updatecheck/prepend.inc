<?php
$versions = array();
function readVersions($old=false) {
  global $versions;
  $versions = array();
  $v = @file_get_contents(($old?'old':'').'versions.txt');
  if ($v) {
    $v = explode("\n", $v);
    $func = function($val, $k) {
      global $versions;
      $val = explode('=', $val);
      if (count($val)>1) {
        $versions[$val[0]] = explode(',', $val[1]);
      }
    };
    array_walk($v, $func);
  }
}
function checkUpdate($ebusdVersion, $ebusdRelease, $architecture, $loadedFiles) {
  if (!$ebusdVersion) {
    return 'invalid request';
  }
  readVersions($architecture && substr($architecture, 0, 3)!=='arm' && (((float)$ebusdVersion)<3.3 || ($ebusdVersion==='3.3' && ($ebusdRelease==='v3.3' || (strtok($ebusdRelease, '-')==='v3.3') && strtok('-')<18))));
  global $versions;
  $ret = 'unknown';
  if ($ebusdVersion==$versions['ebusd'][0] && $ebusdRelease==$versions['ebusd'][1]) {
    $ret = 'OK';
  } else if ($ebusdVersion==$versions['ebusd'][0]) {
    $ret = 'revision '.$versions['ebusd'][1].' available';
  } else {
    $ret = 'version '.$versions['ebusd'][0].' available';
  }
  $newerAvailable = 0;
  $configs = '';
  foreach ($loadedFiles as $k => $val) {
    $v = $versions[$k];
    if ($v) {
      $newer = $v[2]>$val['t'];
      if ($v[0]!=$val['h'] || $v[1]!=$val['s']) {
        if ($newer) {
          $newerAvailable++;
        }
        $configs .= ', '.$k.': '.($newer?'newer':'different').' version available';
      }
    }
  }
  if (strlen($configs)>1000) {
    return $ret.($newerAvailable?', '.$newerAvailable.' newer':', different').' configuration files available';
  }
  return $ret.$configs;
}
?>

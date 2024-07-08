<?php
$versions = array();
function readVersions($prefix='') {
  global $versions;
  $versions = array();
  $v = @file_get_contents($prefix.'versions.txt');
  if (!$v) {
    return false;
  }
  $v = explode("\n", $v);
  $func = function($val, $k) {
    global $versions;
    $val = explode('=', $val);
    if (count($val)>1) {
      $key = $val[0];
      $val = explode(',', $val[1]);
      if ($key==='device' && count($val)===2 && $val[0]===$val[1]) {
        $key = 'devicesame';
      }
      $versions[$key] = $val;
    }
  };
  array_walk($v, $func);
  return true;
}
function checkUpdate($ebusdVersion, $ebusdRelease, $architecture, $deviceVersion, $loadedFiles, $language, $cdnPath) {
  if (!$ebusdVersion) {
    return 'invalid request';
  }
  $old = $architecture && substr($architecture, 0, 3)!=='arm' && (((float)$ebusdVersion)<3.3 || ($ebusdVersion==='3.3' && ($ebusdRelease==='v3.3' || (strtok($ebusdRelease, '-')==='v3.3') && strtok('-')<18)));
  readVersions($cdnPath ? 'cdn' : ($old ? 'old' : '')) || ($cdnPath && readVersions());
  global $versions;
  $ret = 'unknown';
  if ($ebusdVersion==$versions['ebusd'][0] && $ebusdRelease==$versions['ebusd'][1]) {
    $ret = 'OK';
  } else if ($ebusdVersion==$versions['ebusd'][0]) {
    $ret = 'revision '.$versions['ebusd'][1].' available';
  } else {
    $ret = 'version '.$versions['ebusd'][0].' available';
  }
  if ($deviceVersion && @$versions['device']) {
    $feat = strtok($deviceVersion, '.');
    $ver = strtok('.');
    if ($ver) {
      $key = ($ver===strtok('.')) ? 'devicesame' : 'device';
      if ($ver!==$versions[$key][0]) {
        $ret .= ', device firmware '.$versions[$key][0].' available';
      }
    }
  }
  $newerAvailable = 0;
  $configs = '';
  $offset = $language==='en' ? 3 : 0;
  foreach ($loadedFiles as $k => $val) {
    $v = $versions[$k];
    if ($v) {
      $newer = $v[2+$offset]>$val['t'];
      if ($v[0+$offset]!=$val['h'] || $v[1+$offset]!=$val['s']) {
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

<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
if (substr($agent, 0, 5)==='ebusd') {
  $r = @file_get_contents('php://input');
  $v = @file_get_contents('versions.txt');
  $versions = array();
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
  header('Content-Type: text/plain');
  $ret = 'unknown';
  $r = @json_decode($r, true);
  if (!$r || !isset($r['v']) || !isset($r['r'])) {
    $ret = 'invalid request';
  } else {
    if (isset($versions['ebusd'])) {
      if ($r['v']==$versions['ebusd'][0] && $r['r']==$versions['ebusd'][1]) {
        $ret = 'OK';
      } else if ($r['v']==$versions['ebusd'][0]) {
        $ret = 'revision '.$versions['ebusd'][1].' available';
      } else {
        $ret = 'version '.$versions['ebusd'][0].' available';
      }
    }
    $configs = '';
    $neweravail = 0;
    $func = function($val, $k) {
      global $versions, $configs, $neweravail;
      $v = $versions[$k];
      if ($v) {
        $newer = $v[2]>$val['t'];
        if ($newer || $v[0]!=$val['h'] || $v[1]!=$val['s']) {
          if ($newer) $neweravail++;
          $configs .= ', '.$k.': '.($newer?'newer':'different').' version available';
        }
      }
    };
    array_walk($r['l'], $func);
    if (strlen($configs)>100) {
      $ret .= ($neweravail?$neweravail.', newer ':', different').' configuration files available';
    } else {
       $ret .= $configs;
     }
  }
  echo $ret;
  exit;
}
header('HTTP/1.1 404 Not Found');
?>

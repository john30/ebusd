<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
if (substr($agent, 0, 5)==='ebusd') {
  $t = time();
  $a = $_SERVER["REMOTE_ADDR"];
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
  if (!$r || !isset($r['v'])) {
    $ret = 'invalid request';
  } else {
    if (isset($versions['ebusd'])) {
      if ($r['v']==$versions['ebusd'][0]) {
        $ret = 'OK';
      } else {
        $ret = 'ebusd '.$versions['ebusd'][0].' available';
      }
    }
    // TODO add check for config files
  }
  echo $ret;
  exit;
}
header('HTTP/1.1 404 Not Found');
?>

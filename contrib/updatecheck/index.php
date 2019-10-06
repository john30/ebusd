<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
if (substr($agent, 0, 5)==='ebusd') {
  require_once('prepend.inc');
  $r = @file_get_contents('php://input');
  header('Content-Type: text/plain');
  $r = @json_decode($r, true);
  echo checkUpdate(@$r['v'], @$r['r'], @$r['a'], @$r['l']);
  exit;
}
header('HTTP/1.1 404 Not Found');
?>

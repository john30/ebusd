<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
require_once('prepend.inc');
if (substr($agent, 0, 5)==='ebusd') {
  $r = @file_get_contents('php://input');
  header('Content-Type: text/plain');
  $r = @json_decode($r, true);
  echo checkUpdate(@$r['v'], @$r['r'], @$r['a'], @$r['l']);
  exit;
}
readVersions();
?>
<html>
<head>
  <title>ebusd update check service</title>
</head>
<body>
  <p>latest ebusd version: <?=$versions['ebusd'][0]?></p>
  <p>last update: <?=date('c', @filemtime('versions.txt'))?></p>
</body>

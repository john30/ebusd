<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
if (substr($agent, 0, 5)!=='ebusd') {
  header('Location: https://upd.ebusd.eu/', true, 301);
  exit;
}
$p=@$_REQUEST['p'];
$e=@$_REQUEST['t'];
$a=@$_REQUEST['a'];
if ($p && $p[0]==='/') {
  $p=substr($p, 1);
}
if ($p && $p[strlen($p)-1]==='/') {
  $p=substr($p, 0, strlen($p)-1);
}
if ($p && $p[0]!=='/' && strpos($p, '..')===FALSE && is_file($p)) {
  header('Content-Type: text/comma-separated-values');
  header('Cache-Control: Private');
  $t=@filemtime($p);
  if ($t) {
    header('Last-Modified: '.gmdate('D, d M Y H:i:s', $t).' GMT');
  }
  ob_start();
  echo @file_get_contents($p);
  header('Content-Length: '.ob_get_length());
  ob_end_flush();
  exit;
}
if (!$e) {
  header('HTTP/1.1 404 Not Found');
  exit;
}
$e=".$e";
if (!$p) {
  $p='.';
} else if (strpos($p, '/')!==FALSE || strpos($p, '\\')!==FALSE || strpos($p, '..')!==FALSE) {
  header('HTTP/1.1 400 Bad Request');
  exit;
}
$prefix=$a ? $a==='-' ? $a : substr(dechex(0x100|hexdec($a)), 1).'.' : NULL;
$dir=@opendir($p);
if ($dir===FALSE) {
  header('HTTP/1.1 404 Not Found');
  exit;
}
header('Content-Type: text/comma-separated-values');
header('Cache-Control: Private');
ob_start();
while(($f=@readdir())!==FALSE) {
  if (!$f || !is_file($p.'/'.$f) || substr($f, -strlen($e))!==$e) {
    continue;
  }
  if ($f!=='_templates'.$e && $prefix && ($prefix==='-' ? strpos($f, '.') === 2 : substr($f, 0, strlen($prefix))!==$prefix)) {
    continue;
  }
  echo "$f\n";
}
@closedir($dir);
header('Content-Length: '.ob_get_length());
ob_end_flush();
?>

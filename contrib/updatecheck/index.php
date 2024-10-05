<?php
$agent = $_SERVER['HTTP_USER_AGENT'];
require_once('prepend.inc');
if (substr($agent, 0, 5)==='ebusd') {
  $r = @file_get_contents('php://input');
  header('Content-Type: text/plain');
  $r = @json_decode($r, true);
  echo checkUpdate(@$r['v'], @$r['r'], @$r['a'], @$r['dv'], @$r['di'], @$r['l'], @$r['lc'], @$r['cp']);
  exit;
}
readVersions();
$ref = @file_get_contents('ebusd-configuration/.git/refs/heads/master');
?>
<html>
<head>
  <title>ebusd update check webservice</title>
</head>
<body>
  <p>last update: <?=date('c', @filemtime('versions.txt'))?></p>
<?php
  if ($ref) {
    echo "  <p>git revision: <a href=\"https://github.com/john30/ebusd-configuration/tree/$ref\">".substr($ref, 0, 7).'</a></p>';
  }
?>
  <p>latest ebusd version: <?=$versions['ebusd'][0]?></p>
  <p>latest device firmware: v5*=<?=$versions['devicesame'][0]?>, v3*=<?=$versions['device'][0]?></p>
  <p>config files:<table><thead><th>File</th><th>German</th><th>English</th></thead><tbody>
<?php
  $func = function($v, $k) {
    if ($k==='ebusd' || $k==='device' || $k==='devicesame') {
      return;
    }
    echo "<tr><td>$k</td>";
    global $ref;
    $str = date('d.m.Y H:i:s', $v[2]);
    if ($ref) {
      echo "<td><a href=\"https://github.com/john30/ebusd-configuration/blob/$ref/ebusd-2.1.x/de/$k\">$str</a></td>\n";
    } else {
      echo "<td>$str</td>\n";
    }
    $str = date('d.m.Y H:i:s', $v[5]);
    if ($ref) {
      echo "<td><a href=\"https://github.com/john30/ebusd-configuration/blob/$ref/ebusd-2.1.x/en/$k\">$str</a></td>\n";
    } else {
      echo "<td>$str</td>\n";
    }
  };
  array_walk($versions, $func);
?>
  </tbody></table></p>
</body>
#!/bin/bash
./src/ebusd/ebusd --help >/dev/null
./src/ebusd/ebusd -r -f -x >/dev/null 2>/dev/null
./src/ebusd/ebusd -c contrib/etc/ebusd --checkconfig --dumpconfig -s -f "ff08070400/0ab5303132333431313131" >/dev/null
php -r '
error_reporting (E_ALL);
set_time_limit (0);
ob_implicit_flush ();
if (($srv=socket_create(AF_INET, SOCK_STREAM, SOL_TCP))===false) die("server: create socket");
if (socket_bind($srv, "127.0.0.1", 8876)===false) {
  @socket_close($srv);
  die("server: bind socket");
}
if (socket_listen($srv, 5)===false) {
  @socket_close($srv);
  die("server: listen socket");
}
echo "server: waiting\n";
if (($cli=socket_accept($srv))===false) {
  @socket_close($srv);
  die("server: socket accept");
}
@socket_set_block($cli);
$output="";
$input=$hexinput="";
echo "server: running\n";
$endtime=time()+20;
$firstsend=time()+5;
while (time()<$endtime) {
  if (($r=@socket_read($cli, 1))===false) break;
  if (strlen($r)==0) break;
  socket_write($cli, $r, 1);
  if (ord($r[0])==0xaa) {
    if (strlen($output)>0) {
      echo "server: $output\n";
      $output="";
    }
    if ($firstsend && time()>$firstsend) {
      socket_write($cli, "\xff\x36\x07\x04\x00\x06\xaa",8);
      socket_write($cli, "\xff\xfe\xb5\x16\x03\x01\x50\x30\x86\xaa",10);
      $firstsend=0;
    }
  } else {
    $output.=substr(dechex(0x100|ord($r[0])),1);
    if ("31040704006f"==$output) { // answer on ident query
      socket_write($cli, "\x00\x0a\x99\x42\x42\x42\x42\x42\x30\x31\x30\x31\x71",13);
      $output.=">answered<";
    } else if ("315307040091"==$output) { // answer on other ident query
      socket_write($cli, "\x00\x0a\x99\x42\x42\x42\x42\x42\x30\x31\x30\x31\x71",13);
      $output.=">answered<";
    } else if ("311cb509030d0000e1"==$output || "3152b509030d0600c9"==$output || "3153b509030d060034"==$output) {
      socket_write($cli, "\x00\x02\x40\x50\xb4",5);
      $output.=">answered<";
    } else if ("3153b505080290909090909003f6"==$output || "3153b509050e3500190030"==$output) {
      socket_write($cli, "\x00\x00\x00",3);
      $output.=">answered<";
    } else {
      $endtime=time()+10; // keep further alive
    }
  }
}
@socket_close($cli);
@socket_close($srv);
echo "server: done\n";
' &
srvpid=$!
if [ -z "$srvpid" ]; then
  echo "unable to start echo server"
  exit 1
fi
sleep 1
ps -p $srvpid >/dev/null
status="$?"
if [ ! "$status" = 0 ]; then
  echo "unable to start echo server"
  exit 1
fi
echo "server: $srvpid"
cat >contrib/etc/ebusd/test.csv <<EOF
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,D2B
r2,rcc.4,RoomTemp,Raumisttemp,,1c,b509,0d0000,temp,s,D2C,,C
r,mc.4,OutsideTemp,Sensor,,52,b509,0d0600,temp,s,D2C,,°C
r,mc.5,Timer.Monday,,,53,b504,02,from,s,TTM,,,Slots 1-3,to,s,TTM,,,bis,from,s,TTM,,,Slot von/bis,to,s,TTM,,,bis,from,s,TTM,,,Slot von/bis,to,s,TTM,,,bis,daysel,s,UCH,0=selected;1=Mo-Fr;2=Sa-So;3=Mo-So,,Tage
w,mc.5,Timer.Monday,,,53,b505,02,from,m,TTM,,,Slots 1-3,to,m,TTM,,,bis,from,m,TTM,,,Slot von/bis,to,m,TTM,,,bis,from,m,TTM,,,Slot von/bis,to,m,TTM,,,bis,daysel,m,UCH,0=selected;1=Mo-Fr;2=Sa-So;3=Mo-So,,Tage
r,mc.5,HeatingCurve,,,53,b509,0d3500,curve,s,UIN,100
w,mc.5,HeatingCurve,,,53,b509,0e3500,curve,m,UIN,100
EOF
./src/ebusd/ebusd -d tcp:127.0.0.1:8876 --initsend --latency 10000 -n -c "$PWD/contrib/etc/ebusd" --pollinterval=10 -s -a 31 --acquireretries 3 --answer --generatesyn --numbermasters 1 --receivetimeout 10000 --sendretries 1 --enablehex --htmlpath "$PWD/contrib/html" --httpport 8878 --localhost --pidfile "$PWD/ebusd.pid" -p 8877 -l "$PWD/ebusd.log" --logareas all --loglevel debug --lograwdata --dumpfile "$PWD/ebusd.dump" --dumpsize 100 -D
pid=`head -n 1 "$PWD/ebusd.pid"`
if [ -z "$pid" ]; then
  echo "unable to start ebusd"
  kill $srvpid
  exit 1
fi
echo "ebusd: $pid"
readarray lines <<EOF
raw
log
log all notice
grab
info
state
scan full
read -c "hallo du" ich
h
h r
h w
h hex
h f
h l
h s
h i
h g
h scan
h log
h raw
h dump
h reload
h stop
h q
h h
l &
r -f temp
r -f outsidetemp
r -m 10 outsidetemp
r -v outsidetemp
r -c broadcast outsidetemp
r -c mc.4 -d 53 -p 2 outsidetemp
r -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.1
r -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.0
r -n -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.0
r -h
r -h 53070400
f -f outsidetemp
f -v -r temp
f -v -w temp
f -v -p temp
f -d -i b5
f -F name temp
f -e RoomTemp
f -e -c mc.5
r -c mc.5 Timer.Monday
w -c mc.5 Timer.Monday "-:-;-:-;-:-;-:-;-:-;-:-;Mo-So"
w -c mc.5 -d 53 HeatingCurve 0.25
w -h fe070400
hex fe070400
hex 53070400
dump
grab result
reload
EOF
while [ ! "$status" = 0 ]; do
  sleep 5
  ./src/tools/ebusctl -p 8877 state | egrep -q "signal acquired"
  status=$?
done
if [ "$status" = 0 ]; then
  echo "got signal"
  sleep 5
  ./src/tools/ebusctl -p 8877 ""
  for line in "${lines[@]}"; do
    if [ -n "$line" ]; then
      echo "send: $line"
      ./src/tools/ebusctl -p 8877 $line
    fi
  done
  while [ "$status" = 0 ]; do
    ./src/tools/ebusctl -p 8877 scan result | egrep -q "still running"
    status=$?
  done
  echo "scan result:"
  ./src/tools/ebusctl -p 8877 scan result
  echo "commands done"
fi
sleep 5
kill $pid
kill $srvpid
echo "done."

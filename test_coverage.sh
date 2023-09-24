#!/bin/bash -x
prefix=.
ebusd=$prefix/src/ebusd/ebusd
ebusctl=$prefix/src/tools/ebusctl
ebuspicloader=$prefix/src/tools/ebuspicloader

$ebusd --help >/dev/null
$ebusd -r -f -x >/dev/null 2>/dev/null
$ebusd -f -d "" >/dev/null 2>/dev/null
$ebusd -f -d "tcp:192.168.999.999:1" --log bad >/dev/null 2>/dev/null
$ebusd -f -d "enh:192.168.999.999:1" --log bad >/dev/null 2>/dev/null
$ebusd -f -d "/dev/ttyUSBx9" --log bad >/dev/null 2>/dev/null
$ebusd -f --nodevicecheck --log bad >/dev/null 2>/dev/null
$ebusd -f --readonly >/dev/null 2>/dev/null
$ebusd -f --scanconfig=full -r >/dev/null 2>/dev/null
$ebusd -f --latency 999999 >/dev/null 2>/dev/null
$ebusd -f -c "" >/dev/null 2>/dev/null
$ebusd -f -r --scanconfig=fe >/dev/null 2>/dev/null
$ebusd -f -r --configlang=en >/dev/null 2>/dev/null
$ebusd -f --pollinterval 999999 >/dev/null 2>/dev/null
$ebusd -f --inject --checkconfig=stop 01fe030400/ >/dev/null 2>/dev/null
$ebusd -f --address 999 >/dev/null 2>/dev/null
$ebusd -f --acquiretimeout 999999 >/dev/null 2>/dev/null
$ebusd -f --acquireretries 999999 >/dev/null 2>/dev/null
$ebusd -f --sendretries 999999 >/dev/null 2>/dev/null
$ebusd -f --receivetimeout 9999999 >/dev/null 2>/dev/null
$ebusd -f --numbermasters 999999 >/dev/null 2>/dev/null
$ebusd -f -r --answer >/dev/null 2>/dev/null
$ebusd -f -r --generatesyn >/dev/null 2>/dev/null
$ebusd -f -r --initsend >/dev/null 2>/dev/null
$ebusd -f -r --scanconfig=0 >/dev/null 2>/dev/null
$ebusd -f --pidfile "" >/dev/null 2>/dev/null
$ebusd -f -p 999999 >/dev/null 2>/dev/null
$ebusd -f --localhost --log bad >/dev/null 2>/dev/null
$ebusd -f --httpport 999999 >/dev/null 2>/dev/null
$ebusd -f --htmlpath "" >/dev/null 2>/dev/null
$ebusd -f --updatecheck=off --log bad >/dev/null 2>/dev/null
$ebusd -f -l "" --log bad >/dev/null 2>/dev/null
$ebusd -f --log "all debug" --log bad >/dev/null 2>/dev/null
$ebusd -f --logareas some >/dev/null 2>/dev/null
$ebusd -f --loglevel unknown >/dev/null 2>/dev/null
$ebusd -f --lograwdata --log bad >/dev/null 2>/dev/null
$ebusd -f --lograwdata=bytes --log bad >/dev/null 2>/dev/null
$ebusd -f --lograwdatafile=/notexist/xyz --log bad >/dev/null 2>/dev/null
$ebusd -f --lograwdatasize=9999999 >/dev/null 2>/dev/null
$ebusd -f --dump --log bad >/dev/null 2>/dev/null
$ebusd -f --dumpfile "" >/dev/null 2>/dev/null
$ebusd -f --dumpsize 9999999 >/dev/null 2>/dev/null
$ebusd -f --dumpflush --log bad >/dev/null 2>/dev/null
$ebusd -f --accesslevel=inst --log bad >/dev/null 2>/dev/null
$ebusd -f --aclfile=/ >/dev/null 2>/dev/null
$ebusd -f --enablehex --log bad >/dev/null 2>/dev/null
$ebusd -f --enabledefine --log bad >/dev/null 2>/dev/null
$ebusd -f --mqttport= >/dev/null 2>/dev/null
$ebusd -f --mqttport=9999999 >/dev/null 2>/dev/null
$ebusd -f --mqttuser=username --mqttpass=password --mqttclientid=1234 --mqttport=1883 --mqtttopic=ebusd/%circuit/%name/%field --mqttretain --mqttjson --mqttverbose --mqttlog --mqttignoreinvalid --mqttchanges --mqtthost "" >/dev/null 2>/dev/null
$ebusd -f --mqttca=/cafile/ --mqttcert=/cert --mqttkey=12345678 --mqttkeypass=secret --mqttinsecure --log bad >/dev/null 2>/dev/null
$ebusd -c contrib/etc/ebusd -s -f --inject=stop "ff08070400/0ab5303132333431313131" >/dev/null 2>/dev/null
$ebusd -c contrib/etc/ebusd -s -f --inject=stop "ff08070400" >/dev/null 2>/dev/null
$ebusd -c contrib/etc/ebusd -s -f --inject=stop "ff080704/" >/dev/null 2>/dev/null
cat >contrib/etc/ebusd/bad.csv <<EOF
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,UCH,2000000000
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,D2B,2000000000=
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,D2B,0=0;1
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,D2B,a
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,z
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m
u,broadcast,outsidetemp,,,fe,b516,01,temp2,z
u,broadcast,outsidetemp,,,fe,b516,01,temp2,
u,broadcast,outsidetemp,,,fe,b516,01,temp2
u,broadcast,outsidetemp,,,fe,b516,z
u,broadcast,outsidetemp,,,fe,z
u,broadcast,outsidetemp,,,fe,
u,broadcast,outsidetemp,,,fe
u,broadcast,outsidetemp,,,z
u,broadcast,outsidetemp,,,
u,broadcast,outsidetemp,,
u,broadcast,outsidetemp,
u,broadcast,outsidetemp
u,broadcast,
u,broadcast
u,
u
EOF
$ebusd -c contrib/etc/ebusd --checkconfig >/dev/null
rm -f contrib/etc/ebusd/bad.csv
$ebusctl -s testserver -p 100000 >/dev/null 2>/dev/null
$ebusctl -s "" >/dev/null 2>/dev/null
$ebusctl -p "" >/dev/null 2>/dev/null
$ebusctl -x >/dev/null 2>/dev/null
$ebusctl --help >/dev/null
$ebusctl 'help x' >/dev/null 2>/dev/null
$ebuspicloader --help >/dev/null
$ebuspicloader -i "" >/dev/null 2>/dev/null
$ebuspicloader -i x >/dev/null 2>/dev/null
$ebuspicloader -i 192.168.0.10.1 >/dev/null 2>/dev/null
$ebuspicloader -i 192.168.0.10 -d >/dev/null 2>/dev/null
$ebuspicloader -m "" >/dev/null 2>/dev/null
$ebuspicloader -m 65 >/dev/null 2>/dev/null
$ebuspicloader -d -i 192.168.0.10 >/dev/null 2>/dev/null
$ebuspicloader -d -m "" >/dev/null 2>/dev/null
$ebuspicloader -d >/dev/null 2>/dev/null
$ebuspicloader -a 9000 >/dev/null 2>/dev/null
$ebuspicloader -a 150 -f $ebuspicloader -i 192.168.0.10 -m 24 -M -r -s -v /dev/zero >/dev/null 2>/dev/null
$ebuspicloader -f x >/dev/null 2>/dev/null
echo -e ':100800008431542CAE3401347E1484314E01961E52\n:00000001FF' > firmware.hex
$ebuspicloader -f firmware.hex >/dev/null
#server:
php -r 'echo "php is available";'|egrep 'php is available'
if [ ! "$?" = 0 ]; then
  echo `date` "php is not available"
  exit 1
fi
php -r '
error_reporting(E_ALL);
set_time_limit(0);
ob_implicit_flush();
if (($srv=socket_create_listen(8876))===false) {
  die("server: create_listen socket");
}
socket_set_block($srv);
echo "server: waiting\n";
if (($cli=socket_accept($srv))===false) {
  @socket_close($srv);
  die("server: socket accept");
}
echo "server: connection accepted\n";
socket_set_block($cli);
$output="";
$input=$hexinput="";
echo "server: running\n";
$endtime=time()+15;
$firstsend=time()+5;
$secondsend=$firstsend+5;
$expectnext="";
$error=0;
while (time()<$endtime) {
  if (($r=@socket_read($cli, 1))===false) break;
  if (strlen($r)==0) break;
  socket_write($cli, $r, 1);
  if (ord($r[0])==0xaa) {
    if (strlen($output)>0) {
      echo "server: <$output\n";
      if ($expectnext) {
        if ($expectnext!=$output) {
          if (substr($output,0,2)!="ff") {
            echo "server: arbitration lost for ".substr($output,0,2).", retry\n";
            $firstsend=time()+5;
            $secondsend=$firstsend+5;
          } else {
            echo "server: error unexpected answer (should be $expectnext)\n";
            $error=1;
          }
        }
        $expectnext="";
      }
      $output="";
    }
    if ($firstsend && time()>$firstsend) {
      echo "server: sending ID query to 36\n";
      socket_write($cli, "\xff\x36\x07\x04\x00\x06\xaa",8);
      $expectnext="000afd6562757364030001006c";
      $firstsend=0;
    } else if ($secondsend && time()>$secondsend) {
      echo "server: sending brodcast message\n";
      socket_write($cli, "\xff\xfe\xb5\x16\x03\x01\x50\x30\x86\xaa",10);
      $secondsend=0;
    }
  } else {
    $output.=substr(dechex(0x100|ord($r[0])),1);
    if ("31040704006f"==$output) { // answer on first ident query
      socket_write($cli, "\x00\x0a\x99\x42\x42\x42\x42\x42\x30\x31\x30\x31\x71",13);
      $output.=">answered with ACK 0a99424242424230313031";
    } else if ("315307040091"==$output) { // answer on second ident query
      socket_write($cli, "\x00\x0a\x99\x42\x42\x42\x42\x42\x30\x31\x30\x31\x71",13);
      $output.=">answered with ACK 0a99424242424230313031";
    } else if ("315407040090"==$output) { // answer on third ident query with invalid CRC
      socket_write($cli, "\x00\x0a\x99\x42\x42\x42\x42\x42\x30\x31\x30\x31\x72",13);
      $output.=">answered with ACK 0a99424242424230313031 + invalid CRC";
    } else if ("315407040090"==$output) { // answer on fourth ident query with short reply
      socket_write($cli, "\x00\x09\x99\x42\x42\x42\x42\x42\x30\x31\x30\x25",12);
      $output.=">answered with ACK 09994242424242303130";
    } else if ("3155070400ce"==$output) { // answer NAK on sixth ident query
      socket_write($cli, "\x01",1);
      $output.=">answered with NACK";
    } else if ("311cb509030d0000e1"==$output || "3152b509030d0600c9"==$output || "3153b509030d060034"==$output) {
      socket_write($cli, "\x00\x02\x40\x50\xb4",5);
      $output.=">answered with ACK 024050";
    } else if ("3153b505080290909090909003f6"==$output || "3153b509050e3500190030"==$output || "3153b5050802008f030515240161"==$output) {
      socket_write($cli, "\x00\x00\x00",3);
      $output.=">answered with ACK 00";
    } else {
      $endtime=time()+20; // keep further alive
    }
  }
}
@socket_close($cli);
@socket_close($srv);
echo "server: done\n";
if ($error) {
  exit($error);
}
' &
srvpid=$!
if [ -z "$srvpid" ]; then
  echo `date` "unable to start echo server"
  exit 1
fi
sleep 1
ps -p $srvpid >/dev/null
status="$?"
if [ ! "$status" = 0 ]; then
  echo `date` "unable to start echo server"
  exit 1
fi
echo `date` "server: $srvpid"
cat >contrib/etc/ebusd/test.csv <<EOF
#no column names
u,broadcast,outsidetemp,,,fe,b516,01,temp2,m,D2B
r2,rcc.4,RoomTemp,Raumisttemp,,1c,b509,0d0000,temp,s,D2C,,C
r,mc.4,OutsideTemp,Sensor,,52,b509,0d0600,temp,s,D2C,,°C
r,mc.5,Timer.Monday,,,53,b504,02,from,s,TTM,,,Slots 1-3,to,s,TTM,,,bis,from,s,TTM,,,Slot von/bis,to,s,TTM,,,bis,from,s,TTM,,,Slot von/bis,to,s,TTM,,,bis,daysel,s,UCH,0=selected;1=Mo-Fr;2=Sa-So;3=Mo-So,,Tage
w,mc.5,Timer.Monday,,,53,b505,02,from,m,TTM,,,Slots 1-3,to,m,TTM,,,bis,from,m,TTM,,,Slot von/bis,to,m,TTM,,,bis,from,m,TTM,,,Slot von/bis,to,m,TTM,,,bis,daysel,m,UCH,0=selected;1=Mo-Fr;2=Sa-So;3=Mo-So,,Tage
r,mc.5,HeatingCurve,,,53,b509,0d3500,curve,s,UIN,100
w,mc.5,HeatingCurve,,,53,b509,0e3500,curve,m,UIN,100
w,mc.5#installer,installparam,,,53,b509,0e3f00,param,m,UCH
EOF
mkdir -p "$PWD/contrib/etc/ebusd/153"
cat >"$PWD/contrib/etc/ebusd/153/_templates.csv" <<EOF
#
temps,SCH,,°C,Temperatur
EOF
cat >"$PWD/contrib/etc/ebusd/153/53.bbbbb.csv" <<EOF
#
*r,,,,,,"9900",,,,,,,
r,,SoftwareVersion,,,,,"0000",,,HEX:4,,,
EOF
echo -e "#\ntest,testpass,installer" > acl.csv
#ebusd:
if [[ "$1" == "manual" ]]; then
  echo -e "\n\n\nSTART EBUSD NOW, enter the PID and press enter\n"
  read pid
elif [[ -n "$1" ]]; then
  echo "waiting for ebusd to be started..."
  while [[ -z "$pid" ]]; do
    pid=$(ps -C ebusd -o pid=)
  done
else
  $ebusd -d tcp:127.0.0.1:8876 --initsend --latency 10 -n -c "$PWD/contrib/etc/ebusd" --pollinterval=10 -s -a 31 --acquireretries 3 --answer --generatesyn --receivetimeout 40000 --sendretries 1 --enablehex --htmlpath "$PWD/contrib/html" --httpport 8878 --pidfile "$PWD/ebusd.pid" --localhost -p 8877 -l "$PWD/ebusd.log" --logareas all --loglevel debug --lograwdata=bytes --lograwdatafile "$PWD/ebusd.raw" --lograwdatasize 1 --dumpfile "$PWD/ebusd.dump" --dumpsize 100 -D --scanconfig --aclfile="$PWD/acl.csv" --mqttport=1883 --enablehex --enabledefine
  sleep 3
  pid=`head -n 1 "$PWD/ebusd.pid"`
fi
if [ -z "$pid" ]; then
  echo `date` "unable to start ebusd"
  kill $srvpid
  cat "$PWD/ebusd.log"
  exit 1
fi
echo `date` "ebusd: $pid"
if [[ -n "$1" ]]; then
  kill -1 $pid
fi
#client:
readarray lines <<EOF
raw bytes
reload
log
log bus,update debug
log
log all notice
grab
info
state
read -c "hallo du" ich
raw
raw
h
h r
h w
h a
h hex
h f
h l
h s
h i
h g
h define
h d
h e
h scan
h log
h raw
h dump
h reload
h q
h h
l &
r -f temp
r -f outsidetemp
r -m 10 outsidetemp
r -v outsidetemp
r -v -v outsidetemp
r -v -v -v outsidetemp
r -V outsidetemp
r -c broadcast outsidetemp
r -s f0 -c broadcast outsidetemp
r -c mc.4 -d 53 -p 2 outsidetemp
r -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.1
r -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.0
r -n -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.0
r -N -c mc.4 -d 53 -p 2 -i 1 outsidetemp temp.0
r -N -c mc.4 -d 53 -p 2 -i 1 -def "r,cir,nam,cmt,,08,b509,,,,UCH" outsidetemp temp.0
r -h
r -h 53070400
f -f outsidetemp
f -v -r temp
f -v -w temp
f -v -p temp
f -v -v temp
f -v -v -v temp
f -V temp
f -V -h temp
f -d -i b5
f -F name temp
f -e RoomTemp
f -e -c mc.5
f -e -c mc.5 -l installer
r -c mc.5 Timer.Monday
w -c mc.5 Timer.Monday "-:-;-:-;-:-;-:-;-:-;-:-;Mo-So"
w -c mc.5 Timer.Monday "00:00;23:50;00:30;00:50;03:30;06:00;Mo-Fr"
w -c mc.5 -s f0 -d 53 HeatingCurve 0.25
w -c mc.5 -d 53 installparam 123
a b
a test testpass
w -c mc.5 -d 53 installparam 123
hex fe070400
hex 53070400
dump
grab result
grab result all
grab result decode
scan full
r -c mc.5 -d g3 HeatingCurve
r -c mc.5 -d 00 HeatingCurve
r -c mc.5 -m 999999 HeatingCurve
r -c
r -d
r -p
r -i
r -Z
r -h fe070400
w -h fe070400
s
i
g
define -r "r,cir,nam,cmt,,08,b509,,,,UCH"
decode -V -N UCH 102030
encode UCH,,,,uch 10;1
raw bytes
raw
reload
nocommand
EOF
status=1
cnt=3
function send() {
#  $ebusctl -p 8877 -t 10 "$@"
  echo "$@" | nc -N -w 10 localhost 8877
}
while [[ ! "$status" = 0 ]] && [[ $cnt -gt 0 ]]; do
  sleep 5
  echo `date` "check signal"
  send state | egrep -q "signal acquired"
  status=$?
  cnt=$((cnt - 1))
done
if [ "$status" != 0 ]; then
  echo `date` "unable to acquire signal, state:"
  send state
  kill $pid
  kill $srvpid
  echo `date` "ebusd log:"
  cat "$PWD/ebusd.log"
  exit 1
fi
echo `date` "got signal"
sleep 2
echo "listen"|nc -w 10 localhost 8877 &
lstpid=$!
$ebusctl -p 8899 >/dev/null 2>/dev/null
for line in "${lines[@]}"; do
  if [ -n "$line" ]; then
    echo `date` "send: $line"
    send $line
    if [ $? -eq 1 ]; then
      echo "not connectable, exit"
      status=1
      failed=1
      break
    fi
  fi
done
scancnt=0
failed=0
while [ "$status" = 0 ]; do
  sleep 3
  output=$(send scan result)
  status=$?
  if [ $status -ne 0 ]; then
    echo "scan result status=$status"
    failed=1
    break
  fi
  echo $output | egrep -q "still running"
  status=$?
  scancnt=$(( scancnt + 1 ))
done
echo `date` "scan result after $scancnt checks:"
send scan result
if [ $? -ne 0 ]; then
  failed=1
fi
curl -s "http://localhost:8878/data/" >/dev/null
curl -s "http://localhost:8878/data/broadcast/?since=1&exact=1&required=" >/dev/null
curl -s "http://localhost:8878/data/mc.4/outsidetemp?poll=1" >/dev/null
curl -s "http://localhost:8878/data/?verbose=1" >/dev/null
curl -s "http://localhost:8878/data/?indexed=1&numeric=1" >/dev/null
curl -s "http://localhost:8878/data/?full&valuename&write&raw&def" >/dev/null
curl -s "http://localhost:8878/data/mc.4/outsidetemp2?define=r,mc.4,outsidetemp2,,,52,0102,,,,UCH" >/dev/null
curl -s "http://localhost:8878/datatypes" >/dev/null
curl -s "http://localhost:8878/raw" >/dev/null
curl -s "http://localhost:8878/decode?def=UCH&raw=1f" >/dev/null
curl -s "http://localhost:8878/data/mc.5/installparam?poll=1&user=test&secret=testpass" >/dev/null
curl -s -T test_coverage.sh http://localhost:8878/data/
echo `date` "commands done"
kill $lstpid
verify=`send info|egrep "^address 04:"`
expect='address 04: slave #25, scanned "MF=153;ID=BBBBB;SW=3031;HW=3031"'
if [ "x$verify" != "x$expect" ]; then
  echo -e `date` "error unexpected result from info command:\n  expected: >$expect<\n  got: >$verify<"
  ls -latr
  kill $pid
  kill $srvpid
  echo `date` "ebusd log:"
  cat "$PWD/ebusd.log"
  exit 1
fi
send log all debug
if [[ -z "$1" ]]; then
  kill $pid
else
  kill -9 $pid
fi
sleep 2
echo `date` "ebusd log:"
cat "$PWD/ebusd.log"
echo `date` "done."
wait $srvpid
exit $failed

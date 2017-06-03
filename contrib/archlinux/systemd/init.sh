#!/bin/bash
# A wrapper taking care about 'setserial $dev low_latency' bug:
#   https://github.com/john30/ebusd/issues/79
# Needed only for starting, systemd takes care about stopping the daemon.

DAEMON=/usr/bin/ebusd

if [ ! -x $DAEMON ]; then
  echo "$DAEMON is not available or not executable."
  exit 5
fi

start_instance () {
  local opts dev
  opts=$@
  dev=`echo " ${opts}"|sed -e 's#^.* -d ##' -e 's# -.*$##'`
  if [ -z "$dev" ]; then
    dev=/dev/ttyUSB0
  fi
  if [ "${dev:0:5}" == '/dev/' ] && [ -c "$dev" ] && [ -x /bin/setserial ]; then
    echo "Setting low latency for ebusd device $dev"
    /bin/setserial "$dev" low_latency
    echo $?
  fi
  echo "Starting ebusd $opts"
  $DAEMON ${opts}
  echo $?
}

case $1 in
  start)
    shift
    start_instance $@
      ;;
  *)
    echo "Usage: $0 start"
    exit 2
    ;;
esac

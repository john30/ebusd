#!/bin/sh
port=8888
if [ "x$1" = "x-p" ]; then
  shift
  port=$1
  shift
fi
readargs=
if [ "x$1" = "x-R" ]; then
  shift
  readargs=$1
  shift
fi
for i in `echo "f -F circuit,name" "$@"|nc -q 1 127.0.0.1 $port|sort -u|egrep ','`; do
  circuit=${i%%,*}
  name=${i##*,}
  if [ -z "$circuit" ] || [ -z "$name" ] || [ "$circuit,$name" = "scan,id" ]; then
    continue
  fi
  ret=`echo "r ${readargs} -c $circuit $name" |nc -q 1 127.0.0.1 $port|head -n 1`
  echo "$circuit $name = $ret"
done

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
for i in `echo "f -F circuit,name" "$@"|nc 127.0.0.1 $port|sort -u|egrep ','`; do
  ret=`echo "r ${readargs} -c" ${i%%,*} ${i##*,}|nc 127.0.0.1 $port|head -n 1`
  echo ${i%%,*} ${i##*,} "=" $ret
done

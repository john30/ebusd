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
for i in `echo "f" "$@"|nc localhost $port|cut -d ' ' -f '1-2'|sed -e 's# #:#'`; do
  ret=`echo "r ${readargs} -c" ${i%%:*} ${i##*:}|nc localhost $port|head -n 1`
  echo ${i%%:*} ${i##*:} "=" $ret
done

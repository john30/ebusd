#!/bin/sh
for i in `echo "f" "$@"|nc localhost 8888|cut -d ' ' -f '1-2'|sed -e 's# #:#'`; do
  ret=`echo "r -c" ${i%%:*} ${i##*:}|nc localhost 8888|head -n 1`
  echo ${i%%:*} ${i##*:} "=" $ret
done

#!/bin/sh
tags=`git fetch -t`
if [ -n "$tags" ]; then
  echo "tags were updated:"
  echo $tags
  echo "git pull is recommended. stopped."
  exit 1
fi
./make_debian.sh --keepbuilddir $@
./make_debian.sh --reusebuilddir --dontforce --without-mqtt $@

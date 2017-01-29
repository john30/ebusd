#!/bin/sh
set -e
export EBUSD_VERSION=`cat ../../VERSION`
export EBUSD_ARCH=`docker version|grep -i "Arch[^:]*server"|head -n 1|sed -e 's#^.*/##'`
sed -i -e "s#^ENV EBUSD_VERSION .*\$#ENV EBUSD_VERSION ${EBUSD_VERSION}#" -e "s#^ENV EBUSD_ARCH .*\$#ENV EBUSD_ARCH ${EBUSD_ARCH}#" runtime/Dockerfile

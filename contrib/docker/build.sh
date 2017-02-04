#!/bin/sh
stamp=`docker inspect -f '{{.Created}}' ebusd-buildenv 2>/dev/null`
if [ -z "$stamp" ] || ( rm -f .stamp && touch -d "$stamp" .stamp && [ buildenv/Dockerfile -nt .stamp ] ) ; then
  echo "updating ebusd build environment..."
  docker rmi -f ebusd-buildenv 2>/dev/null
  (cd buildenv && docker build -t ebusd-buildenv .) || exit 1
fi
set -e
echo "creating debian image..."
docker run --rm -it -v `pwd`/../..:/build ebusd-buildenv ./make_debian.sh
export EBUSD_VERSION=`cat ../../VERSION`
export EBUSD_ARCH=`docker version|grep -i "Arch[^:]*server"|head -n 1|sed -e 's#^.*/##'`
mv ../../ebusd-${EBUSD_VERSION}_${EBUSD_ARCH}_mqtt1.deb runtime/
echo "building docker image..."
(cd runtime && docker build -t ebusd .)
echo "docker image created."

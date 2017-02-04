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
cat <<EOF
docker image created.

to run it interactively on serial device /dev/ttyUSB1:
docker run --rm -it --device=/dev/ttyUSB1:/dev/ttyUSB0 -p 127.0.0.1:8888:8888 ebusd

to start it in background on serial device /dev/ttyUSB1:
docker run device=/dev/ttyUSB1:/dev/ttyUSB0 -p 127.0.0.1:8888:8888 ebusd

when using a network device, the "--device" argument to docker can be omitted:
docker run --rm -it -p 127.0.0.1:8888:8888 ebusd -f --scanconfig -d udp:192.168.178.123:10000 --latency=80000
EOF

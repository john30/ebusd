#!/bin/sh
# ebusd - daemon for communication with eBUS heating systems.
# Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

echo "*************"
echo " prepare"
echo "*************"
echo
VERSION=`head -n 1 VERSION`
ARCH=`dpkg --print-architecture`
BUILD="build-$ARCH"
RELEASE="ebusd-$VERSION"
PACKAGE="${RELEASE}_${ARCH}"
rm -rf "$BUILD"
mkdir -p "$BUILD" || exit 1
cd "$BUILD" || exit 1
(tar cf - -C .. "--exclude=./$BUILD" --exclude=./.* "--exclude=*.o" "--exclude=*.a" .| tar xf -) || exit 1

echo
echo "*************"
echo " build"
echo "*************"
echo
./autogen.sh $@ || exit 1
make DESTDIR="$PWD/$RELEASE" install-strip || exit 1
extralibs=
ldd $RELEASE/usr/bin/ebusd | egrep -q libmosquitto.so.0
if [ $? -eq 0 ]; then
  extralibs=', libmosquitto0'
  PACKAGE="${PACKAGE}_mqtt0"
else
  ldd $RELEASE/usr/bin/ebusd | egrep -q libmosquitto.so.1
  if [ $? -eq 0 ]; then
    extralibs=', libmosquitto1'
    PACKAGE="${PACKAGE}_mqtt1"
  fi
fi

echo
echo "*************"
echo " pack"
echo "*************"
echo
mkdir -p $RELEASE/DEBIAN $RELEASE/etc/default $RELEASE/etc/ebusd $RELEASE/etc/logrotate.d || exit 1
rm $RELEASE/usr/bin/ebusfeed
if [ -d /run/systemd/system ]; then
  mkdir -p $RELEASE/lib/systemd/system || exit 1
  cp contrib/debian/systemd/ebusd.service $RELEASE/lib/systemd/system/ebusd.service || exit 1
else
  mkdir -p $RELEASE/etc/init.d || exit 1
  cp contrib/debian/init.d/ebusd $RELEASE/etc/init.d/ebusd || exit 1
fi
  cp contrib/debian/default/ebusd $RELEASE/etc/default/ebusd || exit 1
cp contrib/etc/ebusd/* $RELEASE/etc/ebusd/ || exit 1
cp contrib/etc/logrotate.d/ebusd $RELEASE/etc/logrotate.d/ || exit 1
cp ChangeLog.md $RELEASE/DEBIAN/changelog || exit 1
cat <<EOF > $RELEASE/DEBIAN/control
Package: ebusd
Version: $VERSION
Section: net
Priority: required
Architecture: $ARCH
Maintainer: John Baier <ebusd@ebusd.eu>
Homepage: https://github.com/john30/ebusd
Bugs: https://github.com/john30/ebusd/issues
Depends: logrotate, libstdc++6 (>= 4.8.1), libc6, libgcc1$extralibs
Description: eBUS daemon.
 ebusd is a daemon for handling communication with eBUS devices connected to a
 2-wire bus system.
EOF
cat <<EOF > $RELEASE/DEBIAN/dirs
/etc/ebusd
/etc/default
/etc/logrotate.d
/usr/bin
EOF
if [ -d /run/systemd/system ]; then
  echo /lib/systemd/system >> $RELEASE/DEBIAN/dirs
else
  echo /etc/init.d >> $RELEASE/DEBIAN/dirs
fi

cat <<EOF > $RELEASE/DEBIAN/postinst
#!/bin/sh
echo "Instructions:"
echo "1. Edit /etc/default/ebusd if necessary"
echo "   (especially if your device is not /dev/ttyUSB0)"
echo "2. Place CSV configuration files in /etc/ebusd/"
echo "   (see https://github.com/john30/ebusd-configuration)"
echo "3. To start the daemon, enter 'service ebusd start'"
echo "4. Check the log file /var/log/ebusd.log"
EOF
chmod 755 $RELEASE/DEBIAN/postinst

dpkg -b $RELEASE || exit 1
mv ebusd-$VERSION.deb "../${PACKAGE}.deb" || exit 1

echo
echo "*************"
echo " cleanup"
echo "*************"
echo
cd ..
rm -rf "$BUILD"

echo
echo "Package created: ${PACKAGE}.deb"
echo
echo "Content:"
dpkg -c "${PACKAGE}.deb"

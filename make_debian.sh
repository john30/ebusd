#!/bin/sh
# ebusd - daemon for communication with eBUS heating systems.
# Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
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
PACKAGE="${RELEASE}_${ARCH}.deb"
rm -rf "$BUILD"
mkdir -p "$BUILD" || exit 1
cd "$BUILD" || exit 1
(tar cf - -C .. "--exclude=./$BUILD" --exclude=./.* .| tar xf -) || exit 1

echo
echo "*************"
echo " build"
echo "*************"
echo
./autogen.sh || exit 1
make DESTDIR="$PWD/$RELEASE" install-strip || exit 1

echo
echo "*************"
echo " pack"
echo "*************"
echo
mkdir -p $RELEASE/DEBIAN $RELEASE/etc/init.d $RELEASE/etc/default $RELEASE/etc/logrotate.d || exit 1
rm $RELEASE/usr/bin/ebusfeed
cp contrib/etc/init.d/ebusd.debian $RELEASE/etc/init.d/ebusd || exit 1
cp contrib/etc/default/ebusd.debian $RELEASE/etc/default/ebusd || exit 1
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
Depends: libstdc++6, libc6, libgcc1
Description: eBUS daemon.
 ebusd is a daemon for handling communication with eBUS devices connected to a
 2-wire bus system.
EOF
cat <<EOF > $RELEASE/DEBIAN/dirs
/etc/ebusd
/etc/init.d
/etc/default
/etc/logrotate.d
/usr/bin
EOF

dpkg -b $RELEASE || exit 1
mv ebusd-$VERSION.deb "../$PACKAGE" || exit 1

echo
echo "*************"
echo " cleanup"
echo "*************"
echo
cd ..
rm -rf "$BUILD"

echo
echo "Package created: $PACKAGE"
echo
echo "Content:"
dpkg -c "$PACKAGE"


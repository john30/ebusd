#!/bin/sh
# Copyright (C) John Baier 2014-2015 <ebusd@johnm.de>
#
# This file is part of ebusd.
#
# ebusd is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ebusd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ebusd. If not, see http://www.gnu.org/licenses/.

echo "*************"
echo " prepare"
echo "*************"
echo
mkdir -p debian-build/ebusd || exit 1
cd debian-build/ebusd || exit 1
(tar cf - -C  ../.. --exclude=debian-build --exclude=./.* .| tar xf -) || exit 1

echo
echo "*************"
echo " build"
echo "*************"
echo
./autogen.sh || exit 1
make || exit 1

echo
echo "*************"
echo " pack"
echo "*************"
echo
mkdir -p release/DEBIAN release/etc/init.d release/etc/default release/etc/ebusd release/etc/logrotate.d release/usr/bin release/usr/bin || exit 1

cp contrib/etc/init.d/ebusd.debian release/etc/init.d/ebusd || exit 1
cp contrib/etc/default/ebusd.debian release/etc/default/ebusd || exit 1
cp contrib/etc/ebusd/* release/etc/ebusd/ || exit 1
cp contrib/etc/logrotate.d/ebusd release/etc/logrotate.d/ || exit 1
strip src/ebusd/ebusd || exit 1
cp src/ebusd/ebusd release/usr/bin/ || exit 1
strip src/tools/ebusctl || exit 1
cp src/tools/ebusctl release/usr/bin/ || exit 1
cp ChangeLog.md release/DEBIAN/changelog || exit 1
VERSION=`head -n 1 VERSION`

ARCH=`dpkg --print-architecture`
cat <<EOF > release/DEBIAN/control
Package: ebusd
Version: $VERSION
Section: net
Priority: required
Architecture: $ARCH
Maintainer: John Baier <ebusd@johnm.de>
Homepage: https://github.com/john30/ebusd
Bugs: https://github.com/john30/ebusd/issues
Depends: libstdc++6, libc6, libgcc1
Description: eBUS daemon.
 ebusd is a daemon for handling communication with eBUS devices connected to a
 2-wire bus system.
EOF
cat <<EOF > release/DEBIAN/dirs
/etc/ebusd
/usr/bin/
/etc/init.d
/etc/default
/etc/logrotate.d
EOF

mv release ebusd-$VERSION || exit 1
dpkg -b ebusd-$VERSION || exit 1
mv ebusd-$VERSION.deb ../../ebusd-${VERSION}_${ARCH}.deb || exit 1

echo
echo "*************"
echo " cleanup"
echo "*************"
echo
cd ../..
rm -rf debian-build

echo
echo "Debian package created: ./ebusd-${VERSION}_${ARCH}.deb"


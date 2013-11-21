#!/bin/sh
#
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
#

BUILD_DIR=/tmp
VERSION='1.0'

cd $BUILD_DIR
printf ">>> Build directory $BUILD_DIR create $BUILD_DIR/ebusd_build\n"
mkdir ebusd-build
cd ebusd-build

printf ">>> Checkout sources\n"
#svn checkout https://svn.code.sf.net/p/openautomation/code/tools/ebusd
git clone https://github.com/yuhu-/ebusd.git
cd ebusd

printf ">>> Remove hidden files\n"
find $PWD -name .svn -print0 | xargs -0 rm -r
find $PWD -name .gitignore -print0 | xargs -0 rm -r

printf ">>> Build binarys from source\n"
./autogen.sh

printf ">>> Create Debian package related files\n"
mkdir trunk
mkdir trunk/DEBIAN
mkdir trunk/etc
mkdir trunk/etc/init.d
mkdir trunk/usr
mkdir trunk/usr/sbin

cp contrib/debian/init.d trunk/etc/init.d/ebusd
cp -R contrib/etc/* trunk/etc
cp src/ebusd trunk/usr/sbin/ebusd
cp -R contrib/csv/* /etc/ebusd/

cp ChangeLog trunk/DEBIAN/changelog
ARCH=`dpkg --print-architecture`
echo "Package: ebusd\nVersion: $VERSION\nSection: net\nPriority: required\nArchitecture: $ARCH\nMaintainer: Roland Jax <roland.jax@liwest.at>\nDepends:\nDescription: ebus Daemon (ebusd)\n" > trunk/DEBIAN/control
echo "/etc/ebusd\n/usr/sbin/\n/etc/init.d\n/etc/logrotate.d\n/etc/udev\n/etc/udev/rules\n" > trunk/DEBIAN/dirs

mkdir ebusd-$VERSION
cp -R trunk/* ebusd-$VERSION
dpkg -b ebusd-$VERSION

printf ">>> Move Package to $BUILD_DIR\n"
mv ebusd-$VERSION.deb $BUILD_DIR/ebusd-${VERSION}_${ARCH}.deb

printf ">>> Remove Files\n"
cd $BUILD_DIR
rm -R ebusd-build

printf ">>> Debian Package created at $BUILD_DIR/ebusd-${VERSION}_${ARCH}.deb\n"

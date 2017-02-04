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
ARCH=freetz
BUILD="build-$ARCH"
RELEASE="ebusd-$VERSION"
PACKAGE="${RELEASE}_${ARCH}.tgz"
export PATH=$PATH:/usr/mipsel-linux-uclibc/bin
rm -rf "$BUILD"
mkdir -p "$BUILD" || exit 1
cd "$BUILD" || exit 1
(tar cf - -C .. "--exclude=./$BUILD" --exclude=./.* "--exclude=*.o" "--exclude=*.a" .| tar xf -) || exit 1

echo
echo "*************"
echo " build"
echo "*************"
echo
./autogen.sh --host=mipsel-linux-uclibc || exit 1
make DESTDIR="$PWD/$RELEASE" install-strip || exit 1

echo
echo "*************"
echo " pack"
echo "*************"
echo
cp contrib/etc/ebusd/* $RELEASE/ || exit 1
mv $RELEASE/usr/bin/ebusd $RELEASE/usr/bin/ebusctl $RELEASE/ || exit 1
rm -rf $RELEASE/usr/ $RELEASE/etc/ || exit 1

cat <<EOF > $RELEASE/INFO
Package: ebusd
Version: $VERSION
Maintainer: John Baier <ebusd@ebusd.eu>
Homepage: https://github.com/john30/ebusd
Bugs: https://github.com/john30/ebusd/issues
Depends: libstdc++6 (>= 4.8.1), libc6, libgcc1
Description: eBUS daemon.
 ebusd is a daemon for handling communication with eBUS devices connected to a
 2-wire bus system.
EOF

tar czf $RELEASE.tgz $RELEASE || exit 1
mv $RELEASE.tgz "../$PACKAGE" || exit 1

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
tar tzvf "$PACKAGE"


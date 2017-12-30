#!/bin/sh -e
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

mkdir -p "$srcdir/build"

if [ "x$1" = "x--dontforce" ]; then
  shift
  autoreconf --install --verbose "$srcdir"
else
  autoreconf --force --install --verbose "$srcdir"
fi
test -n "$NOCONFIGURE" || "$srcdir/configure" "--prefix=/usr" "--sysconfdir=/etc" "--localstatedir=/var" "$@"

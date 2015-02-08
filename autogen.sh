#!/bin/sh -e
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

mkdir -p "$srcdir/build"

autoreconf --force --install --verbose "$srcdir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "--localstatedir=/var" "$@"

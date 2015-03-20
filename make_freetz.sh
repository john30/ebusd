#!/bin/sh
export PATH=$PATH:/usr/mipsel-linux-uclibc/bin
./autogen.sh --host=mipsel-linux-uclibc
make

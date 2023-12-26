#!/bin/bash
# helper script to check the build from git master is fine
docker run -it --rm -v $PWD:/build -w /build/contrib/archlinux archlinux sh -c 'pacman -Sy && pacman -Sq fakeroot binutils mosquitto autoconf automake make gcc git && useradd test && su test -c "makepkg -p PKGBUILD.git"'
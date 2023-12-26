#!/bin/bash
# helper script to check the release build is fine
docker run -it --rm -v $PWD:/build -w /build/contrib/archlinux archlinux sh -c 'pacman -Sy && pacman -Sq fakeroot binutils mosquitto autoconf automake make gcc && useradd test && su test -c "makepkg"'
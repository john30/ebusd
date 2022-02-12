#!/bin/bash
# helper script to check the build is fine
docker run -it --rm -v $PWD:/build -w /build/contrib/alpine alpine sh -c 'apk add --upgrade abuild && adduser -D test && addgroup test abuild && su test -c "abuild-keygen && abuild -r"'
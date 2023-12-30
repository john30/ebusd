#!/bin/bash
# helper script to check the release build is fine
docker run -it --rm -v $PWD:/build -w /build/contrib/alpine alpine:edge sh -c 'apk add --upgrade abuild build-base && adduser -D test && addgroup test abuild && su test -c "abuild-keygen -a && abuild -r"'
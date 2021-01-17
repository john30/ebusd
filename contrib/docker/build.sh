#!/bin/sh
archs='amd64 i386 arm32v7 arm64v8'
UPLOAD_URL='http://'`hostname`'/ebusdreleaseupload.php'
UPLOAD_CREDENTIALS='anonymous:build'
for arch in $archs; do
  if [ "$arch" != "amd64" ]; then
    suffix=".$arch"
  else
    suffix=''
  fi
  docker build --build-arg "UPLOAD_URL=$UPLOAD_URL" --build-arg "UPLOAD_CREDENTIALS=$UPLOAD_CREDENTIALS" --build-arg "UPLOAD_ONLY=1" -f release/Dockerfile$suffix .
done


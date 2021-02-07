#!/bin/bash
archs='amd64 i386 arm32v5 arm32v7 arm64v8'
images='jessie stretch buster'
UPLOAD_URL='http://'`hostname`'/ebusdreleaseupload.php'
UPLOAD_CREDENTIALS='anonymous:build'
for image in $images; do
  dir=$image
  mkdir -p $dir
  BASE_IMAGE=debian:$image ./update.sh $dir/
  for arch in $archs; do
    if [ "$arch" != "amd64" ]; then
      suffix=".$arch"
    else
      suffix=''
    fi
    docker build --build-arg "UPLOAD_URL=$UPLOAD_URL" --build-arg "UPLOAD_CREDENTIALS=$UPLOAD_CREDENTIALS" --build-arg "UPLOAD_OS=$image" --build-arg "UPLOAD_ONLY=1" -f $dir/Dockerfile$suffix .
  done
  rm -rf $dir
done


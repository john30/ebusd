#!/bin/bash
if [[ -z "$1" ]]; then
  echo "usage: $0 [release|UPLOADHOST]"
  echo "  without arguments: build and push devel docker images"
  echo "  release: build and push release docker images from latest release binaries"
  echo "  UPLOADHOST: build Debian release packages (with and without MQTT) and upload them to UPLOADHOST"
  exit 1
fi
archs=linux/amd64,linux/386,linux/arm/v7,linux/arm64
if [[ -z "$1" ]] || [[ "x$1" == "xrelease" ]]; then
  UPLOAD_URL=
else
  UPLOAD_URL="http://$1/ebusdreleaseupload.php"
fi
UPLOAD_CREDENTIALS='anonymous:build'
version=`cat ../../VERSION`
source='../..'
images='bullseye'
tagprefix=docker.io/john30/ebusd
extratag=

if [[ -z "$1" ]]; then
  namesuffix=''
  target=image
  outputFmt='-o type=docker,type=registry'
  tagsuffix=':devel'
elif [[ "x$1" = "xrelease" ]]; then
  archs=linux/amd64
  namesuffix='.release'
  target=image
  outputFmt='-o type=registry'
  tagsuffix=":v$version"
  extratag="-t $tagprefix:latest"
else
  namesuffix='.build'
  target=build
  images='bullseye buster stretch'
  outputFmt=-q
  tagsuffix=":v$version-prep"
fi

for image in $images; do
  output=$(echo "$outputFmt"|sed -e "s#%IMAGE%#$image#g")
  docker buildx build \
    --target $target \
    --progress pain \
    --platform $archs \
    -f Dockerfile${namesuffix} \
    --build-arg "BASE_IMAGE=debian:$image" \
    --build-arg "EBUSD_VERSION=$version" \
    --build-arg "EBUSD_IMAGE=$image" \
    --build-arg "UPLOAD_URL=$UPLOAD_URL" \
    --build-arg "UPLOAD_CREDENTIALS=$UPLOAD_CREDENTIALS" \
    --build-arg "UPLOAD_OS=$image" \
    -t $tagprefix$tagsuffix \
    $extratag \
    $output \
    $source
done

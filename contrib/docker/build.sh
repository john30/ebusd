#!/bin/bash
if [[ "$1" == "-h" ]]; then
  echo "usage: $0 [release|UPLOADHOST]"
  echo "  without arguments: build and push devel docker images"
  echo "  release: build and push release docker images from latest release binaries"
  echo "  UPLOADHOST: build Debian release packages (with and without MQTT) and upload them to UPLOADHOST"
  exit 1
fi
archs=linux/amd64,linux/386,linux/arm/v7,linux/arm64
if [[ -n "$LIMITARCH" ]]; then
  archs=$(echo ",$archs," | sed -e "s#.*,\([^,/]*/$LIMITARCH\),.*#\1#")
  echo "limiting to arch $archs"
fi
if [[ -z "$1" ]] || [[ "x$1" == "xrelease" ]]; then
  UPLOAD_URL=
else
  UPLOAD_CREDENTIALS=${UPLOAD_CREDENTIALS:-'anonymous:build'}
fi
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
  namesuffix='.release'
  target=image
  outputFmt='-o type=registry'
  tagsuffix=":v$version"
  extratag="-t $tagprefix:latest"
else
  namesuffix='.build'
  target=deb
  images='bullseye buster stretch'
  if [[ -n "$LIMITIMG" ]]; then
    images=$(echo " $images " | sed -e "s#.* \($LIMITIMG\) .*#\1#")
    echo "limiting to image $images"
  fi
  outputFmt='-o out/%IMAGE%'
  tagsuffix=":v$version-prep"
fi

for image in $images; do
  output=$(echo "$outputFmt"|sed -e "s#%IMAGE%#$image#g")
  docker buildx build \
    --target $target \
    --progress plain \
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

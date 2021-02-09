#!/bin/bash
DEFAULT_IMAGE=debian:stretch
EBUSD_VERSION=`cat ../../VERSION`

archs='amd64 i386 arm32v5:arm arm32v7:arm arm64v8:aarch64'

function replaceTemplate () {
  prefix=
  suffix=
  qemu_from_line=
  qemu_from_copy=
  extra_rm=
  if [ "$arch" != "amd64" ]; then
    qemu="${arch##*:}"
    arch="${arch%%:*}"
    prefix="$arch/"
    suffix=".$arch"
    qemu_from_line="FROM multiarch/qemu-user-static as qemu"
    qemu_from_copy="COPY --from=qemu /usr/bin/qemu-$qemu-static /usr/bin/"
    extra_rm=" /usr/bin/qemu-$qemu-static"
  fi
  file="${dir}Dockerfile${namesuffix}${suffix}"
  sed \
    -e "s#%QEMU_FROM_LINE%#${qemu_from_line}#g" \
    -e "s#%BASE_IMAGE%#${prefix}${BASE_IMAGE:-$DEFAULT_IMAGE}#g" \
    -e "s#%QEMU_FROM_COPY%#${qemu_from_copy}#g" \
    -e "s#%EBUSD_MAKE%#${make}#g" \
    -e "s#%EBUSD_VERSION%#${EBUSD_VERSION}#g" \
    -e "s#%EBUSD_VERSION_VARIANT%#${version_variant}#g" \
    -e "s#%EBUSD_ARCH%#${arch}#g" \
    -e "s#%EXTRA_RM%#${extra_rm}#g" \
    -e "s#%EBUSD_UPLOAD_LINES%#${upload_lines}#g" \
    Dockerfile.template > "$file"
  echo "updated $file"
}

if [[ -z "$1" ]]; then
  # devel updates
  version_variant='-devel'
  make='./make_debian.sh'
  dir=''
  upload_line=''
  namesuffix=''
  for arch in $archs; do
    replaceTemplate
  done
fi

# release updates
version_variant=''
dir="$1"
if [[ -n "$1" ]]; then
  make='./make_all.sh'
  upload_lines='ARG UPLOAD_URL\nARG UPLOAD_CREDENTIALS\nARG UPLOAD_OS\nRUN if [ -n "\$UPLOAD_URL" ] \&\& [ -n "\$UPLOAD_CREDENTIALS" ]; then for img in ebusd-*.deb; do echo -n "upload \$img: "; curl -fs -u "\$UPLOAD_CREDENTIALS" -X POST --data-binary "@\$img" -H "Content-Type: application/octet-stream" "\$UPLOAD_URL/\$img?a=\$EBUSD_ARCH\&o=\$UPLOAD_OS\&v=\$EBUSD_VERSION" || echo "failed"; done; fi'
  namesuffix=''
else
  make='./make_debian.sh'
  namesuffix='.release'
fi
for arch in $archs; do
  replaceTemplate
done


#!/bin/bash

function replaceTemplate () {
  file="Dockerfile${namesuffix}"
  sed \
    -e "s#%EBUSD_MAKE%#${make}#g" \
    -e "s#%EBUSD_VERSION_VARIANT%#${version_variant}#g" \
    -e "s#%EBUSD_UPLOAD_LINES%#${upload_lines}#g" \
    -e "s#%EBUSD_COPYDEB%#${copydeb}#g" \
    -e "s#%EBUSD_DEBSRC%#${debsrc}#g" \
    -e "s#%EBUSD_COPYENTRY%#${copyentry}#g" \
    Dockerfile.template > "$file"
  echo "updated $file"
}

# devel update
version_variant='-devel'
make='RUNTEST=full ./make_debian.sh'
upload_lines=''
copydeb='COPY --from=build /build/ebusd-*_mqtt1.deb ebusd.deb'
debsrc='ebusd.deb \&\& rm -f ebusd.deb'
copyentry='COPY --from=build /build/contrib/docker/docker-entrypoint.sh /'
namesuffix=''
replaceTemplate

# release update
version_variant=''
make='./make_debian.sh'
copydeb="ADD https://github.com/john30/ebusd/releases/download/v\${EBUSD_VERSION}/ebusd-\${EBUSD_VERSION}_\${TARGETARCH}\${TARGETVARIANT}-\${EBUSD_IMAGE}_mqtt1.deb ebusd.deb"
copyentry='COPY contrib/docker/docker-entrypoint.sh /'
namesuffix='.release'
replaceTemplate

if [[ -n "$1" ]]; then
  # build releases update
  make='RUNTEST=full ./make_all.sh'
  upload_lines='ARG UPLOAD_URL\nARG UPLOAD_CREDENTIALS\nARG UPLOAD_OS\nRUN if [ -n "\$UPLOAD_URL" ] \&\& [ -n "\$UPLOAD_CREDENTIALS" ]; then for img in ebusd-*.deb; do echo -n "upload \$img: "; curl -fsSk -u "\$UPLOAD_CREDENTIALS" -X POST --data-binary "@\$img" -H "Content-Type: application/octet-stream" "\$UPLOAD_URL/\$img?a=\$EBUSD_ARCH\&o=\$UPLOAD_OS\&v=\$EBUSD_VERSION" || echo "failed"; done; fi'
  upload_lines+='\n\n\nFROM scratch as deb\nCOPY --from=build /build/*.deb /'
  namesuffix='.build'
  replaceTemplate
fi

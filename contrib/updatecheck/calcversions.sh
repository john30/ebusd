#!/bin/sh
version=`head -n 1 ../../VERSION`
revision=`git describe --always`
echo "ebusd=${version},${revision}" > cdnversions.txt
devver=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
devbl=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Bootloader version"|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devbl}" >> cdnversions.txt
devver=`curl -s https://adapter.ebusd.eu/v5/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*id="\([^"]*\)".*<code[^>]*>\([^<]*\)<.*#\2,\2,\1#'`
echo "device=${devver}" >> cdnversions.txt
curl -sS https://ebus.github.io/en/versions.json -o veren.json
curl -sS https://ebus.github.io/de/versions.json -o verde.json
node -e 'fs=require("fs");e=JSON.parse(fs.readFileSync("veren.json","utf-8"));d=JSON.parse(fs.readFileSync("verde.json","utf-8"));console.log(Object.entries(d).map(([k,de])=>{en=e[k];return `${k}=${de.hash},${de.size},${de.mtime},${en.hash},${en.size},${en.mtime}`;}).join("\n"))'|sort >> cdnversions.txt

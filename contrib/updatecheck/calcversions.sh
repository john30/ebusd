#!/bin/sh
version=`head -n 1 ../../VERSION`
revision=`git describe --always`
echo "ebusd=${version},${revision}" > versions.txt
# cp versions.txt > oldversions.txt
devver=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
devbl=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Bootloader version"|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devbl}" >> versions.txt
devver=`curl -s https://adapter.ebusd.eu/v5/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devver}" >> versions.txt
cp versions.txt cdnversions.txt
files=`find config/de -type f -or -type l`
../../src/lib/ebus/test/test_filereader $files|sed -e 's#^config/de/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> versions.txt
files=`find config/en -type f -or -type l`
for filever in $(../../src/lib/ebus/test/test_filereader $files|sed -e 's#^config/en/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'); do
  file=${filever%%=*}
  ver=${filever#*=}
  sed -i -e "s#^$file=\(.*\)\$#$file=\1,$ver#" versions.txt
done
#./oldtest_filereader $files|sed -e 's#^config/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> oldversions.txt
curl -sS https://ebus.github.io/en/versions.json -o veren.json
curl -sS https://ebus.github.io/de/versions.json -o verde.json
node -e 'fs=require("fs");e=JSON.parse(fs.readFileSync("veren.json","utf-8"));d=JSON.parse(fs.readFileSync("verde.json","utf-8"));console.log(Object.entries(e).map(([k,v])=>{w=d[k];return `${k}=${v.hash},${v.size},${v.mtime},${w.hash},${w.size},${w.mtime}`;}).join("\n"))' >> cdnversions.txt

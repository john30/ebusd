#!/bin/sh
version=`head -n 1 ../../VERSION`
revision=`git describe --always`
echo "ebusd=${version},${revision}" > versions.txt
echo "ebusd=${version},${revision}" > oldversions.txt
devver=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
devbl=`curl -s https://adapter.ebusd.eu/v31/firmware/ChangeLog|grep "<h[0-9].*>Bootloader version"|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devbl}" >> versions.txt
devver=`curl -s https://adapter.ebusd.eu/v5/ChangeLog|grep "<h[0-9].*>Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devver}" >> versions.txt
files=`find config/de -type f -or -type l`
../../src/lib/ebus/test/test_filereader $files|sed -e 's#^config/de/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> versions.txt
files=`find config/en -type f -or -type l`
for filever in $(../../src/lib/ebus/test/test_filereader $files|sed -e 's#^config/en/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'); do
  file=${filever%%=*}
  ver=${filever#*=}
  sed -i -e "s#^$file=\(.*\)\$#$file=\1,$ver#" versions.txt
done
#./oldtest_filereader $files|sed -e 's#^config/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> oldversions.txt

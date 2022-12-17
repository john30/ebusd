#!/bin/sh
version=`head -n 1 ../../VERSION`
revision=`git describe --always`
echo "ebusd=${version},${revision}" > versions.txt
echo "ebusd=${version},${revision}" > oldversions.txt
devver=`curl -s https://adapter.ebusd.eu/firmware/ChangeLog|grep "Version "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
devbl=`curl -s https://adapter.ebusd.eu/firmware/ChangeLog|grep "Bootloader "|head -n 1|sed -e 's#.*<code[^>]*>##' -e 's#<.*##' -e 's# ##'`
echo "device=${devver},${devbl}" >> versions.txt
files=`find config/ -type f -or -type l`
../../src/lib/ebus/test/test_filereader $files|sed -e 's#^config/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> versions.txt
#./oldtest_filereader $files|sed -e 's#^config/##' -e 's#^\([^ ]*\) #\1=#' -e 's# #,#g'|sort >> oldversions.txt

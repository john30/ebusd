#!/bin/sh
version=`head -n 1 VERSION`
revision=`git describe --always`
echo "ebusd=${version}.${revision}" > versions.txt
#TODO add calculation for config files

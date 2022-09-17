ebusd - eBUS daemon
===================

ebusd is a daemon for handling communication with eBUS devices connected to a
2-wire bus system ("energy bus" used by numerous heating systems).

[![Build](https://github.com/john30/ebusd/actions/workflows/build.yml/badge.svg)](https://github.com/john30/ebusd/actions/workflows/build.yml)
[![codecov](https://codecov.io/gh/john30/ebusd/branch/master/graph/badge.svg)](https://codecov.io/gh/john30/ebusd)
[![Release Downloads](https://img.shields.io/github/downloads/john30/ebusd/total)](https://github.com/john30/ebusd/releases/latest)
[![Docker Downloads](https://img.shields.io/docker/pulls/john30/ebusd)](https://hub.docker.com/repository/docker/john30/ebusd)
[![Release](https://img.shields.io/github/v/release/john30/ebusd)](https://github.com/john30/ebusd/releases/latest)
[![GitHub Discussions](https://img.shields.io/github/discussions/john30/ebusd)](https://github.com/john30/ebusd/discussions)
[![Sponsors](https://img.shields.io/github/sponsors/john30)](https://github.com/sponsors/john30)

Features
--------

The main features of the daemon are:

 * use one of these device connections:
   * USB serial
   * TCP
   * UDP
   * enhanced ebusd protocol allowing arbitration to be done directly by the hardware, e.g. for recent
     * [ebus adapter 3](https://adapter.ebusd.eu/), or
     * [ebusd-esp firmware](https://github.com/john30/ebusd-esp/)
 * actively send messages to and receive answers from the eBUS
 * passively listen to messages sent on the eBUS
 * regularly poll for messages
 * cache all messages
 * scan for bus participants and automatically pick matching message configuration files from config web service at ebusd.eu (or alternatively local files)
 * parse messages to human readable values and vice versa via message configuration files
 * automatically check for updates of daemon and configuration files
 * pick preferred language for translatable message configuration parts
 * grab all messages on the eBUS and provide decoding hints
 * log messages and problems to a log file
 * capture messages or sent/received bytes to a log file as text
 * dump received bytes to binary files for later playback/analysis
 * listen for [command line client](3.1.-TCP-client-commands) connections on a dedicated TCP port
 * provide a rudimentary HTML interface
 * format messages and data in [JSON on dedicated HTTP port](https://github.com/john30/ebusd/wiki/3.2.-HTTP-client)
 * publish received data to [MQTT topics](https://github.com/john30/ebusd/wiki/3.3.-MQTT-client) and vice versa (if authorized)
 * announce [message definitions and status by MQTT](https://github.com/john30/ebusd/wiki/MQTT-integration) to e.g. integrate with [Home Assistant](https://www.home-assistant.io/) using [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/)
 * support MQTT publish to [Azure IoT hub](https://docs.microsoft.com/en-us/azure/iot-hub/) (see [MQTT integration](https://github.com/john30/ebusd/wiki/MQTT-integration))
 * publish message data to KNX groups and answer to read requests from KNX
 * [user authentication](https://github.com/john30/ebusd/wiki/3.1.-TCP-client-commands#auth) via [ACL file](https://github.com/john30/ebusd/wiki/2.-Run#daemon-options) for access control to certain messages


Installation
------------

Either pick the [latest release package](https://github.com/john30/ebusd/releases/latest) suitable for your system,
use the Debian repository as [described here](https://github.com/john30/ebusd-debian/blob/master/README.md),
use `makepkg` for [Archlinux](https://github.com/john30/ebusd/tree/master/contrib/archlinux)
or pick the [package from the Alpine Linux repository](https://pkgs.alpinelinux.org/package/edge/community/x86/ebusd),
build it yourself, or use a docker image (see below).

Building ebusd from the source requires the following packages and/or features:
 * autoconf (>=2.63) + automake (>=1.11) or cmake
 * g++ with C++11 support (>=4.8.1)
 * make
 * kernel with pselect or ppoll support
 * glibc with argp support or argp-standalone
 * libmosquitto-dev for MQTT support
 * libssl-dev for SSL support

To start the build process, run these commands:  
> ./autogen.sh  
> make install-strip  

Or alternatively with cmake:  
> cmake .  
> make install/strip  

Documentation
-------------

Usage instructions and further information can be found here:
> https://github.com/john30/ebusd/wiki


Configuration
-------------

The most important part of each ebusd installation is the message configuration.
Starting with version 3.2, **ebusd by default uses the config web service at ebusd.eu to retrieve
the latest configuration files** that are reflected by the configuration repository (follow the "latest" symlink there):
> https://github.com/john30/ebusd-configuration


Docker image
------------

A multi-architecture Docker image using the config web service for retrieving the latest message configuration files is  available on the hub.
You can use it like this:  
> docker pull john30/ebusd  
> docker run -it --rm --device=/dev/ttyUSB0 -p 8888 john30/ebusd

For more details, see [Docker Readme](https://github.com/john30/ebusd/blob/master/contrib/docker/README.md).


Contact
-------
For bugs and missing features use github issue system.

The author can be contacted at ebusd@ebusd.eu .

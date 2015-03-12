ebusd - eBUS daemon
===================

ebusd is a daemon for handling communication with eBUS devices connected to a
2-wire bus system ("energy bus" used by numerous heating systems).


Features
--------

The main features of the daemon are:

 * actively send messages to and receive answers from the eBUS
 * passively listen to messages sent on the eBUS
 * regularly poll for messages
 * scan for bus participants
 * cache all data
 * log messages and problems to a log file
 * dump sent/received bytes to the log file
 * dump received bytes to binary files for later playback/analysis
 * listen for client connections on a dedicated TCP port


Installation
------------

Building ebusd from the source requires the following packages:
 * autoconf (>=2.63)
 * automake (>=1.11)
 * g++
 * make

To start the build process, run these commands:
./autogen.sh
make install


Documentation
-------------

Usage instructions and further information can be found here:
https://github.com/john30/ebusd/wiki


Contact
-------
For bugs and missing features use github issue system.

The author can be contacted at ebusd@johnm.de .

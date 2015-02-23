# next version (2015-02-?)

## Bug fixes
* the master number calculation was incorrect (no influence on any behaviour)

## Features
* new command line option "--dumpconfig" for dumping message configurations
* new "find" command option "-f" for retriving message configurations
* new "find" command option "-i PB" to filter messages on primary command byte
* extended "read" command "FIELD" argument with optional ".N" for retrieving the N'th field with that name
* new "write" command option "-c" for compatibility with "read" and "find"

## Changed files
https://github.com/john30/ebusd/compare/v1.0.0...master


# 1.0.0 (2015-02-18)

## Breaking Changes
This is the first version of the completely reworked ebusd since October 2014.

## Bug fixes
A lot of problems were fixed, such as memory leaks and segmentation faults.

## Features
The daemon is now fully aware of the eBUS protocol and future versions will also support addressing a running daemon as answering master/slave.
The main new feature is a completely rewritten configuration file engine, which allows the use of templates and defaults and thus simplifies the configuration files a lot.
On the client side, a lot of commands have been added, such as "listen" (automatically send changed values listened to or polled), "find", and "state".

## Changed files
https://github.com/john30/ebusd/commits/master

Thanks to Roland Jax and other authors for all their work in previous versions!

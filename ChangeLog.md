# next version (2015-03-?)

## Bug fixes
* return passive messages returned in "read" regardless of their age
* corrected divisor for chains of derived number types
* corrected master number calculation (no influence on any behaviour)
* fixed signed char warning on some compilers
* corrected minimum lock count according to spec.
* differentiate between valid empty result and not found field name
* fixed scanning for messages in subdirectories for certain file systems (thanks to joltcoke)
* allow mixing references to base types and templates in message definition
* renamed "class" to "circuit" in commands and message definition

## Features
* new command line option "--dumpconfig" for dumping message configurations
* new "find" command option "-f" for retrieving message configurations in CSV format
* new "find" command option "-i PB" to filter messages on primary command byte
* extended "read" command "FIELD" argument with optional ".N" for retrieving the N''th field with that name
* new "write" command option "-c" for compatibility with "read" and "find"
* automatically detect number of masters on the bus and add to "state" command
* support reciprocal divisor
* cache symbols instead of formatted strings. this allows retrieval of individual fields and verbose read from cache
* removed valid CRCs from log and hex "write"
* shortened manufcaturer names for "scan"
* allow specifying multiple destination addresses in message definition and defaults
* added Arch Linux PKGBUILD scripts (thanks to cogano)
* new "read" command option "-d ZZ" for overriding destination address (e.g. for manual scan)
* allow message definitions without any field
* added broadcast ident message (requests all masters to send their identification) and added read ident for manual scan
* added support for AUTO-SYN generation (experimental!)
* added "grab" command for collecting and reporting seen unknown messages

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

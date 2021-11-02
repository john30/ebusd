# 21.3 (tbd)

## Bug Fixes
* fix for escaping double quote in CSV format
* adjusted helper shell scripts and Munin plugin to newer netcat
* fix for weekday in BDA data type (for sending only)
* fix some compiler warnings
* fix non-unique message keys in HTTP JSON output with "full" query parameter
* fix for message after debian install

## Features
* added DTM and BDZ data types
* added "-n" argument to "hex" and "direct" commands for automatically determining message length from input
* added level/pollprio/condition to HTTP JSON output
* added message dump from commandline in JSON format
* added support for newer MQTT broker versions
* added some PIC calibration data to "ebuspicloader" verbose output
* added support for upcoming adapter 3 firmware enhancements
* added config override path

## Breaking Changes
* remove support for Debian 8 Jessie in docker


# 21.2 (2021-02-08)

## Bug Fixes
* fix for UDP device connection issue
* fix maxage check in HTTP/JSON port

## Features
* changed docker image to multi-architecture including Raspberry Pi, reduced image size
* added trailing wildcard to "ebuspicloader" port
* added i386 and arm64 architectures to docker image
* added more startup logging
* added colon as additional separator for "--log" option


# 21.1 (2021-01-10)

## Bug Fixes
* fix for publishing several MQTT updates at once
* optimized keep alive option for net devices
* fix for duplicate entry "minarbitrationmicros" in HTTP GET
* fix for extra send retry
* fix for newer compiler versions
* fix for potential illegal string usages
* fix for named net device not being resolvable during startup

## Features
* added Raspberry Pi docker image
* added support for Cygwin build
* added option to use "\*" as trailing wildcard for circuit and name in MQTT /list topic
* added "--mqttinsecure" option
* updated to newest version of enhanced protocol
* added adapter 3 PIC tool "ebuspicloader" for uploading new firmware and configuring fix IP address


# 3.4 (2019-10-27)

## Bug Fixes
* fix for always enabled "--mqttchanges" option
* fix for dynamically adjusted poll priority
* fix for enhanced escape chars
* fix for too frequent logging on MQTT broker communication error
* fix for unexpected "different version available" message during update check
* fix for detecting subsequent fields with less than 8 bits not fitting into 1 byte
* fix for compilation and running on FreeBSD and MacOS with low latency setting for FTDI device (thanks to samm-git)
* fix for switching to daemon mode when log file was not opened
* fix for checking required arguments of "write" command with "-h"
* fix for potential MQTT reconnect issue after lost connection

## Features
* added option to set poll priority with MQTT get topic
* added "direct" command for listening to all valid messages from the bus
* added MQTT /list topic for retrieval of all known messages
* added support for init scripts on non-LSB distributions (thanks to andr2000)
* added support for logging to syslog instead of file (thanks to samm-git)
* added adjustable verbosity and option to include unknown messages to "listen" command


# 3.3 (2018-12-26)

## Bug Fixes
* fix for missing MQTT subscription after broker reconnect
* fix for answering to first scan only in answer mode
* fix dont add transfer latency to receive timeout when acting as SYN generator
* fix for bit combinations during write to SymbolString
* fix for MQTT handling after broker disconnect
* fix for NaN in JSON
* fix for deadlock in libmosquitto

## Features
* wait for being online before starting and automatically restart after 30 seconds
* added "--mqttchanges" option to only publish changed messages and changed to publish all messages by default
* added "--mqttclientid" to set own client ID instead of using the default
* added support for single quotes to all commands
* added "--mqttlog" and "--mqttversion" options

## Breaking Changes
* added support for enhanced network protocol mode for recent [ebusd-esp firmware](https://github.com/john30/ebusd-esp/) that allows the arbitration to be done directly by the Wemos


# 3.2 (2018-05-10)

## Breaking Changes
* changed default configuration file location to config web service at ebusd.eu

## Bug Fixes
* corrected tag name set by make_debian
* corrected MQTT /get topic on field level
* corrected weird IP address logging
* corrected logrotate script

## Features
* added support for retrieving configuration files from config web service
* changed docker image to be smaller size and fixed missing library dependencies
* added automatic reconnect to MQTT broker
* changed logging for messages from "updated" to "received" and "sent" depending on who initiated the request and added message direction/scan/poll/update info
* added support for serving .csv files and new argument maxage to HTTP/JSON port
* added timeout argument and error result code to ebusctl
* added "-def" option to "read" and "write" commands and "define" command to test message defintions and add new definitions during runtime and disabled all by default (use "--enabledefine" command line option).
* added "encode" and "decode" commands for testing field definitions
* added option to ignore failed host name resolution during initialization of MQTT
* added logging when device became invalid and close it on error


# 3.1 (2017-12-26)

## Bug Fixes
* corrected wrong defaults directory in Debian Jessie build
* corrected dump of chained messages
* corrected missing default MQTT topic
* corrected dump of value list and constant fields
* fix for input string vanishing on certain compiler versions in "write" command
* fix for potential endless waits
* corrected initial dumping and raw data logging
* corrected address check with "-s" or "-d" argument in "read" and "write" commands
* fixed timeout for update check
* corrected wrong initial scan load message
* corrected "read" command with field name and MQTT output by field when master and slave part both carry fields

## Features
* added helper script for reading all Vaillant registers for a single slave via "hex" command
* check all message definitions independent of any condition when checking/dumping configuration
* added option to disable automatic update check
* added scan config mode to start log entry
* added measurement and logging of min/max send-receive latency and include values in "info" command and JSON output
* drop initial potential garbage for UDP/TCP devices
* better support for cygwin builds
* added measurement of arbitration delay
* continue trying to get slave ID in scan config mode in case of communication errors


# 3.0 (2017-08-29)

## Breaking Changes
* switched to C++11 compiler
* separated access level from circuit name
* introduced access control list and user authentication for access to certain messages
* added automatic check for updates (ebusd and configuration)
* use CSV header line for determining columns and picking multi-language columns by language preference
* a circuit name is now required for all message configurations
* raw logging allows logging of messages or each sent/received byte defaulting to messages
* added systemd unit script

## Bug Fixes
* corrected numeric condition formatting
* corrected reporting of CSV error position
* corrected last update time of write messages
* close log file on SIGHUP for better logrotate support
* only allow a single scan at the same time
* check allowed value range for base types during decoding
* ignore first seconds of data when calculating symbols per second
* exclude messages without name
* fix for potentially zero resolution
* fix for ebusd not answering scan request from another participant
* corrected missing update notification on master message part
* corrected grouping of JSON output by circuit
* corrected address conflict detection when in answer mode
* fixed potential memory leaks and misused iterator
* corrected duplicates when using "--dumpconfig"
* corrected unclean shutdown
* also use transfer latency for SYN generator
* fix for timeout message when decoding scan result failed
* corrected flushing in dump and raw files
* corrected unique keys in JSON port
* fix for potential invalid access during configuration reload

## Features
* added support for MQTT handling via libmosquitto (will be compiled in when library is available)
* allow empty ID in filename for circuit and suffix extraction
* use scan ID as fallback for default circuit name
* added U3N/U3R/S3N/S3R data types
* added another log facility "other"
* added support for using cmake in addition to autoconf
* added Docker support
* allow invalid SW and HW fields and remove all but alpha-numeric chars and underscore from ident when looking for scan config file
* added "auth" command and user/access information to "info" command output
* added user and secret arguments to HTTP/JSON port and added options to retrieve message definition and raw messages
* use ACL in "read"/"write"/"find"/"scan" commands
* added -a and -l options to "find" command
* added possibility to define different log level per area on command line and with "log" command
* added decode option to "grab" command presenting decoding hints for unknown messages
* added option to use different source address QQ to "read"/"write"/"hex" commands
* enhanced scan for individual slave
* wait for broadcast scan answers before doing individual scans
* enhanced SymbolString to be aware of master/slave and get rid of CRC (instead calculate while sending/receiving)
* allow TTQ/TTH types to use less than 8 bits
* added support for optional user-defined columns to config files
* added circuit-level attributes to config files for use in JSON
* added -N option to "read" command
* added "--inject" option to inject remaining arguments as already seen messages
* use scans initiated by other participants and allow "--readonly" with "--scanconfig[=none]"
* set the serial device to low latency mode to fix huge latency found on recent kernel versions


# 2.4 (2016-12-17)

## Bug Fixes
* fixed removed last byte in "hex" command answers
* corrected checking duplicates of chained messages
* fixed reading beyond message bounds
* corrected output precision with reciprocal divisor
* corrected caching of master-master messages
* corrected default dump file name to "/tmp/ebusd_dump.bin"

## Features
* extended default definition in CSV to allow message name and comment prefix/suffix
* added automatic reconnect of device if signal loss is persistent and extended "info" result with number of reconnects
* extended "grab" command to allow selection of all or only unknown messages in grab result
* added error log entry when ebusd address conflicts with another participant
* extended BTI/HTI/BTM types to allow replacement value
* added condition name to logging of "!load" instructions and "info" result
* added "--lograwdatafile" and "--lograwdatasize" options logging received/sent bytes to a dedicated text file
* added TTQ base type
* added support for constant value fields


# 2.3 (2016-11-06)

## Bug Fixes
* synchronously request a message if needed for loading "*.inc" files
* corrected derivation for TEM_P type
* allow chained messages to be polled
* removed CRC from slave answer in decode

## Features
* no longer poll scan messages since those are expected not to change
* added more message counts to "info" result
* added uih circuit with bar chart containing yields to HTML interface


# 2.2 (2016-10-16)

## Breaking Changes
* completely reworked base data types to allow user-defined types
* added first version of TEM specific data type
* allow inline rename of template reference
* removed "stop" command for enhanced security
* use broadcast ID messages for identification and automatic configuration file selection

## Bug Fixes
* corrected extra separator in CSV style dump
* corrected BCD representation in JSON
* corrected serial close()
* fix for storing own prepared master data
* no longer show messages that did not receive data in "find" command
* check address argument for validity in "scan" command
* only use cached data if no input is necessary in "read" command
* fix for poll "continue" errors
* fix for checking command line arguments
* fix for DAY data type
* corrected checking of inline conditions
* escape special characters in string data types for JSON
* corrected dumping of value lists

## Features
* allow running multiple ebusd instances by configuration file (/etc/default/ebusd on debian)
* set keepalive option on network device
* added hex option to "find" command
* added optional initial active scan address to "--scanconfig" option that allows initiating a single destination address scan, a full scan, or sending a broadcast ident message (this is now the default)
* added MIN datatype
* avoid output of unprintable characters in string data types
* split verbosity of "read" and "find" commands into 3 levels
* added "indexed" parameter for JSON
* allow non-exact values for truncated time types


# 2.1 (2016-05-05)

## Breaking Changes
* added "hex" command for sending arbitrary hex data, disabled it by default (use "--enablehex" command line option), and limit "write -h" command to known messages only.  
  This increases security by avoiding unintended sending of arbitrary messages.
* changed ebusd default address to 0x31 in order to avoid address conflicts with popular devices on the bus
* added "!load" instruction for CSV configuration files allowing conditional loading of single other CSV file
* added "!include" instruction for CSV configuration files in order to include other files
* added support for UDP connected devices

## Bug fixes
* require "-c" in "write" command
* corrected duplicate messages check with condition
* corrected maximum values for BCD and HCD types
* corrected weekday calculation (for smaller systems mainly)
* corrected determination of field name uniqueness
* corrected JSON read of passive messages

## Features
* allow CSV to contain empty lines (no field filled in)
* added shorter HCD and BCD data types
* added data type NTS (null terminated string)
* added EXP and EXR data types (exponential)
* added option for changing PID file
* allow using newlines within quoted fields in CSV
* added support for string based conditions and on-the-fly condition definition
* enhanced HTTP user interface
* increased default receive timeout to 25ms
* extract default circuit name from CSV file name
* automatically start grabbing all messages
* add circuit+message name of known messages to grab result
* added buffer for network devices and flush potentially buffered input when connecting
* added option for setting transfer latency and use default 10 ms for network device


# 2.0 (2016-01-06)

## Breaking Changes
* automatic configuration file selection by querying device identification ("--scanconfig").  
  Previous configuration files are still usable, but only without the "--scanconfig" command line parameter to ebusd (unless the files are renamed).  
  This is now enabled by default in the init.d (or systemd) scripts to work with ebusd-2.x.x configuration files.
* support for chained messages with fields covering more than one message ID

## Bug fixes
* avoid multiple identical derived messages
* return "empty" for queries without result (e.g. grab result)
* fix for stopping main loop in rare cases
* fix for extracting default circuit name from CSV file name
* fix for using pselect
* fix for finishing active request when own master address reception timed out
* fix for some memory issues
* fix for closing serial device that is no tty
* fix for dumping message definition
* fix for cache invalidation
* fixes for answer mode

## Features
* respond to broadcast "write" command with "done broadcast"
* prepared for full file check with overlapping names and message keys
* added replacement value for date/time types
* extended "info" command with seen addresses, scan state, and loaded CSV file
* improved CSV file check
* insert index suffix extracted from CSV file name when applying defaults (format "ZZ.CCCCC[.index].csv")
* added support for MACH architecture (Mac OS)
* use empty condition messagename for referencing scan message
* allow read and write message definitions having the same ID
* add hint to decoding problems in "read" and "write" if answer was retrieved successfully
* added script for converting "grab result" to CSV
* exclusively lock serial device to prevent simultaneous access by another process
* extended "-i" parameter of "find" command to accept further ID parts
* include decode problems instead of returning invalid JSON
* extended allowed message ID length and max STR and HEX length
* extended "grab" command with "all" option for grabbing all messages instead of only unknown
* allow float values being used for int types
* added big endian data type variants (UIR, SIR, FLR, ULR, SLR)
* use numeric value as fallback for value lists with unknown value association
* increased verbosity during scan
* increased default bus acquisition retries from 2 to 3
* use named objects for JSON fields instead of array with fallback to numbered
* exclude conditions from "find -f" command
* let ebusd answer to scan request (if started with "--answer")
* add special length '*' for consuming remaining input for some string types (IGN, STR, HEX)

## Changed files
https://github.com/john30/ebusd/compare/v1.3.0...v2.0


# 1.3.0 (2015-10-24)

## Breaking Changes
* started with a new git repository for working on 2.0 version (previous repository was renamed to ebusd1)
* added support for conditional message definitions based on other message values

## Bug fixes
* fix for numeric parsing of empty string
* allow HTTP port query string keys without value
* fix for thread cleanup
* update last message update time when actively sending it
* trim CSV lines and fields
* corrected derivation of field name, comment and unit in templates
* check invalid divisor/values for string and value list based types
* fix for overriding destination address in "read" command
* fix for cached master data

## Features
* new "read" option "-n" for retrieving name/value pairs in numeric form
* new "find" option "-e" for exactly matching name and optional circuit (ignoring case)
* new "find" option "-F" for listing messages in CSV format with selected columns only
* added numeric, verbose, and required options to JSON and use "null" for unset or replacement value
* allow static ".json" files being served by HTTP port
* allow passing raw value when writing name/value pairs
* enhanced static HTML user interface to dynamic
* added possibility to use a different name for referenced field template
* added data type HCD (for Ochsner)
* added base type TTH (truncated time with 30 minutes resolution)
* added base type VTM (reverse HTM)
* added log entry for scan completion
* automatically invalidate cached data of messages with same name+circuit (ignoring level)
* added "info" command
* added circuit and name to verbose "read" output
* added option to pass additional input data to "read" command
* added messages without dedicated destination and cached data decoding error to "find" result
* new "write" option "-d" for overriding destination address
* allow CSV file name to define default destination address and circuit name (format "ZZ.CCCCC.csv") and if present, concat default circuit name and security suffix (circuit name in CSV starting with "#")

## Changed files
https://github.com/john30/ebusd/compare/v1.2.0...v1.3.0


# 1.2.0 (2015-05-25)

## Bug fixes
* corrected missing newline in ebusctl
* avoid updating last message change time when only the querying master was modified
* notify running requests on device error

## Features
* new command line option "--readonly" for read-only access to the bus
* added support for retrieving data as JSON and additional files via optional HTTP port
* added first version of static HTML user interface (to be extended to a dynamic one later)
* new "read" option "-h" for sending hex read messages
* new "read" option "-p PRIO" for setting the poll priority of a message
* update cache on hex read or write

## Changed files
https://github.com/john30/ebusd/compare/v1.1.0...v1.2.0


# 1.1.0 (2015-03-28)

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
* fixed reading cached values
* extended max allowed data length to 25 (spec says 16)

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
* shortened manufacturer names for "scan"
* allow specifying multiple destination addresses in message definition and defaults
* added Arch Linux PKGBUILD scripts (thanks to cogano)
* new "read" command option "-d ZZ" for overriding destination address (e.g. for manual scan)
* allow message definitions without any field
* added broadcast ident message (requests all masters to send their identification) and added read ident for manual scan
* added support for AUTO-SYN generation (experimental!)
* added "grab" command for collecting and reporting seen unknown messages

## Changed files
https://github.com/john30/ebusd/compare/v1.0.0...v1.1.0


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

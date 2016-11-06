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
* require "-c" in write command
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
* fix for overriding destination address in read command
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

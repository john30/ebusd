/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EBUSD_MAIN_H_
#define EBUSD_MAIN_H_

#include <stdint.h>
#include <string>
#include "data.h"
#include "message.h"
#include "result.h"

/** \file main.h */

namespace ebusd {

/** A structure holding all program options. */
struct options {
  const char* device;  //!< eBUS device (serial device or [udp:]ip:port) [/dev/ttyUSB0]
  bool noDeviceCheck;  //!< skip serial eBUS device test
  bool readOnly;  //!< read-only access to the device
  bool initialSend;  //!< send an initial escape symbol after connecting device
  int latency;  //!< transfer latency in us [0 for USB, 10000 for IP]

  const char* configPath;  //!< path to CSV configuration files [/etc/ebusd]
  bool scanConfig;  //!< pick configuration files matching initial scan
  /** the initial address to scan for scanconfig
   * (@a ESC=none, 0xfe=broadcast ident, @a SYN=full scan, else: single slave address). */
  unsigned char initialScan;
  int checkConfig;  //!< check CSV config files ( != 0) and optionally dump (2), then stop
  int pollInterval;  //!< poll interval in seconds, 0 to disable [5]

  unsigned char address;  //!< own bus address [31]
  bool answer;  //!< answer to requests from other masters
  int acquireTimeout;  //!< bus acquisition timeout in us [9400]
  int acquireRetries;  //!< number of retries for bus acquisition [3]
  int sendRetries;  //!< number of retries for failed sends [2]
  int receiveTimeout;  //!< timeout for receiving answer from slave in us [25000]
  int masterCount;  //!< expected number of masters for arbitration [0]
  bool generateSyn;  //!< enable AUTO-SYN symbol generation

  bool foreground;  //!< run in foreground
  bool enableHex;  //!< enable hex command
  const char* pidFile;  //!< PID file name [/var/run/ebusd.pid]
  uint16_t port;  //!< port to listen for command line connections [8888]
  bool localOnly;  //!< listen on 127.0.0.1 interface only
  uint16_t httpPort;  //!< optional port to listen for HTTP connections, 0 to disable [0]
  const char* htmlPath;  //!< path for HTML files served by the HTTP port [/var/ebusd/html]

  const char* logFile;  //!< log file name [/var/log/ebusd.log]

  bool logRaw;  //!< raw log each received/sent byte on the bus
  const char* logRawFile;  //!< name of raw log file [/var/log/ebusd.log]
  unsigned int logRawSize;  //!< maximum size of raw log file in kB [100]

  bool dump;  //!< binary dump received bytes
  const char* dumpFile;  //!< name of dump file [/tmp/ebusd_dump.bin]
  unsigned int dumpSize;  //!< maximum size of dump file in kB [100]
};

/**
 * Get the @a DataFieldTemplates for the specified configuration file.
 * @param filename the full name of the configuration file.
 * @return the @a DataFieldTemplates.
 */
DataFieldTemplates* getTemplates(const string filename);

/**
 * Load the message definitions from configuration files.
 * @param messages the @a MessageMap to load the messages into.
 * @param verbose whether to verbosely log problems.
 * @param denyRecursive whether to avoid loading all files recursively (e.g. for scan config check).
 * @return the result code.
 */
result_t loadConfigFiles(MessageMap* messages, bool verbose = false, bool denyRecursive = false);

/**
 * Load the message definitions from a configuration file matching the scan result.
 * @param messages the @a MessageMap to load the messages into.
 * @param address the address of the scan participant
 * (either master for broadcast master data or slave for read slave data).
 * @param data the scan @a SymbolString for which to load the configuration file.
 * @param relativeFile the string in which the name of the configuration file is stored on success.
 * @param verbose whether to verbosely log problems.
 * @return the result code.
 */
result_t loadScanConfigFile(MessageMap* messages, unsigned char address, SymbolString& data, string& relativeFile,
    bool verbose = false);

}  // namespace ebusd

#endif  // EBUSD_MAIN_H_

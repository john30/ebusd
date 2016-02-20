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

#ifndef MAIN_H_
#define MAIN_H_

#include "result.h"
#include "data.h"
#include "message.h"
#include <stdint.h>

/** \file main.h */

/** A structure holding all program options. */
struct options
{
	const char* device; //!< eBUS device (serial device or ip:port) [/dev/ttyUSB0]
	bool noDeviceCheck; //!< skip serial eBUS device test
	bool readonly; //!< read-only access to the device

	const char* configPath; //!< path to CSV configuration files [/etc/ebusd]
	bool scanConfig; //!< pick configuration files matching initial scan
	int checkConfig; //!< check CSV config files (!=0) and optionally dump (2), then stop
	int pollInterval; //!< poll interval in seconds, 0 to disable [5]

	unsigned char address; //!< own bus address [FF]
	bool answer; //!< answer to requests from other masters
	int acquireTimeout; //!< bus acquisition timeout in us [9400]
	int acquireRetries; //!< number of retries for bus acquisition [3]
	int sendRetries; //!< number of retries for failed sends [2]
	int receiveTimeout; //!< timeout for receiving answer from slave in us [15000]
	int masterCount; //!< expected number of masters for arbitration [5]
	bool generateSyn; //!< enable AUTO-SYN symbol generation

	bool foreground; //!< run in foreground
	const char* pidFile; //!< PID file name [/var/run/ebusd.pid]
	uint16_t port; //!< port to listen for command line connections [8888]
	bool localOnly; //!< listen on 127.0.0.1 interface only
	uint16_t httpPort; //!< optional port to listen for HTTP connections, 0 to disable [0]
	const char* htmlPath; //!< path for HTML files served by the HTTP port [/var/ebusd/html]

	const char* logFile; //!< log file name [/var/log/ebusd.log]
	bool logRaw; //!< log each received/sent byte on the bus

	bool dump; //!< dump received bytes
	const char* dumpFile; //!< dump file name [/tmp/ebus_dump.bin]
	int dumpSize; //!< maximum size of dump file in kB [100]
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
result_t loadConfigFiles(MessageMap* messages, bool verbose=false, bool denyRecursive=false);

/**
 * Load the message definitions from a configuration file matching the scan result.
 * @param messages the @a MessageMap to load the messages into.
 * @param address the address of the scan participant (either master for broadcast master data or slave for read slave data).
 * @param data the scan @a SymbolString for which to load the configuration file.
 * @param relativeFile the string in which the name of the configuration file is stored on success.
 * @return the result code.
 */
result_t loadScanConfigFile(MessageMap* messages, unsigned char address, SymbolString& data, string& relativeFile);

#endif // MAIN_H_

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <argp.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "device.h"

using namespace std;

/** A structure holding all program options. */
struct options
{
	const char* device; //!< device to write to [/dev/ttyUSB60]
	unsigned int time; //!< delay between bytes in us [10000]

	const char* dumpFile; //!< dump file to read
};

/** the program options. */
static struct options opt = {
	"/dev/ttyUSB60", // device
	10000, // time

	"/tmp/ebus_dump.bin", // dumpFile
};

/** the version string of the program. */
const char *argp_program_version = "ebusfeed of """PACKAGE_STRING"";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = ""PACKAGE_BUGREPORT"";

/** the documentation of the program. */
static const char argpdoc[] =
	"Feed data from an "PACKAGE" DUMPFILE to a serial device.\n"
	"\v"
	"With no DUMPFILE, /tmp/ebus_dump.bin is used.\n"
	"\n"
	"Example for setting up two pseudo terminals with 'socat':\n"
	"  1. 'socat -d -d pty,raw,echo=0 pty,raw,echo=0'\n"
	"  2. create symbol links to appropriate devices, e.g.\n"
	"     'ln -s /dev/pts/2 /dev/ttyUSB60'\n"
	"     'ln -s /dev/pts/3 /dev/ttyUSB20'\n"
	"  3. start "PACKAGE": '"PACKAGE" -f -d /dev/ttyUSB20 --nodevicecheck'\n"
	"  4. start ebusfeed: 'ebusfeed /path/to/ebus_dump.bin'\n";

/** the description of the accepted arguments. */
static char argpargsdoc[] = "[DUMPFILE]";

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
	{"device", 'd', "DEV",  0, "Write to DEV (serial device) [/dev/ttyUSB60]", 0 },
	{"time",   't', "USEC", 0, "Delay each byte by USEC us [10000]", 0 },

	{NULL,       0, NULL,   0, NULL, 0 },
};

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct options *opt = (struct options*)state->input;
	char* strEnd = NULL;
	switch (key) {
	// Device settings:
	case 'd': // --device=/dev/ttyUSB60
		if (arg == NULL || arg[0] == 0) {
			argp_error(state, "invalid device");
			return EINVAL;
		}
		opt->device = arg;
		break;
	case 't': // --time=10000
		opt->time = (unsigned int)strtoul(arg, &strEnd, 10);
		if (strEnd == NULL || strEnd == arg || *strEnd != 0 || opt->time < 1000 || opt->time > 100000000) {
			argp_error(state, "invalid time");
			return EINVAL;
		}
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num == 0) {
			if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
				argp_error(state, "invalid dumpfile");
				return EINVAL;
			}
			opt->dumpFile = arg;
		} else
			return ARGP_ERR_UNKNOWN;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}


int main(int argc, char* argv[]) {
	struct argp argp = { argpoptions, parse_opt, argpargsdoc, argpdoc, NULL, NULL, NULL };
	setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &opt) != 0) {
		return EINVAL;
	}
	Device* device = Device::create(opt.device, false, false, NULL);
	if (device == NULL) {
		cout << "unable to create device " << opt.device << endl;
		return EINVAL;
	}
	result_t result = device->open();
	if (result != RESULT_OK) {
		cout << "unable to open " << opt.device << ": " << getResultCode(result) << endl;
	}
	if (!device->isValid()) {
		cout << "device " << opt.device << " not available" << endl;
	} else {
		cout << "device opened" << endl;
		fstream file(opt.dumpFile, ios::in | ios::binary);
		if (file.is_open()) {
			while (true) {
				unsigned char byte = (unsigned char)file.get();
				if (file.eof()) {
					break;
				}
				cout << hex << setw(2) << setfill('0')
				     << static_cast<unsigned>(byte) << endl;
				device->send(byte);
				usleep(opt.time);
			}
			file.close();
		} else {
			cout << "error opening file " << opt.dumpFile << endl;
		}
	}

	delete device;
	exit(EXIT_SUCCESS);
}


/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2023 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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
#  include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "lib/ebus/device.h"
#include "lib/ebus/result.h"
#include "lib/utils/arg.h"

namespace ebusd {

using std::hex;
using std::fstream;
using std::cout;
using std::endl;
using std::setw;
using std::setfill;
using std::ios;
using ebusd::result_t;
using ebusd::Device;

/** A structure holding all program options. */
struct options {
  const char* device;  //!< device to write to [/dev/ttyACM60]
  unsigned int time;  //!< delay between bytes in us [10000]

  const char* dumpFile;  //!< dump file to read
};

/** the program options. */
static struct options opt = {
  "/dev/ttyACM60",  // device
  10000,  // time

  "/tmp/ebus_dump.bin",  // dumpFile
};

/** the definition of the known program arguments. */
static const ebusd::argDef argDefs[] = {
  {"device", 'd', "DEV",     0, "Write to DEV (serial device) [/dev/ttyACM60]"},
  {"time",   't', "USEC",    0, "Delay each byte by USEC us [10000]"},
  {nullptr, 0x100, "DUMPFILE", af_optional, "Dump file to read [/tmp/ebus_dump.bin]"},

  {nullptr,    0, nullptr,   0, nullptr},
};

/**
 * The program argument parsing function.
 * @param key the key from @a argDefs.
 * @param arg the option argument, or nullptr.
 * @param parseOpt the parse options.
 */
static int parse_opt(int key, char *arg, const ebusd::argParseOpt *parseOpt, struct options *opt) {
  char* strEnd = nullptr;
  switch (key) {
  // Device settings:
  case 'd':  // --device=/dev/ttyACM60
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid device");
      return EINVAL;
    }
    opt->device = arg;
    break;
  case 't':  // --time=10000
    opt->time = (unsigned int)strtoul(arg, &strEnd, 10);
    if (strEnd == nullptr || strEnd == arg || *strEnd != 0 || opt->time < 1000 || opt->time > 100000000) {
      argParseError(parseOpt, "invalid time");
      return EINVAL;
    }
    break;
  case 0x100:  // DUMPFILE
    if (arg[0] == 0 || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid dumpfile");
      return EINVAL;
    }
    opt->dumpFile = arg;
    break;

  default:
    return ESRCH;
  }
  return 0;
}


/**
 * Main function.
 * @param argc the number of command line arguments.
 * @param argv the command line arguments.
 * @return the exit code.
 */
int main(int argc, char* argv[]) {
  ebusd::argParseOpt parseOpt = {
    argDefs,
    reinterpret_cast<parse_function_t>(parse_opt),
    af_noVersion,
    "Feed data from an " PACKAGE " DUMPFILE to a serial device.",
    "Example for setting up two pseudo terminals with 'socat':\n"
    "  1. 'socat -d -d pty,raw,echo=0 pty,raw,echo=0'\n"
    "  2. create symbol links to appropriate devices, e.g.\n"
    "     'ln -s /dev/pts/2 /dev/ttyACM60'\n"
    "     'ln -s /dev/pts/3 /dev/ttyACM20'\n"
    "  3. start " PACKAGE ": '" PACKAGE " -f -d /dev/ttyACM20 --nodevicecheck'\n"
    "  4. start ebusfeed: 'ebusfeed /path/to/ebus_dump.bin'",
    nullptr,
  };
  switch (ebusd::argParse(&parseOpt, argc, argv, &opt)) {
    case 0:  // OK
      break;
    case '?':  // help printed
      return 0;
    default:
      return EINVAL;
  }

  Device* device = Device::create(opt.device, 0, false);
  if (device == nullptr) {
    cout << "unable to create device " << opt.device << endl;
    return EINVAL;
  }
  result_t result = device->open();
  if (result != ebusd::RESULT_OK) {
    cout << "unable to open " << opt.device << ": " << getResultCode(result) << endl;
  }
  if (!device->isValid()) {
    cout << "device " << opt.device << " not available" << endl;
  } else {
    cout << "device opened" << endl;
    fstream file(opt.dumpFile, ios::in | ios::binary);
    if (file.is_open()) {
      while (true) {
        symbol_t byte = (symbol_t)file.get();
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

}  // namespace ebusd

int main(int argc, char* argv[]) {
  return ebusd::main(argc, argv);
}

/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
 */

#include "libebus.h"
#include "appl.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>

using namespace libebus;

Appl& A = Appl::Instance();

void define_args()
{
	A.addItem("p_device", Appl::Param("/dev/ttyUSB60"), "d", "device",
		  "dummy serial device (default: /dev/ttyUSB60)\n\t\t(socat -d -d pty,raw,echo=0 pty,raw,echo=0)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_file", Appl::Param("test/ebus_dump.bin"), "f", "file",
		  "dump file with raw data (default: test/ebus_dump.bin)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_time", Appl::Param(10000), "t", "time",
		  "wait time  [ms] (default: 10000)",
		  Appl::type_long, Appl::opt_mandatory);

	A.addItem("p_help", Appl::Param(false), "h", "help",
		  "print this message",
		  Appl::type_bool, Appl::opt_none);
}

int main(int argc, char* argv[])
{
	// define Arguments and Application variables
	define_args();

	// parse Arguments
	if (A.parseArgs(argc, argv) == false) {
		A.printArgs();
		exit(EXIT_FAILURE);
	}

	// print Help
	if (A.getParam<bool>("p_help") == true) {
		A.printArgs();
		exit(EXIT_SUCCESS);
	}

	std::string dev(A.getParam<const char*>("p_device"));
	Port port(dev, true);

	port.open();
	if(port.isOpen() == true)
		std::cout << "openPort successful." << std::endl;

	std::fstream file(A.getParam<const char*>("p_file"), std::ios::in | std::ios::binary);

	if(file.is_open() == true) {

		while (file.eof() == false) {
			unsigned char byte = file.get();
			std::cout << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned>(byte) << std::endl;

			port.send(&byte, 1);
			usleep(A.getParam<long>("p_time"));
		}

		file.close();
	}

	port.close();
	if(port.isOpen() == false)
		std::cout << "closePort successful." << std::endl;

	return 0;

}

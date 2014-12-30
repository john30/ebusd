/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appl.h"
#include "port.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>

using namespace std;

Appl& A = Appl::Instance("/path/dumpfile");

void define_args()
{
	A.setVersion("ebusfeed is part of """PACKAGE_STRING"");

	A.addText(" 'ebusfeed' sends hex values from dump file to a pseudo terminal device (pty)\n\n"
		  "   Usage: 1. 'socat -d -d pty,raw,echo=0 pty,raw,echo=0'\n"
		  "          2. create symbol links to appropriate devices\n"
		  "             for example: 'ln -s /dev/pts/2 /dev/ttyUSB60'\n"
		  "                          'ln -s /dev/pts/3 /dev/ttyUSB20'\n"
		  "          3. start ebusd: 'ebusd -f -d /dev/ttyUSB20'\n"
		  "          4. start ebusfeed: 'ebusfeed /path/to/ebus_dump.bin'\n\n"
		  "Options:\n");

	A.addOption("device", "d", OptVal("/dev/ttyUSB60"), dt_string, ot_mandatory,
		    "link on pseudo terminal device (/dev/ttyUSB60)");

	A.addOption("time", "t", OptVal(10000), dt_long, ot_mandatory,
		    "delay between 2 bytes in 'us' (10000)");

}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	if (A.parseArgs(argc, argv) == false)
		return EXIT_SUCCESS;

	if (A.missingCommand() == true) {
		cout << "ebus dump file is required." << endl;
		exit(EXIT_FAILURE);
	}

	string dev(A.getOptVal<const char*>("device"));
	Port port(dev, true, false, NULL, false, "", 1);

	port.open();
	if(port.isOpen() == true) {
		cout << "openPort successful." << endl;

		fstream file(A.getCommand().c_str(), ios::in | ios::binary);

		if (file.is_open() == true) {

			while (file.eof() == false) {
				unsigned char byte = file.get();
				cout << hex << setw(2) << setfill('0')
				     << static_cast<unsigned>(byte) << endl;

				port.send(byte);
				usleep(A.getOptVal<long>("time"));
			}

			file.close();
		}
		else
			cout << "error opening file " << A.getOptVal<const char*>("file") << endl;

		port.close();
		if(port.isOpen() == false)
			cout << "closePort successful." << endl;
	}
	else
		cout << "error opening device " << A.getOptVal<const char*>("device") << endl;


	exit(EXIT_SUCCESS);
}


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

#include "port.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main ()
{
	string dev("/dev/ttyUSB20");
	Port port(dev, true, false, NULL, false, "", 1);

	port.open();

	if(port.isOpen() == true)
		cout << "openPort successful." << endl;

	int count = 0;

	while (1) {
		result_t result;
		unsigned char byte = 0;
		result = port.recv(0, byte);

		if (result == RESULT_OK)
			cout << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(byte) << endl;

		count++;
	}

	port.close();

	if(port.isOpen() == false)
		cout << "closePort successful." << endl;

	return 0;

}

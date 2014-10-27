/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of libebus.
 *
 * libebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libebus. If not, see http://www.gnu.org/licenses/.
 */

#include "port.h"
#include <iostream>
#include <iomanip>

using namespace libebus;

int main ()
{
	std::string dev("/dev/ttyUSB20");
	Port port(dev, true);

	port.open();

	if(port.isOpen() == true)
		std::cout << "openPort successful." << std::endl;

	int count = 0;

	while (1) {
		ssize_t bytes_read;
		unsigned char byte = 0;
		bytes_read = port.recv(0);

		for (int i = 0; i < bytes_read; i++)
			byte = port.byte();
			std::cout << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned>(byte) << std::endl;

		bytes_read = 0;

		count++;
	}

	port.close();

	if(port.isOpen() == false)
		std::cout << "closePort successful." << std::endl;

	return 0;

}

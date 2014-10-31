/*
 * Copyright (C) John Baier 2014 <ebusd@johnm.de>
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

#include "symbol.h"
#include <iostream>
#include <iomanip>

using namespace libebus;

int main ()
{
	SymbolString sstr("10feb5050427a915aa");

	std::stringstream out;
	for (size_t i = 0; i<sstr.size(); i++) {
		out << std::nouppercase << std::setw(2) << std::hex
			<< std::setfill('0') << static_cast<unsigned>(sstr[i]);
	}

	std::string gotStr = out.str(), expectStr = "10feb5050427a90015a90177";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "ctor escaped successful." << std::endl;
	else
		std::cout << "ctor escaped invalid: got " << gotStr
			<< ", expected " << expectStr << std::endl;

	unsigned char gotCrc = sstr.getCRC(), expectCrc = 0x77;

	if (gotCrc == expectCrc)
		std::cout << "CRC successful." << std::endl;
	else
		std::cout << "CRC invalid: got 0x"
			<< std::nouppercase << std::setw(2) << std::hex
			<< std::setfill('0') << static_cast<unsigned>(gotCrc)
			<< ", expected 0x"
			<< std::nouppercase << std::setw(2) << std::hex
			<< std::setfill('0') << static_cast<unsigned>(expectCrc)
			<< std::endl;

	gotStr = sstr.getDataStr(true), expectStr = "10feb5050427a915aa77";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "unescape successful." << std::endl;
	else
		std::cout << "unescape invalid: got " << gotStr
			<< ", expected " << expectStr << std::endl;

	sstr = SymbolString("10feb5050427a90015a90177", true);
	gotStr = sstr.getDataStr(false);
	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "ctor unescaped successful." << std::endl;
	else
		std::cout << "ctor unescaped invalid: got " << gotStr
			<< ", expected " << expectStr << std::endl;

	return 0;

}

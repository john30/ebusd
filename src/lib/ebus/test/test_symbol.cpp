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

using namespace std;

int main ()
{
	SymbolString sstr = SymbolString("10feb5050427a915aa");

	std::string gotStr = sstr.getDataStr(false), expectStr = "10feb5050427a90015a90177";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "ctor escaped OK" << std::endl;
	else
		std::cout << "ctor escaped error: got " << gotStr << ", expected "
		        << expectStr << std::endl;

	unsigned char gotCrc = sstr.getCRC(), expectCrc = 0x77;

	if (gotCrc == expectCrc)
		std::cout << "CRC OK" << std::endl;
	else
		std::cout << "CRC error: got 0x" << std::nouppercase << std::setw(2)
		        << std::hex << std::setfill('0')
		        << static_cast<unsigned>(gotCrc) << ", expected 0x"
		        << std::nouppercase << std::setw(2) << std::hex
		        << std::setfill('0') << static_cast<unsigned>(expectCrc)
		        << std::endl;

	gotStr = sstr.getDataStr(), expectStr = "10feb5050427a915aa77";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "unescape OK" << std::endl;
	else
		std::cout << "unescape error: got " << gotStr << ", expected "
		        << expectStr << std::endl;

	sstr = SymbolString("10feb5050427a90015a90177", true);

	gotStr = sstr.getDataStr();

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		std::cout << "ctor unescaped OK" << std::endl;
	else
		std::cout << "ctor unescaped error: got " << gotStr << ", expected "
		        << expectStr << std::endl;

	return 0;

}

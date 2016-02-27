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

#include "symbol.h"
#include <iostream>
#include <iomanip>

using namespace std;

int main(int argc, char** argv)
{
	SymbolString sstr(true);

	if (argc>1) {
		result_t result = sstr.parseHex(argv[1], true);
		if (result != RESULT_OK) {
			cout << "parse escaped error: " << getResultCode(result) << endl;
		} else {
			unsigned char gotCrc = sstr.getCRC();
			cout << "calculated CRC: 0x"
					<< nouppercase << setw(2) << hex << setfill('0')
			        << static_cast<unsigned>(gotCrc) << endl;
		}
		return 0;
	}
	result_t result = sstr.parseHex("10feb5050427a915aa", false);
	if (result != RESULT_OK)
		cout << "parse escaped error: " << getResultCode(result) << endl;

	string gotStr = sstr.getDataStr(false, false), expectStr = "10feb5050427a90015a90177";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		cout << "parse escaped OK" << endl;
	else
		cout << "parse escaped error: got " << gotStr << ", expected "
		        << expectStr << endl;

	unsigned char gotCrc = sstr.getCRC(), expectCrc = 0x77;

	if (gotCrc == expectCrc)
		cout << "CRC OK" << endl;
	else
		cout << "CRC error: got 0x" << nouppercase << setw(2)
		        << hex << setfill('0')
		        << static_cast<unsigned>(gotCrc) << ", expected 0x"
		        << nouppercase << setw(2) << hex
		        << setfill('0') << static_cast<unsigned>(expectCrc)
		        << endl;

	gotStr = sstr.getDataStr(true, false), expectStr = "10feb5050427a915aa77";

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		cout << "unescape OK" << endl;
	else
		cout << "unescape error: got " << gotStr << ", expected "
		        << expectStr << endl;

	sstr = SymbolString(false);
	result = sstr.parseHex("10feb5050427a90015a90177", true);
	if (result != RESULT_OK)
		cout << "parse unescaped error: " << getResultCode(result) << endl;

	gotStr = sstr.getDataStr(true, false);

	if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
		cout << "parse unescaped OK" << endl;
	else
		cout << "parse unescaped error: got " << gotStr << ", expected "
		        << expectStr << endl;

	return 0;

}

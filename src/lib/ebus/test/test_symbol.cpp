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

#include <iostream>
#include <iomanip>
#include <string>
#include "symbol.h"

using namespace std;
using namespace ebusd;

static bool error = false;

void verify(bool expectFailMatch, string type, string input,
		bool match, string expectStr, string gotStr) {
	match = match && expectStr == gotStr;
	if (expectFailMatch) {
		if (match) {
			cout << "  failed " << type << " match >" << input
			        << "< error: unexpectedly succeeded" << endl;
			error = true;
		} else {
			cout << "  failed " << type << " match >" << input << "< OK" << endl;
		}
	} else if (match) {
		cout << "  " << type << " match >" << input << "< OK" << endl;
	} else {
		cout << "  " << type << " match >" << input << "< error: got >"
		        << gotStr << "<, expected >" << expectStr << "<" << endl;
			error = true;
	}
}

int main(int argc, char** argv) {
	SymbolString sstr(true);

	if (argc > 1) {
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
	string gotStr, expectStr;
	result_t result = sstr.parseHex("10feb5050427a915aa", false);
	if (result != RESULT_OK) {
		cout << "parse escaped error: " << getResultCode(result) << endl;
		error = true;
	} else {
		gotStr = sstr.getDataStr(false, false), expectStr = "10feb5050427a90015a90177";
		verify(false, "parse escaped", "10feb5050427a915aa", true, expectStr, gotStr);
		unsigned char gotCrc = sstr.getCRC(), expectCrc = 0x77;
		ostringstream ostr;
		ostr << nouppercase << setw(2) << hex << setfill('0') << static_cast<unsigned>(expectCrc);
		expectStr = ostr.str();
		ostr.str("");
		ostr << nouppercase << setw(2) << hex << setfill('0') << static_cast<unsigned>(gotCrc);
		gotStr = ostr.str();
		verify(false, "CRC", "10feb5050427a915aa", gotCrc == expectCrc, expectStr, gotStr);

		gotStr = sstr.getDataStr(true, false), expectStr = "10feb5050427a915aa77";
		verify(false, "unescape", "10feb5050427a915aa", true, expectStr, gotStr);
	}

	sstr = SymbolString(false);
	result = sstr.parseHex("10feb5050427a90015a90177", true);
	if (result != RESULT_OK) {
		cout << "parse unescaped error: " << getResultCode(result) << endl;
		error = true;
	} else {
		gotStr = sstr.getDataStr(true, false), expectStr = "10feb5050427a915aa77";
		verify(false, "parse unescaped", "10feb5050427a90015a90177", true, expectStr, gotStr);
	}

	return error ? 1 : 0;
}

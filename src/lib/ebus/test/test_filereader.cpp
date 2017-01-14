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
#include <vector>
#include "filereader.h"

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

int main() {
	istringstream ifs(
		"line 1 col 1,line 1 col 2,line 1 col 3\n"
		"line 2 col 1,\"line 2 col 2\",\"line 2 \"\"col 3\"\"\"\n"
		"line 4 col 1,\"line 4 col 2 part 1\n"
		"line 4 col 2 part 2\",line 4 col 3\n"
		",,,\n"
		"line 6 col 1,,line 6 col 3\n"
		"line 8 col 1,\"line 8 col 2 part 1;\n"
		"line 8 col 2 part 2\",line 8 col 3\n"
	);
	string resultlines[][3] = {
		{"line 1 col 1", "line 1 col 2", "line 1 col 3"},
		{"line 2 col 1", "line 2 col 2", "line 2 \"col 3\""},
		{"", "", ""},
		{"line 4 col 1", "line 4 col 2 part 1;line 4 col 2 part 2", "line 4 col 3"},
		{"", "", ""},
		{"line 6 col 1", "", "line 6 col 3"},
		{"", "", ""},
		{"line 8 col 1", "line 8 col 2 part 1;line 8 col 2 part 2", "line 8 col 3"},
	};
	unsigned int lineNo = 0;
	vector<string> row;

	while (FileReader::splitFields(ifs, row, lineNo)) {
		cout << "line " << static_cast<unsigned>(lineNo) << ": split OK" << endl;
		string resultline[3] = resultlines[lineNo-1];
		if (row.empty()) {
			cout << "  result empty";
			if (resultline[0] == "") {
				cout << ": OK" << endl;
			} else {
				cout << ": error" << endl;
				error = true;
			}
			continue;
		}
		for (vector<string>::iterator it = row.begin(); it != row.end(); it++) {
			string got = *it;
			string expect = resultline[distance(row.begin(), it)];
			ostringstream type;
			type << "line " << static_cast<unsigned>(lineNo) << " col " << static_cast<size_t>(distance(row.begin(), it)+1);
			verify(false, type.str(), expect, got == expect, expect, got);
		}
	}

	return error ? 1 : 0;
}

/*
 * Copyright (C) John Baier 2014 <ebusd@johnm.de>
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

#include "data.h"
#include <iostream>
#include <iomanip>

using namespace libebus;

void verify(bool expectFailMatch, std::string type, std::string input,
		bool match, std::string expectStr, std::string gotStr)
{
	if (expectFailMatch == true) {
		if (match == true)
			std::cout << "  failed " << type << " match >" << input
			          << "< error: unexpectedly succeeded" << std::endl;
		else
			std::cout << "  failed " << type << " match >" << input << "< OK"
			          << std::endl;
	}
	else if (match == true)
		std::cout << "  " << type << " >" << input << "< OK" << std::endl;
	else
		std::cout << "  " << type << " >" << input << "< error: got >" << gotStr
		          << "<, expected >" << expectStr << "<" << std::endl;
}

int main()
{
	std::string checks[][5] = {
		//name;[pos];type[;[divisor|values][;[unit][;[comment]]]], decoded value, master, slave, flags
		{"x;1-10;str",  "Hallo, Du!",                    "10fe07000a48616c6c6f2c20447521", "00", ""},
		{"x;1-10;str",  "Hallo, Du ",                    "10fe07000a48616c6c6f2c20447520", "00", ""},
		{"x;1-10;str",  "          ",                    "10fe07000a20202020202020202020", "00", ""},
		{"x;1-11;str",  "",                              "10fe07000a20202020202020202020", "00", "rW"},
		{"x;;hex",      "20",                            "10fe07000120",                   "00", ""},
		{"x;1-10;hex",  "48 61 6c 6c 6f 2c 20 44 75 21", "10fe07000a48616c6c6f2c20447521", "00", ""},
		{"x;1-11;hex",  "",                              "10fe07000a48616c6c6f2c20447521", "00", "rW"},
		{"x;;bda",   "26.10.2014","10fe07000426100014", "00", ""},
		{"x;;bda",   "01.01.2000","10fe07000401010000", "00", ""},
		{"x;;bda",   "31.12.2099","10fe07000431120099", "00", ""},
		{"x;;bda",   "",          "10fe07000432100014", "00", "rw"},
		{"x;1-3;bda","26.10.2014","10fe070003261014",   "00", ""},
		{"x;1-3;bda","01.01.2000","10fe070003010100",   "00", ""},
		{"x;1-3;bda","31.12.2099","10fe070003311299",   "00", ""},
		{"x;1-3;bda","",          "10fe070003321299",   "00", "rw"},
		{"x;;bti",   "21:04:58",  "10fe070003580421",   "00", ""},
		{"x;;bti",   "00:00:00",  "10fe070003000000",   "00", ""},
		{"x;;bti",   "23:59:59",  "10fe070003595923",   "00", ""},
		{"x;;bti",   "",          "10fe070003605923",   "00", "rw"},
		{"x;;htm", "21:04", "10fe0700021504", "00", ""},
		{"x;;htm", "00:00", "10fe0700020000", "00", ""},
		{"x;;htm", "23:59", "10fe070002173b", "00", ""},
		{"x;;htm", "",      "10fe070002183b", "00", "rw"},
		{"x;;htm", "24:00", "10fe070002173b", "00", "Rw"},
		{"x;;ttm", "22:40", "10fe07000188",   "00", ""},
		{"x;;ttm", "00:00", "10fe07000100",   "00", ""},
		{"x;;ttm", "23:50", "10fe0700018f",   "00", ""},
		{"x;;ttm", "24:00", "10fe07000190",   "00", "rw"}, // TODO check range
		{"x;;ttm", "",      "10fe07000191",   "00", "rw"}, // TODO check range
		{"x;;bdy", "Mon",   "10fe07000300",   "00", ""},
		{"x;;bdy", "Sun",   "10fe07000306",   "00", ""},
		{"x;;bdy", "",      "10fe07000308",   "00", "rw"},
		{"x;;hdy", "Mon",   "10fe07000301",   "00", ""},
		{"x;;hdy", "Sun",   "10fe07000307",   "00", ""},
		{"x;;hdy", "",      "10fe07000308",   "00", "rw"},
		{"x;;bcd", "26",    "10feffff0126", "00", ""},
		{"x;;bcd", "0",     "10feffff0100", "00", ""},
		{"x;;bcd", "99",    "10feffff0199", "00", ""},
		{"x;;bcd", "-",     "10feffff01ff", "00", ""},
		{"x;;bcd", "",      "10feffff019a", "00", "rw"},
		{"x;;uch", "38",    "10feffff0126", "00", ""},
		{"x;;uch", "0",     "10feffff0100", "00", ""},
		{"x;;uch", "254",   "10feffff01fe", "00", ""},
		{"x;;uch", "-",     "10feffff01ff", "00", ""},
		{"x;;sch", "-90",   "10feffff01a6", "00", ""},
		{"x;;sch", "0",     "10feffff0100", "00", ""},
		{"x;;sch", "-1",    "10feffff01ff", "00", ""},
		{"x;;sch", "-",     "10feffff0180", "00", ""},
		{"x;;sch", "-127",  "10feffff0181", "00", ""},
		{"x;;sch", "127",   "10feffff017f", "00", ""},
		{"x;;d1b", "-90",   "10feffff01a6", "00", ""},
		{"x;;d1b", "0",     "10feffff0100", "00", ""},
		{"x;;d1b", "-1",    "10feffff01ff", "00", ""},
		{"x;;d1b", "-",     "10feffff0180", "00", ""},
		{"x;;d1b", "-127",  "10feffff0181", "00", ""},
		{"x;;d1b", "127",   "10feffff017f", "00", ""},
		{"x;;d1c", "19.5",  "10feffff0127", "00", ""},
		{"x;;d1c", "0.0",   "10feffff0100", "00", ""},
		{"x;;d1c", "100.0", "10feffff01c8", "00", ""},
		{"x;;d1c", "-",     "10feffff01ff", "00", ""},
		{"x;;uin", "38",     "10feffff022600", "00", ""},
		{"x;;uin", "0",      "10feffff020000", "00", ""},
		{"x;;uin", "65534",  "10feffff02feff", "00", ""},
		{"x;;uin", "-",      "10feffff02ffff", "00", ""},
		{"x;;sin", "-90",    "10feffff02a6ff", "00", ""},
		{"x;;sin", "0",      "10feffff020000", "00", ""},
		{"x;;sin", "-1",     "10feffff02ffff", "00", ""},
		{"x;;sin", "-",      "10feffff020080", "00", ""},
		{"x;;sin", "-32767", "10feffff020180", "00", ""},
		{"x;;sin", "32767",  "10feffff02ff7f", "00", ""},
		{"x;;flt", "-0.090", "10feffff02a6ff", "00", ""},
		{"x;;flt", "0.000",  "10feffff020000", "00", ""},
		{"x;;flt", "-0.001", "10feffff02ffff", "00", ""},
		{"x;;flt", "-",      "10feffff020080", "00", ""},
		{"x;;flt","-32.767", "10feffff020180", "00", ""},
		{"x;;flt", "32.767", "10feffff02ff7f", "00", ""},
		{"x;;d2b", "18.004", "10fe0700090112", "00", ""},
		{"x;;d2b", "0.000",  "10feffff020000", "00", ""},
		{"x;;d2b", "-0.004", "10feffff02ffff", "00", ""},
		{"x;;d2b", "-",      "10feffff020080", "00", ""},
		{"x;;d2b","-127.996","10feffff020180", "00", ""},
		{"x;;d2b", "127.996","10feffff02ff7f", "00", ""},
		{"x;;d2c", "288.06", "10fe0700090112", "00", ""},
		{"x;;d2c", "0.00",   "10feffff020000", "00", ""},
		{"x;;d2c", "-0.06",  "10feffff02ffff", "00", ""},
		{"x;;d2c", "-",      "10feffff020080", "00", ""},
		{"x;;d2c","-2047.94","10feffff020180", "00", ""},
		{"x;;d2c", "2047.94","10feffff02ff7f", "00", ""},
		{"x;;ulg", "38",         "10feffff0426000000", "00", ""},
		{"x;;ulg", "0",          "10feffff0400000000", "00", ""},
		{"x;;ulg", "4294967294", "10feffff04feffffff", "00", ""},
		{"x;;ulg", "-",          "10feffff04ffffffff", "00", ""},
		{"x;;slg", "-90",        "10feffff04a6ffffff", "00", ""},
		{"x;;slg", "0",          "10feffff0400000000", "00", ""},
		{"x;;slg", "-1",         "10feffff04ffffffff", "00", ""},
		{"x;;bi3", "1",            "10feffff0108", "00", ""},
		{"x;;bi3", "-",            "10feffff0100", "00", ""},
		{"x;;bi3;0=off,1=on","on", "10feffff0108", "00", ""},
		{"x;;bi3;0=off,1=on","off","10feffff0100", "00", ""},
		{"x;;b34", "1",            "10feffff0108", "00", ""},
		{"x;;b34", "-",            "10feffff0100", "00", ""},
		{"x;;b34", "3",            "10feffff0118", "00", ""},
		{"x;;b34;1=on","on",       "10feffff0108", "00", ""},
		{"x;;b34;1=on","-",        "10feffff0100", "00", ""},
		{"x;;b34;0=off,1=on,2=auto,3=eco","auto", "10feffff0110", "00", ""},
		{"x;;b34;0=off,1=on","on", "10feffff0108", "00", ""},
		{"x;;b34;0=off,1=on","off","10feffff0100", "00", ""},
		{"x;;uch;1=test,2=high,3=off,4=on","on","10feffff0104", "00", ""},
		{"x;s3;uch","3","1050ffff00", "03000003", ""},
		{"x;s3;uch","3","1050ffff00", "020000", "rW"},
		{"x;;d2b;;°C;Aussentemperatur","x=18.004 °C [Aussentemperatur]","10fe0700090112", "00", "v"},
		{"x;;bti;;;;y;;bda;;;;z;6;bdy", "21:04:58;26.10.2014;Sun","10fe07000758042126100614", "00", "c"},
		//TODO test bit combinations
		{"temprel;;d2b;;°C;Aussentemperatur","","", "", "p"}, // predefined type with relative pos
		{"tempabs;1;d2b;;°C;Aussentemperatur","","", "", "p"},// predefined type with absolute pos
		{"strucrelrel;;d2b;;;;y;;d1c","","", "", "p"},   // predefined combined type with relative pos
		{"strucrelabs;;d2b;;;;y;1;d1c","","", "", "p"},  // predefined combined type with relative pos
		{"strucabsrel;1;d2b;;;;y;;d1c","","", "", "p"},  // predefined combined type with relative pos
		{"strucabsabs;2;d2b;;;;y;1;d1c","","", "", "p"}, // predefined combined type with absolute pos
		{"x;;temprel","18.004","10fe0700090112", "00", ""}, // reference predefined type
		{"strucrelrel;;temprel,temprel","","", "", "p"},   // predefined combined type with relative pos
		{"strucrelabs;;temprel,tempabs","","", "", "p"},  // predefined combined type with relative pos
		{"strucabsrel;1;tempabs,temprel","","", "", "p"},  // predefined combined type with relative pos
		{"strucabs;2;tempabs","","", "", "p"}, // predefined combined type with absolute pos
	};
	std::map<std::string, DataField*> templates;
	std::vector<DataField*> fields;
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		std::string check[5] = checks[i];
		std::istringstream isstr(check[0]);
		std::string expectStr = check[1];
		SymbolString mstr = SymbolString(check[2], false);
		SymbolString sstr = SymbolString(check[3], false);
		std::string flags = check[4];
		bool isSet = flags.find('s') != std::string::npos;
		bool failedRead = flags.find('r') != std::string::npos;
		bool failedReadMatch = flags.find('R') != std::string::npos;
		bool failedWrite = flags.find('w') != std::string::npos;
		bool failedWriteMatch = flags.find('W') != std::string::npos;
		bool verbose = flags.find('v') != std::string::npos;
		bool combinedValue = flags.find('c') != std::string::npos;
		bool isPredefine = flags.find('p') != std::string::npos;
		std::string item;
		std::vector<std::string> entries;

		while (fields.empty() == false) {
			delete fields.back();
			fields.pop_back();
		}
		while (std::getline(isstr, item, ';') != 0)
			entries.push_back(item);

		std::vector<std::string>::iterator it = entries.begin();
		result_t result;
		do {
			result = DataField::create(it, entries.end(), templates, fields, isSet, isPredefine ? SYN : mstr[1]);
		} while (result == RESULT_OK && it != entries.end());

		if (result != RESULT_OK) {
			std::cout << "\"" << check[0] << "\": create error: "
			          << getResultCodeCStr(result) << std::endl;
			continue;
		}
		if (fields.empty() == true) {
			std::cout << "\"" << check[0] << "\": create error: empty" << std::endl;
			continue;
		}
		std::cout << "\"" << check[0] << "\": create OK" << std::endl;
		if (isPredefine) {
			// store new template
			while (fields.empty() == false) {
				DataField* field = fields.front();
				fields.erase(fields.begin());
				std::map<std::string, DataField*>::iterator current = templates.find(field->getName());
				if (current == templates.end())
					templates[field->getName()] = field;
				else {
					delete current->second;
					current->second = field;
				}
			}
			continue;
		}

		std::ostringstream output;
		std::istringstream input(expectStr);
		SymbolString writeMstr = SymbolString(mstr.getDataStr().substr(0, 10), false);
		SymbolString writeSstr = SymbolString(sstr.getDataStr().substr(0, 2), false);
		bool first = true, failed = false;
		while (fields.empty() == false) {
			DataField* field = fields.front();
			fields.erase(fields.begin());
			if (first == false)
				output << ";";

			result = field->read(mstr, sstr, output, verbose);
			if (failedRead == true)
				if (result == RESULT_OK)
					std::cout << "  failed read " << field->getName() << " >"
					          << check[2] << "< error: unexpectedly succeeded" << std::endl;
				else
					std::cout << "  failed read " << field->getName() << " >"
					          << check[2] << "< OK" << std::endl;
			else if (result != RESULT_OK) {
				std::cout << "  read " << field->getName() << " >" << check[2] << "< error: "
				          << getResultCodeCStr(result) << std::endl;
				failed = true;
			}
			else if (combinedValue == false) {
				bool match = strcasecmp(output.str().c_str(), expectStr.c_str()) == 0;
				verify(failedReadMatch, "read", check[2], match, expectStr, output.str());
			}

			if (verbose == false) {
				std::string token;
				std::getline(input, token, ';');
				std::istringstream tokeninput(token);

				result = field->write(tokeninput, writeMstr, writeSstr);
				if (failedWrite == true) {
					if (result == RESULT_OK)
						std::cout << "  failed write " << field->getName() << " >"
						          << expectStr << "< error: unexpectedly succeeded" << std::endl;
					else
						std::cout << "  failed write " << field->getName() << " >"
						          << expectStr << "< OK" << std::endl;
				}
				else if (result != RESULT_OK) {
					std::cout << "  write " << field->getName() << " >"
					          << expectStr << "< error: " << getResultCodeCStr(result) << std::endl;
					failed = true;
				}
			}
			first = false;
		}

		if (combinedValue == true && failedRead == false && failed == false) {
			bool match = strcasecmp(output.str().c_str(), expectStr.c_str()) == 0;
			verify(failedReadMatch, "read", check[2], match, expectStr, output.str());
		}
		if (verbose == false && failedWrite == false && failed == false) {
			bool match = mstr == writeMstr && sstr == writeSstr;
			verify(failedWriteMatch, "write", expectStr, match, mstr.getDataStr() + " " + sstr.getDataStr(), writeMstr.getDataStr() + " " + writeSstr.getDataStr());
		}
	}

	while (fields.empty() == false) {
		delete fields.back();
		fields.pop_back();
	}
	for (std::map<std::string, DataField*>::iterator it = templates.begin(); it != templates.end(); it++)
		delete it->second;

	return 0;

}

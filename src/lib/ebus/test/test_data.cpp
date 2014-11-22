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
		std::cout << "  " << type << " match >" << input << "< OK" << std::endl;
	else
		std::cout << "  " << type << " match >" << input << "< error: got >" << gotStr
		          << "<, expected >" << expectStr << "<" << std::endl;
}

int main()
{
	std::string checks[][5] = {
		//name;[len];type[;[divisor|values][;[unit][;[comment]]]], decoded value, master, slave, flags
		{"x;10;str",  "Hallo, Du!",                    "10fe07000a48616c6c6f2c20447521", "00", ""},
		{"x;10;str",  "Hallo, Du ",                    "10fe07000a48616c6c6f2c20447520", "00", ""},
		{"x;10;str",  "          ",                    "10fe07000a20202020202020202020", "00", ""},
		{"x;11;str",  "",                              "10fe07000a20202020202020202020", "00", "rW"},
		{"x;;hex",      "20",                            "10fe07000120",                   "00", ""},
		{"x;10;hex",  "48 61 6c 6c 6f 2c 20 44 75 21", "10fe07000a48616c6c6f2c20447521", "00", ""},
		{"x;11;hex",  "",                              "10fe07000a48616c6c6f2c20447521", "00", "rW"},
		{"x;;bda",   "26.10.2014","10fe07000426100014", "00", ""},
		{"x;;bda",   "01.01.2000","10fe07000401010000", "00", ""},
		{"x;;bda",   "31.12.2099","10fe07000431120099", "00", ""},
		{"x;;bda",   "",          "10fe07000432100014", "00", "rw"},
		{"x;3;bda","26.10.2014","10fe070003261014",   "00", ""},
		{"x;3;bda","01.01.2000","10fe070003010100",   "00", ""},
		{"x;3;bda","31.12.2099","10fe070003311299",   "00", ""},
		{"x;3;bda","",          "10fe070003321299",   "00", "rw"},
		{"x;;bti",   "21:04:58",  "10fe070003580421",   "00", ""},
		{"x;;bti",   "00:00:00",  "10fe070003000000",   "00", ""},
		{"x;;bti",   "23:59:59",  "10fe070003595923",   "00", ""},
		{"x;;bti",   "",          "10fe070003605923",   "00", "rw"},
		{"x;;htm", "21:04", "10fe0700021504", "00", ""},
		{"x;;htm", "00:00", "10fe0700020000", "00", ""},
		{"x;;htm", "23:59", "10fe070002173b", "00", ""},
		{"x;;htm", "24:00", "10fe0700021800", "00", ""},
		{"x;;htm", "",      "10fe070002183b", "00", "rw"},
		{"x;;htm", "24:01", "10fe0700021801", "00", "rw"},
		{"x;;ttm", "22:40", "10fe07000188",   "00", ""},
		{"x;;ttm", "00:00", "10fe07000100",   "00", ""},
		{"x;;ttm", "23:50", "10fe0700018f",   "00", ""},
		{"x;;ttm", "24:00", "10fe07000190",   "00", ""},
		{"x;;ttm", "",      "10fe07000191",   "00", "rw"},
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
		{"x;16;str", "0123456789ABCDEF",  "10feffff1130313233343536373839414243444546", "00", ""},
		{"x;17;uch", "",    "10feffff00", "00", "c"},
		{"x;s;uch", "0",   "1025ffff0310111213", "0300010203", "W"},
		{"x;s;uch", "0",   "1025ffff00", "0100", ""},
		{"x;s;uch;;;;y;m;uch", "2;3","1025ffff0103", "0102", ""},
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
		{"x;s;uch","3","1050ffff00", "0103", ""},
		{"x;;d2b;;°C;Aussentemperatur","x=18.004 °C [Aussentemperatur]","10fe0700090112", "00", "v"},
		{"x;;bti;;;;y;;bda;;;;z;;bdy", "21:04:58;26.10.2014;Sun","10fe0700085804212610001406", "00", ""}, // combination
		{"x;;bi3;;;;y;;bi5", "1;-",            "10feffff0108", "00", ""}, // bit combination
		{"x;;bi3;;;;y;;bi5", "1;1",            "10feffff0128", "00", ""}, // bit combination
		{"x;;bi3;;;;y;;bi5", "-;1",            "10feffff0120", "00", ""}, // bit combination
		{"x;;bi3;;;;y;;bi5", "-;-",            "10feffff0100", "00", ""}, // bit combination
		{"x;;bi3;;;;y;;bi7;;;;t;;uch", "-;-;9","10feffff020009", "00", ""}, // bit combination, auto pos incr
		{"x;;bi3;;;;y;;bi5;;;;t;;uch", "-;-;9","10feffff020009", "00", "RW"}, // bit combination
		{"temp;;d2b;;°C;Aussentemperatur","","", "", "t"}, // template with relative pos
		{"x;;temp","18.004","10fe0700020112", "00", ""}, // reference to template
		{"relrel;;d2b;;;;y;;d1c","","", "", "t"},   // template struct with relative pos
		{"x;;relrel","18.004;9.5","10fe070003011213", "00", ""}, // reference to template struct
		{"trelrel;;temp,temp","","", "", "t"},   // template struct with relative pos and ref to templates
		{"x;;trelrel","18.004;19.008","10fe07000401120213", "00", ""}, // reference to template struct
	};
	std::map<std::string, DataField*> templates;
	DataField* fields = NULL;
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		std::string check[5] = checks[i];
		std::istringstream isstr(check[0]);
		std::string expectStr = check[1];
		SymbolString mstr = SymbolString(check[2], false);
		SymbolString sstr = SymbolString(check[3], false);
		std::string flags = check[4];
		bool isSet = flags.find('s') != std::string::npos;
		bool failedCreate = flags.find('c') != std::string::npos;
		bool failedRead = flags.find('r') != std::string::npos;
		bool failedReadMatch = flags.find('R') != std::string::npos;
		bool failedWrite = flags.find('w') != std::string::npos;
		bool failedWriteMatch = flags.find('W') != std::string::npos;
		bool verbose = flags.find('v') != std::string::npos;
		bool isTemplate = flags.find('t') != std::string::npos;
		std::string item;
		std::vector<std::string> entries;

		while (std::getline(isstr, item, ';') != 0)
			entries.push_back(item);

		if (fields != NULL) {
			delete fields;
			fields = NULL;
		}
		std::vector<std::string>::iterator it = entries.begin();
		result_t result = DataField::create(it, entries.end(), templates, fields, isSet, isTemplate ? SYN : mstr[1]);

		if (failedCreate == true) {
			if (result == RESULT_OK)
				std::cout << "\"" << check[0] << "\": failed create error: unexpectedly succeeded" << std::endl;
			else
				std::cout << "\"" << check[0] << "\": failed create OK" << std::endl;
			continue;
		}
		if (result != RESULT_OK) {
			std::cout << "\"" << check[0] << "\": create error: "
					  << getResultCode(result) << std::endl;
			continue;
		}
		if (fields == NULL) {
			std::cout << "\"" << check[0] << "\": create error: NULL" << std::endl;
			continue;
		}
		if (it != entries.end()) {
			std::cout << "\"" << check[0] << "\": create error: trailing input" << std::endl;
			continue;
		}
		std::cout << "\"" << check[0] << "\": create OK" << std::endl;
		if (isTemplate) {
			// store new template
			std::string name = fields->getName();
			std::map<std::string, DataField*>::iterator current = templates.find(name);
			if (current == templates.end()) {
				templates[name] = fields;
			} else {
				delete current->second;
				current->second = fields;
			}
			fields = NULL;
			continue;
		}

		std::ostringstream output;
		SymbolString writeMstr = SymbolString(mstr.getDataStr().substr(0, 10), false);
		SymbolString writeSstr = SymbolString(sstr.getDataStr().substr(0, 2), false);
		result = fields->read(mstr, 0, sstr, 0, output, verbose);
		if (failedRead == true)
			if (result == RESULT_OK)
				std::cout << "  failed read " << fields->getName() << " >"
						  << check[2] << "< error: unexpectedly succeeded" << std::endl;
			else
				std::cout << "  failed read " << fields->getName() << " >"
						  << check[2] << "< OK" << std::endl;
		else if (result != RESULT_OK) {
			std::cout << "  read " << fields->getName() << " >" << check[2] << "< error: "
					  << getResultCode(result) << std::endl;
		}
		else {
			bool match = strcasecmp(output.str().c_str(), expectStr.c_str()) == 0;
			verify(failedReadMatch, "read", check[2], match, expectStr, output.str());
		}

		if (verbose == false) {
			std::istringstream input(expectStr);
			result = fields->write(input, writeMstr, 0, writeSstr, 0);
			if (failedWrite == true) {
				if (result == RESULT_OK)
					std::cout << "  failed write " << fields->getName() << " >"
							  << expectStr << "< error: unexpectedly succeeded" << std::endl;
				else
					std::cout << "  failed write " << fields->getName() << " >"
							  << expectStr << "< OK" << std::endl;
			}
			else if (result != RESULT_OK) {
				std::cout << "  write " << fields->getName() << " >"
						  << expectStr << "< error: " << getResultCode(result) << std::endl;
			}
			else {
				bool match = mstr == writeMstr && sstr == writeSstr;
				verify(failedWriteMatch, "write", expectStr, match, mstr.getDataStr() + " " + sstr.getDataStr(), writeMstr.getDataStr() + " " + writeSstr.getDataStr());
			}
		}
		delete fields;
		fields = NULL;
	}

	for (std::map<std::string, DataField*>::iterator it = templates.begin(); it != templates.end(); it++)
		delete it->second;

	return 0;

}

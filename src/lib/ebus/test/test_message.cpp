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

#include "message.h"
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
		std::cout << "  " << type << " >" << input << "< OK" << std::endl;
	else
		std::cout << "  " << type << " >" << input << "< error: got >" << gotStr
		          << "<, expected >" << expectStr << "<" << std::endl;
}

int main()
{
	// message= [type];class;name;[comment];[QQ];ZZ;id;fields...
	// field=   name;[pos];type[;[divisor|values][;[unit][;[comment]]]]
	std::string checks[][5] = {
		// "message", "flags"
		{";;first;;;fe;0700;x;;bda", "26.10.2014", "10fe07000426100014", "00", ""},
		{";;first;;;15;b509;id;x;;bda", "26.10.2014", "10fe07000426100014", "00", ""},
	};
	std::map<std::string, DataField*> templates;
	Message* message = NULL;
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		std::string check[5] = checks[i];
		std::istringstream isstr(check[0]);
		std::string expectStr = check[1];
		SymbolString mstr = SymbolString(check[2], false);
		SymbolString sstr = SymbolString(check[3], false);
		std::string flags = check[4];
		bool failedCreate = flags.find('c') != std::string::npos;
		std::string item;
		std::vector<std::string> entries;

		while (std::getline(isstr, item, ';') != 0)
			entries.push_back(item);

		if (message != NULL) {
			delete message;
			message = NULL;
		}
		std::vector<std::string>::iterator it = entries.begin();
		result_t result = Message::create(it, entries.end(), templates, message);

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
		if (message == NULL) {
			std::cout << "\"" << check[0] << "\": create error: NULL" << std::endl;
			continue;
		}
		if (it != entries.end()) {
			std::cout << "\"" << check[0] << "\": create error: trailing input" << std::endl;
			continue;
		}
		std::cout << "\"" << check[0] << "\": create OK" << std::endl;

		std::istringstream input(expectStr);
		result = message->prepare(SYN, mstr, input);
		if (result != RESULT_OK)
			std::cout << "  prepare >" << expectStr << "< error: "
					  << getResultCode(result) << std::endl;

		delete message;
		message = NULL;
	}

	for (std::map<std::string, DataField*>::iterator it = templates.begin(); it != templates.end(); it++)
		delete it->second;

	return 0;

}

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

int main ()
{
	//TODO dt_float,
	//TODO dt_d1b,
	//TODO dt_d1c,
	//TODO dt_d2c,
	std::string checks[][4] = {
		//name;position(s);type;factor;unit;comment
//		{"temp;1;d2b;;°C;Aussentemperatur","temp=18.004 °C [Aussentemperatur]","10fe070009019258042126100714cc", "00"},
//		{"zeit;1;ttm;2;Uhr;","zeit=22:40 Uhr","10feffff0188", "00"},
		{"hex;1-10;hex","53 70 65 69 63 68 65 72 20 20", "10fe07000a53706569636865722020", "00"},
		{"zeit;1;bti","21:04:58","10fe070009580421", "00"},
		{"datum;1;bda","26.10.2014","10fe07000926100714", "00"},
		{"datum;1-3;bda","26.10.2014","10fe070003261014", "00"},
		{"tag;1;bdy","Sun","10fe07000307", "00"},
		{"temp;1;d2b","18.004","10fe0700090112", "00"},
		{"zeit;1;ttm","22:40","10feffff0188", "00"},
		{"bcd;1;bcd","26","10feffff0126", "00"},
		{"bcd;1;bcd","-","10feffff01ff", "00"},
		{"uch;1;uch","38","10feffff0126", "00"},
		{"sch;1;sch","-90","10feffff01a6", "00"},
		{"uin;1;uin","38","10feffff022600", "00"},
		{"sin;1;sin","-90","10feffff02a6ff", "00"},
		{"ulg;1;ulg","38","10feffff0426000000", "00"},
		{"slg;1;slg","-90","10feffff04a6ffffff", "00"},
		{"str;1-9;str","hallo Du!","10feffff0868616c6c6f20447521", "00"},
		{"str;1-9;str","hallo Du ","10feffff0868616c6c6f20447500", "00"},
	};
	for (size_t i = 0; i < sizeof(checks)/sizeof(checks[0]); i++) {
		std::istringstream isstr(checks[i][0]);
		std::string expectStr = checks[i][1];
		SymbolString mstr = SymbolString(checks[i][2], false);
		SymbolString sstr = SymbolString(checks[i][3], false);
		std::string item;
		std::vector<std::string> entries;

		while (std::getline(isstr, item, ';') != 0)
			entries.push_back(item);

		std::vector<std::string>::iterator it = entries.begin();
		DataField* field = DataField::create(mstr[1], false, it, entries.end());

		if (field == NULL) {
			std::cout << "create \"" << checks[i][0] << "\" invalid: null" << std::endl;
			return 1;
		}
		std::cout << "create \"" << checks[i][0] << "\" successful" << std::endl;


		std::string gotStr = field->parseSymbols(mstr, sstr);

		if (strcasecmp(gotStr.c_str(), expectStr.c_str()) == 0)
			std::cout << "parse successful: " << gotStr << std::endl;
		else
			std::cout << "parse invalid: got " << gotStr
				<< ", expected " << expectStr << std::endl;

		SymbolString writeMstr = SymbolString(mstr.getDataStr().substr(0, 10), false);
		SymbolString writeSstr = SymbolString(sstr.getDataStr().substr(0, 2), false);
		if (field->formatSymbols(gotStr, writeMstr, writeSstr) == false)
			std::cout << "format failed" << std::endl;
		else {
			if (mstr == writeMstr && sstr == writeSstr)
				std::cout << "format successful" << std::endl;
			else {
				std::cout << "format invalid: ";
				if (mstr == writeMstr)
					std::cout << "master OK";
				else
					std::cout << "master got " << writeMstr.getDataStr() << ", expected " << mstr.getDataStr();

				if (sstr == writeSstr)
					std::cout << ", slave OK";
				else
					std::cout << ", slave got " << writeSstr.getDataStr() << ", expected " << sstr.getDataStr();
				std::cout << std::endl;
			}
		}
		delete field;
	}
	return 0;

}

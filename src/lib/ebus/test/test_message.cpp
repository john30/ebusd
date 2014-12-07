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
#include <fstream>
#include <iomanip>

using namespace std;

void verify(bool expectFailMatch, string type, string input,
		bool match, string expectStr, string gotStr)
{
	if (expectFailMatch == true) {
		if (match == true)
			cout << "  failed " << type << " match >" << input
			        << "< error: unexpectedly succeeded" << endl;
		else
			cout << "  failed " << type << " match >" << input << "< OK" << endl;
	}
	else if (match == true)
		cout << "  " << type << " match >" << input << "< OK" << endl;
	else
		cout << "  " << type << " match >" << input << "< error: got >"
		        << gotStr << "<, expected >" << expectStr << "<" << endl;
}

int main()
{
	// message= [type];class;name;[comment];[QQ];ZZ;PBSB;fields...
	// field=   name;[pos];type[;[divisor|values][;[unit][;[comment]]]]
	string checks[][5] = {
		// "message", "flags"
		{"u;;first;;;fe;0700;;x;;bda", "26.10.2014", "fffe0700042610061451", "00", "p"},
		{"w;;first;;;15;b509;0400;date;;bda", "26.10.2014", "ff15b5090604002610061445", "00", "m"},
		{"r;ehp;time;;;08;b509;0d2800;;;time", "15:00:17", "ff08b509030d2800ea", "0311000f00", "m"},
		{"r;ehp;date;;;08;b509;0d2900;;;hda:3", "23.11.2014", "ff08b509030d290071", "03170b0e5a", "m"},
		{"u;ehp;ActualEnvironmentPower;Energiebezug;;08;B509;29BA00;;s;IGN:2;;;;;s;power", "8", "1008b5090329ba00", "03ba0008", "pm"},
		{"uw;ehp;test;Test;;08;B5de;ab;;;power;;;;;s;hex:1", "8;39", "1008b5de02ab08", "0139", "pm"},
		{"","55.50;ok","1025b50903290000","050000780300",""},
		{"","no;25","10feb505042700190023","",""},
	};
	DataFieldTemplates* templates = new DataFieldTemplates();
	result_t result = templates->readFromFile("_types.csv");
	if (result == RESULT_OK)
		cout << "read templates OK" << endl;
	else
		cout << "read templates error: " << getResultCode(result) << endl;

	MessageMap* messages = new MessageMap();
	result = messages->readFromFile("neu-ehp00.csv", templates);
	if (result == RESULT_OK)
		cout << "read messages OK" << endl;
	else
		cout << "read messages error: " << getResultCode(result) << endl;

	Message* message = NULL;
	Message* deleteMessage = NULL;
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		string check[5] = checks[i];
		istringstream isstr(check[0]);
		string inputStr = check[1];
		SymbolString mstr(check[2], false);
		SymbolString sstr(check[3], false);
		string flags = check[4];
		bool dontMap = flags.find('m') != string::npos;
		bool failedCreate = flags.find('c') != string::npos;
		bool failedPrepare = flags.find('p') != string::npos;
		bool failedPrepareMatch = flags.find('P') != string::npos;
		string item;
		vector<string> entries;

		while (getline(isstr, item, ';') != 0)
			entries.push_back(item);

		if (deleteMessage != NULL) {
			delete deleteMessage;
			deleteMessage = NULL;
		}
		if (entries.size() == 0) {
			message = messages->find(mstr);
			if (message == NULL) {
				cout << "\"" << check[2] << "\": find error: NULL" << endl;
				continue;
			}
			cout << "\"" << check[2] << "\": find OK" << endl;
		} else {
			vector<string>::iterator it = entries.begin();
			result = Message::create(it, entries.end(), NULL, templates, deleteMessage);
			if (failedCreate == true) {
				if (result == RESULT_OK)
					cout << "\"" << check[0] << "\": failed create error: unexpectedly succeeded" << endl;
				else
					cout << "\"" << check[0] << "\": failed create OK" << endl;
				continue;
			}
			if (result != RESULT_OK) {
				cout << "\"" << check[0] << "\": create error: "
						<< getResultCode(result) << endl;
				printErrorPos(entries.begin(), entries.end(), it);
				continue;
			}
			if (deleteMessage == NULL) {
				cout << "\"" << check[0] << "\": create error: NULL" << endl;
				continue;
			}
			if (it != entries.end()) {
				cout << "\"" << check[0] << "\": create error: trailing input" << endl;
				continue;
			}
			cout << "\"" << check[0] << "\": create OK" << endl;
			if (dontMap == false) {
				result_t result = messages->add(deleteMessage);
				if (result != RESULT_OK) {
					cout << "\"" << check[0] << "\": add error: "
							<< getResultCode(result) << endl;
					continue;
				}
				cout << "  map OK" << endl;
				message = deleteMessage;
				deleteMessage = NULL;
				Message* foundMessage = messages->find(mstr);
				if (foundMessage == message)
					cout << "  find OK" << endl;
				else if (foundMessage == NULL)
					cout << "  find error: NULL" << endl;
				else
					cout << "  find error: different" << endl;
			}
			else
				message = deleteMessage;
		}
		istringstream input(inputStr);
		SymbolString writeMstr;
		if (message->isPassive() == true) {
			ostringstream output;
			result = message->decode(pt_masterData, mstr, output);
			if (result == RESULT_OK)
				result = message->decode(pt_slaveData, sstr, output, output.str().empty() == false);
			if (result != RESULT_OK) {
				cout << "  \"" << inputStr << "\": decode error: "
						<< getResultCode(result) << endl;
				continue;
			}
			cout << "  \"" << inputStr << "\": decode OK" << endl;

			bool match = inputStr == output.str();
			verify(false, "decode", check[2] + "/" + check[3], match, inputStr, output.str());
		} else {
			result = message->prepareMaster(0xff, writeMstr, input);
			if (failedPrepare == true) {
				if (result == RESULT_OK)
					cout << "  \"" << inputStr << "\": failed prepare error: unexpectedly succeeded" << endl;
				else
					cout << "  \"" << inputStr << "\": failed prepare OK" << endl;
				continue;
			}

			if (result != RESULT_OK) {
				cout << "  \"" << inputStr << "\": prepare error: "
						<< getResultCode(result) << endl;
				continue;
			}
			cout << "  \"" << inputStr << "\": prepare OK" << endl;

			bool match = writeMstr==mstr;
			verify(failedPrepareMatch, "prepare", inputStr, match, mstr.getDataStr(), writeMstr.getDataStr());
		}
	}

	if (deleteMessage != NULL) {
		delete deleteMessage;
		deleteMessage = NULL;
	}

	delete templates;
	delete messages;

	return 0;

}

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

void printErrorPos(vector<string>::iterator it, const vector<string>::iterator end, vector<string>::iterator pos)
{
	cout << "Erroneous item is here:" << endl;
	bool first = true;
	int cnt = 0;
	if (pos > it)
		pos--;
	while (it != end) {
		if (first == true)
			first = false;
		else {
			cout << ';';
			if (it <= pos) {
				cnt++;
			}
		}
		if (it < pos) {
			cnt += (*it).length();
		}
		cout << (*it++);
	}
	cout << endl;
	cout << setw(cnt) << " " << setw(0) << "^" << endl;
}

bool readTemplates(string filename, DataFieldTemplates* templates)
{
	ifstream ifs;
	ifs.open(filename.c_str(), ifstream::in);
	if (ifs.is_open() == false) {
		cerr << "error reading \"" << filename << endl;
		return false;
	}

	string line;
	unsigned int lineNo = 0;
	vector<string> row;
	string token;
	while (getline(ifs, line) != 0) {
		lineNo++;
		istringstream isstr(line);
		row.clear();
		while (getline(isstr, token, ';') != 0)
			row.push_back(token);

		// skip empty and commented rows
		if (row.empty() == true || row[0][0] == '#')
			continue;

		DataField* field = NULL;
		vector<string>::iterator it = row.begin();
		result_t result = DataField::create(it, row.end(), templates, field);
		if (result != RESULT_OK) {
			cerr << "error reading \"" << filename << "\" line " << static_cast<unsigned>(lineNo) << ": " << getResultCode(result) << endl;
			printErrorPos(row.begin(), row.end(), it);
		} else if (it != row.end())
			cout << "extra data in \"" << filename << "\" line " << static_cast<unsigned>(lineNo) << endl;
		else {
			result = templates->add(field, true);
			if (result != RESULT_OK) {
				cerr << "error adding template \"" << field->getName() << "\": " << getResultCode(result) << endl;
				delete field;
			}
		}
	}

	ifs.close();
	return true;
}

int main()
{
	// message= [type];class;name;[comment];[QQ];ZZ;PBSB;fields...
	// field=   name;[pos];type[;[divisor|values][;[unit][;[comment]]]]
	string checks[][5] = {
		// "message", "flags"
		{"c;;first;;;fe;0700;x;;bda", "26.10.2014", "fffe0700042610061451", "00", "p"},
		{"w;;first;;;15;b5090400;date;;bda", "26.10.2014", "ff15b5090604002610061445", "00", "m"},
		{"r;ehp;time;;;08;b5090d2800;;;time", "15:00:17", "ff08b509030d2800ea", "0311000f00", "m"},
		{"r;ehp;date;;;08;b5090d2900;;;hda:3", "23.11.2014", "ff08b509030d290071", "03170b0e5a", "m"},
		{"c;ehp;ActualEnvironmentPower;Energiebezug;;08;B50929BA00;;s;IGN:2;;;;;s;power", "8", "1008b5090329ba00", "03ba0008", "p"},
	};
	DataFieldTemplates* templates = new DataFieldTemplates();
	readTemplates("_types.csv", templates);

	Message *message = NULL;
	Message* deleteMessage = NULL;
	MessageMap* messages = new MessageMap();
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		string check[5] = checks[i];
		istringstream isstr(check[0]);
		string inputStr = check[1];
		SymbolString mstr = SymbolString(check[2], false);
		SymbolString sstr = SymbolString(check[3], false);
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
		vector<string>::iterator it = entries.begin();
		result_t result = Message::create(it, entries.end(), templates, deleteMessage);

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
			result = messages->add(deleteMessage);
			if (result != RESULT_OK) {
				cout << "\"" << check[0] << "\": add error: "
						<< getResultCode(result) << endl;
				continue;
			}
			cout << "  map OK" << endl;
			message = deleteMessage;
			deleteMessage = NULL;
			if (messages->find(mstr) == message)
				cout << "  find OK" << endl;
			else
				cout << "  find error: NULL" << endl;
		}
		else
			message = deleteMessage;
		istringstream input(inputStr);
		SymbolString writeMstr = SymbolString();
		result = message->prepare(0xff, writeMstr, input);
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

	if (deleteMessage != NULL) {
		delete deleteMessage;
		deleteMessage = NULL;
	}

	delete templates;
	delete messages;

	return 0;

}

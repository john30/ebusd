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

#include "data.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>

using namespace std;

/** the known data field types. */
static const dataType_t dataTypes[] = {
	{"IGN",16*8,bt_str, IGN|ADJ,          0,          1,         16,    0, 0}, // >= 1 byte ignored data
	{"STR",16*8,bt_str,     ADJ,        ' ',          1,         16,    0, 0}, // >= 1 byte character string filled up with space
	{"HEX",16*8,bt_hexstr,  ADJ,          0,          2,         47,    0, 0}, // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	{"BDA", 32, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date with weekday in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is weekday Mon=0x00 - Sun=0x06)
	{"BDA", 24, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99)
	{"HDA", 32, bt_dat,       0,          0,         10,         10,    0, 0}, // date with weekday, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is weekday Mon=0x01 - Sun=0x07))
	{"HDA", 24, bt_dat,       0,          0,         10,         10,    0, 0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99) // TODO remove duplicate of BDA
	{"BTI", 24, bt_tim, BCD|REV,          0,          8,          8,    0, 0}, // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	{"HTI", 24, bt_tim,       0,          0,          8,          8,    0, 0}, // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x17,0x3b,0x3b)
	{"HTM", 16, bt_tim,       0,          0,          5,          5,    0, 0}, // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x17,0x3b)
	{"TTM",  8, bt_tim,       0,       0x90,          5,          5,    0, 0}, // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	{"BDY",  8, bt_num, DAY|LST,       0x07,          0,          6,    1, 0}, // weekday, "Mon" - "Sun" (0x00 - 0x06) [ebus type]
	{"HDY",  8, bt_num, DAY|LST,       0x00,          1,          7,    1, 0}, // weekday, "Mon" - "Sun" (0x01 - 0x07) [Vaillant type]
	{"BCD",  8, bt_num, BCD|LST,       0xff,          0,       0x99,    1, 0}, // unsigned decimal in BCD, 0 - 99
	{"UCH",  8, bt_num,     LST,       0xff,          0,       0xfe,    1, 0}, // unsigned integer, 0 - 254
	{"SCH",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1, 0}, // signed integer, -127 - +127
	{"D1B",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1, 0}, // signed integer, -127 - +127
	{"D1C",  8, bt_num,       0,       0xff,       0x00,       0xc8,    2, 1}, // unsigned number (fraction 1/2), 0 - 100 (0x00 - 0xc8, replacement 0xff)
	{"UIN", 16, bt_num,     LST,     0xffff,          0,     0xfffe,    1, 0}, // unsigned integer, 0 - 65534
	{"SIN", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,    1, 0}, // signed integer, -32767 - +32767
	{"FLT", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff, 1000, 3}, // signed number (fraction 1/1000), -32.767 - +32.767
	{"D2B", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,  256, 3}, // signed number (fraction 1/256), -127.99 - +127.99
	{"D2C", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,   16, 2}, // signed number (fraction 1/16), -2047.9 - +2047.9
	{"ULG", 32, bt_num,     LST, 0xffffffff,          0, 0xfffffffe,    1, 0}, // unsigned integer, 0 - 4294967294
	{"SLG", 32, bt_num,     SIG, 0x80000000, 0x80000001, 0xffffffff,    1, 0}, // signed integer, -2147483647 - +2147483647
	{"BI0",  7, bt_num, ADJ|LST,          0,          0,       0xef,    1, 0}, // bit 0 (up to 7 bits until bit 6)
	{"BI1",  7, bt_num, ADJ|LST,          0,          0,       0x7f,    1, 1}, // bit 1 (up to 7 bits until bit 7)
	{"BI2",  6, bt_num, ADJ|LST,          0,          0,       0x3f,    1, 2}, // bit 2 (up to 6 bits until bit 7)
	{"BI3",  5, bt_num, ADJ|LST,          0,          0,       0x1f,    1, 3}, // bit 3 (up to 5 bits until bit 7)
	{"BI4",  4, bt_num, ADJ|LST,          0,          0,       0x0f,    1, 4}, // bit 4 (up to 4 bits until bit 7)
	{"BI5",  3, bt_num, ADJ|LST,          0,          0,       0x07,    1, 5}, // bit 5 (up to 3 bits until bit 7)
	{"BI6",  2, bt_num, ADJ|LST,          0,          0,       0x03,    1, 6}, // bit 6 (up to 2 bits until bit 7)
	{"BI7",  1, bt_num, ADJ|LST,          0,          0,       0x01,    1, 7}, // bit 7
};


/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

#define VALUE_SEPARATOR ','
#define LENGTH_SEPARATOR ':'
#define NULL_VALUE "-"
#define MAX_POS 16

unsigned int parseInt(const char* str, int base, const unsigned int minValue, const unsigned int maxValue, result_t& result, unsigned int* length) {
	char* strEnd = NULL;

	unsigned int ret = strtoul(str, &strEnd, base);

	if (strEnd == NULL || *strEnd != 0) {
		result = RESULT_ERR_INVALID_ARG; // invalid value
		return 0;
	}

	if (ret < minValue || ret > maxValue) {
		result = RESULT_ERR_INVALID_ARG; // invalid value
		return 0;
	}
	if (length != NULL)
		*length = strEnd - str;

	result = RESULT_OK;
	return ret;
}

void printErrorPos(vector<string>::iterator begin, const vector<string>::iterator end, vector<string>::iterator pos, char separator)
{
	cout << "Erroneous item is here:" << endl;
	bool first = true;
	int cnt = 0;
	if (pos > begin)
		pos--;
	while (begin != end) {
		if (first == true)
			first = false;
		else {
			cout << separator;
			if (begin <= pos) {
				cnt++;
			}
		}
		if (begin < pos) {
			cnt += (*begin).length();
		}
		cout << (*begin++);
	}
	cout << endl;
	cout << setw(cnt) << " " << setw(0) << "^" << endl;
}


result_t DataField::create(vector<string>::iterator& it,
		const vector<string>::iterator end,
		DataFieldTemplates* templates,
		DataField*& returnField, const bool isSetMessage,
		const unsigned char dstAddress)
{
	vector<SingleDataField*> fields;
	string firstName, firstComment;
	result_t result = RESULT_OK;
	while (it != end && result == RESULT_OK) {
		string unit, comment;
		PartType partType;
		unsigned int divisor = 0;
		const bool isTemplate = dstAddress == SYN;
		string token;
		if (it == end)
			break;

		// name;part;type[:len][;[divisor|values][;[unit][;[comment]]]]
		const string name = *it++;
		if (it == end)
			break;

		const char* partStr = (*it++).c_str();
		if (it == end)
			break;

		if (fields.empty() == true) {
			firstName = name;
			firstComment = comment;
		}
		if (dstAddress == BROADCAST || isMaster(dstAddress)
			|| (isTemplate == false && isSetMessage == true && partStr[0] == 0)
			|| strcasecmp(partStr, "M") == 0) { // master data
			partType = pt_masterData;
		}
		else if ((isTemplate == false && isSetMessage == false && partStr[0] == 0)
			|| strcasecmp(partStr, "S") == 0) { // slave data
			partType = pt_slaveData;
		}
		else if (isTemplate) {
			partType = pt_any;
		}
		else {
			result = RESULT_ERR_INVALID_ARG;
			break;
		}

		string typeStr = *it++;
		if (typeStr.empty() == true) {
			if (name.empty() == false || partStr[0] != 0)
				result = RESULT_ERR_INVALID_ARG;
			break;
		}

		map<unsigned int, string> values;
		if (it != end) {
			string divisorStr = *it++;
			if (divisorStr.empty() == false) {
				if (divisorStr.find('=') == string::npos) {
					divisor = parseInt(divisorStr.c_str(), 10, 1, 10000, result);
					if (result != RESULT_OK)
						break;
				}
				else {
					istringstream stream(divisorStr);
					while (getline(stream, token, VALUE_SEPARATOR) != 0) {
						const char* str = token.c_str();
						char* strEnd = NULL;
						unsigned int id = strtoul(str, &strEnd, 10);
						if (strEnd == NULL || strEnd == str || *strEnd != '=') {
							result = RESULT_ERR_INVALID_ARG;
							break;
						}

						values[id] = string(strEnd + 1);
					}
					if (result != RESULT_OK)
						break;
				}
			}
		}

		if (it == end)
			unit = "";
		else {
			unit = *it++;
			if (strcasecmp(unit.c_str(), NULL_VALUE) == 0)
				unit.clear();
		}

		if (it == end)
			comment = "";
		else {
			comment = *it++;
			if (strcasecmp(comment.c_str(), NULL_VALUE) == 0)
				comment.clear();
		}

		size_t pos = typeStr.find(LENGTH_SEPARATOR);
		unsigned char length;
		if (pos == string::npos) {
			length = 0;
			// check for reference(s) to templates
			if (templates != NULL) {
				istringstream stream(typeStr);
				bool found = false;
				string lengthStr;
				while (getline(stream, token, VALUE_SEPARATOR) != 0) {
					DataField* templ = templates->get(token);
					if (templ == NULL) {
						if (found == false)
							break; // fallback to direct definition
						result = RESULT_ERR_INVALID_ARG; // cannot mix reference and direct definition
						break;
					}
					found = true;
					result = templ->derive(name, comment, unit, partType, divisor, values, fields);
					if (result != RESULT_OK)
						break;
				}
				if (result != RESULT_OK)
					break;
				if (found == true)
					continue; // go to next definition
			}
		}
		else {
			length = parseInt(typeStr.substr(pos+1).c_str(), 10, 1, MAX_POS, result);
			if (result != RESULT_OK)
				break;
			typeStr = typeStr.substr(0, pos);
		}

		SingleDataField* add = NULL;
		const char* typeName = typeStr.c_str();
		for (size_t i = 0; result == RESULT_OK && add == NULL && i < sizeof(dataTypes) / sizeof(dataTypes[0]); i++) {
			dataType_t dataType = dataTypes[i];
			if (strcasecmp(typeName, dataType.name) == 0) {
				unsigned char bitCount = dataType.maxBits;
				unsigned char useLength = (bitCount + 7) / 8;
				if ((dataType.flags & ADJ) != 0) { // adjustable length
					if ((bitCount % 8) != 0) {
						if (length == 0) {
							useLength = 1; // default length: 1 byte
							bitCount = 1; // default count: 1 bit
						}
						else if (length > bitCount) {
							result = RESULT_ERR_INVALID_ARG; // invalid length
							break;
						}
						else {
							bitCount = length;
							useLength = (length + 7) / 8;
						}
					}
					else if (length == 0) {
						useLength = 1; // default length: 1 byte
					}
					else if (length <= useLength) {
						useLength = length;
					}
					else {
						result = RESULT_ERR_INVALID_ARG; // invalid length
						break;
					}
				}
				else if (length > 0 && length != useLength)
					continue; // check for another one with same name but different length

				switch (dataType.type)
				{
				case bt_str:
				case bt_hexstr:
				case bt_dat:
				case bt_tim:
					add = new StringDataField(name, comment, unit, dataType, partType, useLength);
					break;
				case bt_num:
					if (values.empty() == true && (dataType.flags & DAY) != 0) {
						for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++)
							values[dataType.minValueOrLength + i] = dayNames[i];
					}
					if (values.empty() == true || (dataType.flags & LST) == 0) {
						if (divisor == 0) {
							divisor = dataType.divisor;
						}
						else
							divisor *= dataType.divisor;

						add = new NumberDataField(name, comment, unit, dataType, partType, useLength, bitCount, divisor);
						break;
					}
					if (values.begin()->first < dataType.minValueOrLength
							|| values.rbegin()->first > dataType.maxValueOrLength) {
						result = RESULT_ERR_INVALID_ARG;
						break;
					}

					add = new ValueListDataField(name, comment, unit, dataType, partType, useLength, bitCount, values);
					break;
				}
			}
		}
		if (add != NULL)
			fields.push_back(add);
		else if (result == RESULT_OK)
			result = RESULT_ERR_INVALID_ARG; // type not found
	}
	if (fields.empty() == true || result != RESULT_OK) {
		while (fields.empty() == false) {
			delete fields.back();
			fields.pop_back();
		}
		return result == RESULT_OK ? RESULT_ERR_INVALID_ARG  :result;
	}

	if (fields.size() == 1)
		returnField = fields[0];
	else {
		returnField = new DataFieldSet(firstName, firstComment, fields);
	}
	return RESULT_OK;
}


result_t SingleDataField::read(SymbolString& masterData, unsigned char masterOffset,
		SymbolString& slaveData, unsigned char slaveOffset,
		ostringstream& output,
		bool verbose, char separator)
{
	SymbolString& input = m_partType != pt_slaveData ? masterData : slaveData;
	unsigned char offset;
	switch (m_partType)
	{
	case pt_masterData:
		offset = 5 + masterOffset; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset = 1 + slaveOffset; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_ARG; // invalid part type
	}

	if (isIgnored() == true) {
		if (offset + m_length > input.size()) {
			return RESULT_ERR_INVALID_ARG;
		}
		return RESULT_OK;
	}

	if (verbose == true)
		output << m_name << "=";

	result_t result = readSymbols(input, offset, output);
	if (result != RESULT_OK)
		return result;

	if (verbose && m_unit.length() > 0)
		output << " " << m_unit;
	if (verbose && m_comment.length() > 0)
		output << " [" << m_comment << "]";

	return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input,
		SymbolString& masterData, unsigned char masterOffset,
		SymbolString& slaveData, unsigned char slaveOffset,
		char separator)
{
	SymbolString& output = m_partType != pt_slaveData ? masterData : slaveData;
	unsigned char offset;
	switch (m_partType)
	{
	case pt_masterData:
		offset = 5 + masterOffset; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset = 1 + slaveOffset; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_ARG;
	}
	return writeSymbols(input, offset, output);
}


result_t StringDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (divisor != 0 || values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for string field
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;

	fields.push_back(new StringDataField(name, comment, unit, m_dataType, partType, m_length));

	return RESULT_OK;
}

result_t StringDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch, last = 0;

	if (baseOffset + m_length > input.size()) {
		return RESULT_ERR_INVALID_ARG;
	}

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		if (m_length == 4 && i == 2 && m_dataType.type == bt_dat)
			continue; // skip weekday in between
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0 || m_dataType.type == bt_dat) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_INVALID_ARG; // invalid BCD
			ch = (ch >> 4) * 10 + (ch & 0x0f);
		}
		switch (m_dataType.type)
		{
		case bt_hexstr:
			if (i > 0)
				output << ' ';
			output << nouppercase << setw(2) << hex << setfill('0')
			        << static_cast<unsigned>(ch);
			break;
		case bt_dat:
			if (i + 1 == m_length)
				output << (2000 + ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_INVALID_ARG; // invalid date
			else
				output << setw(2) << setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case bt_tim:
			if (m_length == 1) { // truncated time
				if (i == 0) {
					ch /= 6; // hours
					offset -= incr; // repeat for minutes
					count++;
				}
				else
					ch = (ch % 6) * 10; // minutes
			}
			if ((i == 0 && ch > 24) || (i > 0 && (ch > 59 || ( last == 24 && ch > 0) )))
				return RESULT_ERR_INVALID_ARG; // invalid time
			if (i > 0)
				output << ":";
			output << setw(2) << setfill('0') << static_cast<unsigned>(ch);
			break;
		default:
			if (ch < 0x20)
				ch = m_dataType.replacement;
			output << static_cast<char>(ch);
			break;
		}
		last = ch;
	}

	return RESULT_OK;
}

result_t StringDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned long int value = 0, last = 0, lastLast = 0;
	string token;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	if (isIgnored() == true) {
		for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
			output[baseOffset + offset] = m_dataType.replacement; // fill up with replacement
		}
		return RESULT_OK;
	}
	result_t result;
	size_t i = 0;
	for (size_t offset = start; i < count; offset += incr, i++) {
		switch (m_dataType.type)
		{
		case bt_hexstr:
			while (input.eof() == false && input.peek() == ' ')
				input.get();
			if (input.eof() == true) // no more digits
				value = m_dataType.replacement; // fill up with replacement
			else {
				token.clear();
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_ARG; // too short hex value
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_ARG; // too short hex value

				value = parseInt(token.c_str(), 16, 0, 0xff, result);
				if (result != RESULT_OK)
					return result; // invalid hex value
			}
			break;
		case bt_dat:
			if (m_length == 4 && i == 2)
				continue; // skip weekday in between
			if (input.eof() == true || getline(input, token, '.') == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete
			value = parseInt(token.c_str(), 10, 0, 2099, result);
			if (result != RESULT_OK)
				return result; // invalid date part
			if (i + 1 == m_length) {
				if (m_length == 4) {
					// calculate local week day
					struct tm t;
					t.tm_min = t.tm_sec = 0;
					t.tm_hour = 12;
					t.tm_mday = lastLast;
					t.tm_mon = last-1; // January=0
					t.tm_year = (value < 100 ? value + 2000 : value) - 1900;
					t.tm_isdst = 0; // automatic
					if (mktime(&t) < 0)
						return RESULT_ERR_INVALID_ARG; // invalid date
					unsigned char daysSinceSunday = (unsigned char)t.tm_wday; // Sun=0
					if ((m_dataType.flags & BCD) != 0)
						output[baseOffset + offset - incr] = (6+daysSinceSunday) % 7; // Sun=0x06
					else
						output[baseOffset + offset - incr] = (daysSinceSunday==0 ? 7 : daysSinceSunday); // Sun=0x07
				}
				if (value >= 2000)
					value -= 2000;
				else if (value > 99)
					return RESULT_ERR_INVALID_ARG; // invalid year
			} else if (value < 1 || (i == 0 && value > 31) || (i == 1 && value > 12))
				return RESULT_ERR_INVALID_ARG; // invalid date part
			break;
		case bt_tim:
			if (input.eof() == true || getline(input, token, LENGTH_SEPARATOR) == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete
			value = parseInt(token.c_str(), 10, 0, 59, result);
			if (result != RESULT_OK)
				return result; // invalid time part
			if ((i == 0 && value > 24) || (i > 0 && (last == 24 && value > 0) ))
				return RESULT_ERR_INVALID_ARG; // invalid time part
			if (m_length == 1) { // truncated time
				if (i == 0) {
					last = value;
					offset -= incr; // repeat for minutes
					count++;
					continue;
				}
				if ((value % 10) != 0)
					return RESULT_ERR_INVALID_ARG; // invalid truncated time minutes
				value = last * 6 + (value / 10);
				if (value > 24 * 6)
					return RESULT_ERR_INVALID_ARG; // invalid time
			}
			break;
		default:
			if (input.eof() == true)
				value = m_dataType.replacement;
			else {
				value = input.get();
				if (input.eof() == true || value < 0x20)
					value = m_dataType.replacement;
			}
			break;
		}
		lastLast = last;
		last = value;
		if ((m_dataType.flags & BCD) != 0 || m_dataType.type == bt_dat) {
			if (value > 99)
				return RESULT_ERR_INVALID_ARG; // invalid BCD
			value = ((value / 10) << 4) | (value % 10);
		}
		if (value > 0xff)
			return RESULT_ERR_INVALID_ARG; // value out of range
		output[baseOffset + offset] = (unsigned char)value;
	}

	if (i < m_length)
		return RESULT_ERR_INVALID_ARG; // input too short

	return RESULT_OK;
}


bool NumericDataField::hasFullByteOffset(bool after)
{
	return m_length > 1 || (m_bitCount % 8) == 0
		|| (after == true && m_bitOffset + (m_bitCount % 8) >= 8);
}

result_t NumericDataField::readRawValue(SymbolString& input,
		unsigned char baseOffset, unsigned int& value)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch;

	if (baseOffset + m_length > input.size())
		return RESULT_ERR_INVALID_ARG; // not enough data available

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	value = 0;
	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0) {
			if (ch == m_dataType.replacement) {
				value = m_dataType.replacement;
				return RESULT_OK;
			}
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_INVALID_ARG; // invalid BCD

			ch = (ch >> 4) * 10 + (ch & 0x0f);
			value += ch * exp;
			exp *= 100;
		}
		else {
			value |= ch * exp;
			exp <<= 8;
		}
	}

	if ((m_dataType.flags & BCD) == 0) {
		value >>= m_bitOffset;
		if ((m_bitCount % 8) != 0)
			value &= (1 << m_bitCount) - 1;
	}

	return RESULT_OK;
}

result_t NumericDataField::writeRawValue(unsigned int value,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	if ((m_dataType.flags & BCD) == 0) {
		if ((m_bitCount % 8) != 0 && (value & ~((1 << m_bitCount) - 1)) != 0)
			return RESULT_ERR_INVALID_ARG;

		value <<= m_bitOffset;
	}
	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		if ((m_dataType.flags & BCD) != 0) {
			if (value == m_dataType.replacement)
				ch = m_dataType.replacement;
			else {
				ch = (value / exp) % 100;
				ch = ((ch / 10) << 4) | (ch % 10);
			}
			exp = exp * 100;
		}
		else {
			ch = (value / exp) & 0xff;
			exp = exp << 8;
		}
		if (offset == start && (m_bitCount % 8) != 0 && baseOffset + offset < output.size())
			output[baseOffset + offset] |= ch;
		else
			output[baseOffset + offset] = ch;
	}

	return RESULT_OK;
}


result_t NumberDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	if (divisor == 0)
		divisor = m_divisor;
	else
		divisor *= m_dataType.divisor;
	if (values.empty() == false) {
		if (divisor != 1)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

		fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));
	}
	else
		fields.push_back(new NumberDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, divisor));

	return RESULT_OK;
}

result_t NumberDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	unsigned int value = 0;
	int signedValue;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	if (value == m_dataType.replacement) {
		output << NULL_VALUE;
		return RESULT_OK;
	}

	bool negative = (m_dataType.flags & SIG) != 0 && (value & (1 << (m_bitCount - 1))) != 0;
	if (m_bitCount == 32) {
		if (negative == false) {
			if (m_divisor <= 1)
				output << static_cast<unsigned>(value);
			else
				output << setprecision((m_bitCount % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
				       << fixed << static_cast<float>(value / (float) m_divisor);
			return RESULT_OK;
		}
		signedValue = (int) value; // negative signed value
	}
	else if (negative) // negative signed value
		signedValue = (int) value - (1 << m_bitCount);
	else
		signedValue = (int) value;

	if (m_divisor <= 1)
		output << static_cast<int>(signedValue);
	else
		output << setprecision((m_bitCount % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
		       << fixed << static_cast<float>(signedValue / (float) m_divisor);

	return RESULT_OK;
}

result_t NumberDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	unsigned int value;

	const char* str = input.str().c_str();
	if (isIgnored() == true || strcasecmp(str, NULL_VALUE) == 0)
		value = m_dataType.replacement; // replacement value
	else if (str == NULL || *str == 0)
		return RESULT_ERR_INVALID_ARG; // input too short
	else {
		char* strEnd = NULL;
		if (m_divisor <= 1) {
			if ((m_dataType.flags & SIG) != 0) {
				int signedValue = strtol(str, &strEnd, 10);
				if (signedValue < 0 && m_bitCount != 32)
					value = (unsigned int) (signedValue + (1 << m_bitCount));
				else
					value = (unsigned int) signedValue;
			}
			else
				value = strtoul(str, &strEnd, 10);
			if (strEnd == NULL || *strEnd != 0)
				return RESULT_ERR_INVALID_ARG; // invalid value
		}
		else {
			char* strEnd = NULL;
			double dvalue = strtod(str, &strEnd);
			if (strEnd == NULL || *strEnd != 0)
				return RESULT_ERR_INVALID_ARG; // invalid value
			dvalue = round(dvalue * m_divisor);
			if ((m_dataType.flags & SIG) != 0) {
				if (dvalue < -(1LL << (8 * m_length)) || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_INVALID_ARG; // value out of range
				if (dvalue < 0 && m_bitCount != 32)
					value = (unsigned int) (dvalue + (1 << m_bitCount));
				else
					value = (unsigned int) dvalue;
			}
			else {
				if (dvalue < 0.0 || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_INVALID_ARG; // value out of range
				value = (unsigned int) dvalue;
			}
		}

		if ((m_dataType.flags & SIG) != 0) { // signed value
			if ((value & (1 << (m_bitCount - 1))) != 0) { // negative signed value
				if (value < m_dataType.minValueOrLength)
					return RESULT_ERR_INVALID_ARG; // value out of range
			}
			else if (value > m_dataType.maxValueOrLength)
				return RESULT_ERR_INVALID_ARG; // value out of range
		}
		else if (value < m_dataType.minValueOrLength || value > m_dataType.maxValueOrLength)
			return RESULT_ERR_INVALID_ARG; // value out of range
	}

	return writeRawValue(value, baseOffset, output);
}


result_t ValueListDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	if (divisor != 0 && divisor != 1)
		return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

	if (values.empty() == false) {
		if (values.begin()->first < m_dataType.minValueOrLength
			|| values.rbegin()->first > m_dataType.maxValueOrLength)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
	}
	else
		values = m_values;

	fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));

	return RESULT_OK;
}

result_t ValueListDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	map<unsigned int, string>::iterator it = m_values.find(value);
	if (it != m_values.end()) {
		output << it->second;
		return RESULT_OK;
	}

	if (value == m_dataType.replacement) {
		output << NULL_VALUE;
		return RESULT_OK;
	}

	return RESULT_ERR_INVALID_ARG; // value assignment not found
}

result_t ValueListDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	if (isIgnored() == true)
		return writeRawValue(m_dataType.replacement, baseOffset, output); // replacement value

	const char* str = input.str().c_str();

	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return writeRawValue(it->first, baseOffset, output);

	if (strcasecmp(str, NULL_VALUE) == 0)
		return writeRawValue(m_dataType.replacement, baseOffset, output); // replacement value

	return RESULT_ERR_INVALID_ARG; // value assignment not found
}

DataFieldSet::~DataFieldSet()
{
	while (m_fields.empty() == false) {
		delete m_fields.back();
		m_fields.pop_back();
	}
}

unsigned char DataFieldSet::getLength(PartType partType)
{
	unsigned char length = 0;

	bool previousFullByteOffset[] = { true, true, true, true };

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->getPartType() == partType) {
			if (previousFullByteOffset[partType] == false && field->hasFullByteOffset(false) == false)
				length--;

			length += field->getLength(partType);

			previousFullByteOffset[partType] = field->hasFullByteOffset(true);
		}
	}

	return length;
}

result_t DataFieldSet::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // value list not allowed in set derive

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result = (*it)->derive("", "", "", partType,  divisor, values, fields);
		if (result != RESULT_OK)
			return result;
	}

	return RESULT_OK;
}

result_t DataFieldSet::read(SymbolString& masterData, unsigned char masterOffset,
		SymbolString& slaveData, unsigned char slaveOffset,
		ostringstream& output, bool verbose, char separator)
{
	if (verbose)
		output << m_name << "={ ";

	bool first = true;
	unsigned char offsets[3];
	memset(offsets, 0, sizeof(offsets));
	offsets[pt_masterData] = masterOffset;
	offsets[pt_slaveData] = slaveOffset;
	bool previousFullByteOffset[] = { true, true, true };
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		bool ignored = field->isIgnored();
		PartType partType = field->getPartType();

		if (ignored == false) {
			if (first)
				first = false;
			else
				output << separator;
		}
		if (previousFullByteOffset[partType] == false && field->hasFullByteOffset(false) == false)
			offsets[partType]--;

		result_t result = field->read(masterData, offsets[pt_masterData], slaveData, offsets[pt_slaveData], output, verbose, separator);

		if (result != RESULT_OK)
			return result;

		offsets[partType] += field->getLength(partType);

		previousFullByteOffset[partType] = field->hasFullByteOffset(true);
	}

	if (verbose == true) {
		if (m_comment.length() > 0)
			output << " [" << m_comment << "]";
		output << "}";
	}

	return RESULT_OK;
}

result_t DataFieldSet::write(istringstream& input,
		SymbolString& masterData, unsigned char masterOffset,
		SymbolString& slaveData, unsigned char slaveOffset,
		char separator)
{
	string token;

	unsigned char offsets[3];
	memset(offsets, 0, sizeof(offsets));
	offsets[pt_masterData] = masterOffset;
	offsets[pt_slaveData] = slaveOffset;
	bool previousFullByteOffset[] = { true, true, true };
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		bool ignored = field->isIgnored();
		PartType partType = field->getPartType();

		if (previousFullByteOffset[partType] == false && field->hasFullByteOffset(false) == false)
			offsets[partType]--;

		result_t result;
		if (m_fields.size() > 1) {
			if (ignored == true)
				token.clear();
			else if (getline(input, token, separator) == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete

			istringstream single(token);
			result = (*it)->write(single, masterData, offsets[pt_masterData], slaveData, offsets[pt_slaveData], separator);
		}
		else
			result = (*it)->write(input, masterData, offsets[pt_masterData], slaveData, offsets[pt_slaveData], separator);

		if (result != RESULT_OK)
			return result;

		offsets[partType] += field->getLength(partType);
		previousFullByteOffset[partType] = field->hasFullByteOffset(true);
	}

	return RESULT_OK;
}


void DataFieldTemplates::clear()
{
	for (map<string, DataField*>::iterator it=m_fieldsByName.begin(); it!=m_fieldsByName.end(); it++) {
		delete it->second;
		it->second = NULL;
	}
	m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(DataField* field, bool replace)
{
	string name = field->getName();
	map<string, DataField*>::iterator it = m_fieldsByName.find(name);
	if (it != m_fieldsByName.end()) {
		if (replace == false)
			return RESULT_ERR_DUPLICATE; // duplicate key

		delete it->second;
		it->second = field;

		return RESULT_OK;
	}

	m_fieldsByName[name] = field;

	return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(vector<string>& row, void* arg, vector< vector<string> >* defaults)
{
	DataField* field = NULL;
	vector<string>::iterator it = row.begin();
	result_t result = DataField::create(it, row.end(), this, field);
	if (result != RESULT_OK)
		return result;

	result = add(field);
	if (result != RESULT_OK)
		delete field;

	return result;
}

DataField* DataFieldTemplates::get(const string name)
{
	map<string, DataField*>::const_iterator ref = m_fieldsByName.find(name);
	if (ref == m_fieldsByName.end())
		return NULL;

	return ref->second;
}


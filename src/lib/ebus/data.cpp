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

#include "data.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>

using namespace std;

static const dataType_t stringDataType = {
	"STR",MAX_LEN*8,bt_str, ADJ,        ' ',          1,          0,    0  // >= 1 byte character string filled up with space
};

static const dataType_t pinDataType = {
	"PIN", 16, bt_num, FIX|BCD|REV,  0xffff,          0,     0x9999,    1 // unsigned decimal in BCD, 0000 - 9999 (fixed length)
};

static const dataType_t uchDataType = {
	"UCH",  8, bt_num,     LST,        0xff,          0,       0xfe,    1 // unsigned integer, 0 - 254
};

/** the known data field types. */
static const dataType_t dataTypes[] = {
	{"IGN",MAX_LEN*8,bt_str, IGN|ADJ,     0,          1,          0,    0}, // >= 1 byte ignored data
	stringDataType,
	{"HEX",MAX_LEN*8,bt_hexstr,  ADJ,    0,           2,         47,    0}, // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	{"BDA", 32, bt_dat,     BCD,       0xff,         10,         10,    0}, // date with weekday in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is weekday Mon=0x00 - Sun=0x06, replacement 0xff)
	{"BDA", 24, bt_dat,     BCD,       0xff,         10,         10,    0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99, replacement 0xff)
	{"HDA", 32, bt_dat,       0,       0xff,         10,         10,    0}, // date with weekday, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x1f,0x0c,WW,0x63, WW is weekday Mon=0x01 - Sun=0x07, replacement 0xff)
	{"HDA", 24, bt_dat,       0,       0xff,         10,         10,    0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x1f,0x0c,0x63, replacement 0xff)
	{"BTI", 24, bt_tim, BCD|REV|REQ,      0,          8,          8,    0}, // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	{"HTI", 24, bt_tim,     REQ,          0,          8,          8,    0}, // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x17,0x3b,0x3b)
	{"VTI", 24, bt_tim,     REV,       0x63,          8,          8,    0}, // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x3b,0x3b,0x17, replacement 0x63) [Vaillant type]
	{"HTM", 16, bt_tim,     REQ,          0,          5,          5,    0}, // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x17,0x3b)
	{"VTM", 16, bt_tim,     REV,       0xff,          5,          5,    0}, // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x3b,0x17, replacement 0xff) [Vaillant type]
	{"TTM",  8, bt_tim,       0,       0x90,          5,          5,   10}, // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	{"TTH",  8, bt_tim,       0,          0,          5,          5,   30}, // truncated time (only multiple of 30 minutes), 00:30 - 24:00 (minutes div 30 + hour * 2 as integer)
	{"BDY",  8, bt_num, DAY|LST,       0x07,          0,          6,    1}, // weekday, "Mon" - "Sun" (0x00 - 0x06) [eBUS type]
	{"HDY",  8, bt_num, DAY|LST,       0x00,          1,          7,    1}, // weekday, "Mon" - "Sun" (0x01 - 0x07) [Vaillant type]
	{"BCD",  8, bt_num, BCD|LST,       0xff,          0,       0x99,    1}, // unsigned decimal in BCD, 0 - 99
	{"BCD", 16, bt_num, BCD|LST,     0xffff,          0,     0x9999,    1}, // unsigned decimal in BCD, 0 - 9999
	{"BCD", 24, bt_num, BCD|LST,   0xffffff,          0,   0x999999,    1}, // unsigned decimal in BCD, 0 - 999999
	{"BCD", 32, bt_num, BCD|LST, 0xffffffff,          0, 0x99999999,    1}, // unsigned decimal in BCD, 0 - 99999999
	{"HCD", 32, bt_num, HCD|BCD|REQ,      0,          0, 0x63636363,    1}, // unsigned decimal in HCD, 0 - 99999999
	pinDataType,
	uchDataType,
	{"SCH",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1}, // signed integer, -127 - +127
	{"D1B",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1}, // signed integer, -127 - +127
	{"D1C",  8, bt_num,       0,       0xff,       0x00,       0xc8,    2}, // unsigned number (fraction 1/2), 0 - 100 (0x00 - 0xc8, replacement 0xff)
	{"D2B", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,  256}, // signed number (fraction 1/256), -127.99 - +127.99
	{"D2C", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,   16}, // signed number (fraction 1/16), -2047.9 - +2047.9
	{"FLT", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff, 1000}, // signed number (fraction 1/1000), -32.767 - +32.767, little endian
	{"FLR", 16, bt_num, SIG|REV,     0x8000,     0x8001,     0x7fff, 1000}, // signed number (fraction 1/1000), -32.767 - +32.767, big endian
	{"UIN", 16, bt_num,     LST,     0xffff,          0,     0xfffe,    1}, // unsigned integer, 0 - 65534, little endian
	{"UIR", 16, bt_num, LST|REV,     0xffff,          0,     0xfffe,    1}, // unsigned integer, 0 - 65534, big endian
	{"SIN", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,    1}, // signed integer, -32767 - +32767, little endian
	{"SIR", 16, bt_num, SIG|REV,     0x8000,     0x8001,     0x7fff,    1}, // signed integer, -32767 - +32767, big endian
	{"ULG", 32, bt_num,     LST, 0xffffffff,          0, 0xfffffffe,    1}, // unsigned integer, 0 - 4294967294, little endian
	{"ULR", 32, bt_num, LST|REV, 0xffffffff,          0, 0xfffffffe,    1}, // unsigned integer, 0 - 4294967294, big endian
	{"SLG", 32, bt_num,     SIG, 0x80000000, 0x80000001, 0xffffffff,    1}, // signed integer, -2147483647 - +2147483647, little endian
	{"SLR", 32, bt_num, SIG|REV, 0x80000000, 0x80000001, 0xffffffff,    1}, // signed integer, -2147483647 - +2147483647, big endian
	{"BI0",  7, bt_num, ADJ|LST|REQ,      0,          0,       0xef,    0}, // bit 0 (up to 7 bits until bit 6)
	{"BI1",  7, bt_num, ADJ|LST|REQ,      0,          0,       0x7f,    1}, // bit 1 (up to 7 bits until bit 7)
	{"BI2",  6, bt_num, ADJ|LST|REQ,      0,          0,       0x3f,    2}, // bit 2 (up to 6 bits until bit 7)
	{"BI3",  5, bt_num, ADJ|LST|REQ,      0,          0,       0x1f,    3}, // bit 3 (up to 5 bits until bit 7)
	{"BI4",  4, bt_num, ADJ|LST|REQ,      0,          0,       0x0f,    4}, // bit 4 (up to 4 bits until bit 7)
	{"BI5",  3, bt_num, ADJ|LST|REQ,      0,          0,       0x07,    5}, // bit 5 (up to 3 bits until bit 7)
	{"BI6",  2, bt_num, ADJ|LST|REQ,      0,          0,       0x03,    6}, // bit 6 (up to 2 bits until bit 7)
	{"BI7",  1, bt_num, ADJ|LST|REQ,      0,          0,       0x01,    7}, // bit 7
};

/** the maximum divisor value. */
#define MAX_DIVISOR 1000000000

/** the maximum value for value lists. */
#define MAX_VALUE (0xFFFFFFFFu)

/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

unsigned int parseInt(const char* str, int base, const unsigned int minValue, const unsigned int maxValue, result_t& result, unsigned int* length) {
	char* strEnd = NULL;

	unsigned long int ret = strtoul(str, &strEnd, base);

	if (strEnd == NULL || strEnd == str || *strEnd != 0) {
		result = RESULT_ERR_INVALID_NUM; // invalid value
		return 0;
	}

	if (minValue > ret || ret > maxValue) {
		result = RESULT_ERR_OUT_OF_RANGE; // invalid value
		return 0;
	}
	if (length != NULL)
		*length = (unsigned int)(strEnd - str);

	result = RESULT_OK;
	return (unsigned int)ret;
}

int parseSignedInt(const char* str, int base, const int minValue, const int maxValue, result_t& result, unsigned int* length) {
	char* strEnd = NULL;

	long int ret = strtol(str, &strEnd, base);

	if (strEnd == NULL || *strEnd != 0) {
		result = RESULT_ERR_INVALID_NUM; // invalid value
		return 0;
	}

	if (minValue > ret || ret > maxValue) {
		result = RESULT_ERR_OUT_OF_RANGE; // invalid value
		return 0;
	}
	if (length != NULL)
		*length = (unsigned int)(strEnd - str);

	result = RESULT_OK;
	return (int)ret;
}

void printErrorPos(ostream& out, vector<string>::iterator begin, const vector<string>::iterator end, vector<string>::iterator pos, string filename, size_t lineNo, result_t result)
{
	if (pos > begin)
		pos--;
	out << "Error reading \"" << filename << "\" line " << setw(0) << dec << static_cast<unsigned>(lineNo) << " field " << static_cast<unsigned>(1+pos.base()-begin.base()) << " value \"" << *pos << "\": " << getResultCode(result) << endl;
	out << "Erroneous item is here:" << endl;
	bool first = true;
	int cnt = 0;
	while (begin != end) {
		if (first)
			first = false;
		else {
			out << FIELD_SEPARATOR;
			if (begin <= pos) {
				cnt++;
			}
		}
		string item = *begin;
		size_t i = item.find(TEXT_SEPARATOR);
		if (i != string::npos) {
			do {
				item.replace(i, 1, TEXT_SEPARATOR_STR TEXT_SEPARATOR_STR);
				i = item.find(TEXT_SEPARATOR, i+sizeof(TEXT_SEPARATOR_STR)+sizeof(TEXT_SEPARATOR_STR));
			} while (i != string::npos);
			i = 0;
		} else {
			i = item.find(FIELD_SEPARATOR);
		}
		if (i!=string::npos) {
			out << TEXT_SEPARATOR << item << TEXT_SEPARATOR;
			if (begin < pos)
				cnt += 2;
			else if (begin == pos)
				cnt++;
		} else {
			out << item;
		}
		if (begin < pos)
			cnt += (unsigned int)(item).length();

		begin++;
	}
	out << endl;
	out << setw(cnt) << " " << setw(0) << "^" << endl;
}


result_t DataField::create(vector<string>::iterator& it,
		const vector<string>::iterator end,
		DataFieldTemplates* templates, DataField*& returnField,
		const bool isWriteMessage,
		const bool isTemplate, const bool isBroadcastOrMasterDestination,
		const unsigned char maxFieldLength)
{
	vector<SingleDataField*> fields;
	string firstName, firstComment;
	result_t result = RESULT_OK;
	if (it == end)
		return RESULT_ERR_EOF;

	while (it != end && result == RESULT_OK) {
		string unit, comment;
		PartType partType;
		int divisor = 0;
		bool hasPartStr = false;
		string token;

		// template: name,type[:len][,[divisor|values][,[unit][,[comment]]]]
		// std: name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
		const string name = *it++; // name
		if (it == end) {
			if (!name.empty())
				result = RESULT_ERR_MISSING_TYPE;
			break;
		}

		if (isTemplate)
			partType = pt_any;
		else {
			const char* partStr = (*it++).c_str(); // part
			hasPartStr = partStr[0] != 0;
			if (it == end) {
				if (!name.empty() || hasPartStr)
					result = RESULT_ERR_MISSING_TYPE;
				break;
			}
			if (isBroadcastOrMasterDestination
				|| (isWriteMessage && !hasPartStr)
				|| strcasecmp(partStr, "M") == 0) { // master data
				partType = pt_masterData;
			}
			else if ((!isWriteMessage && !hasPartStr)
				|| strcasecmp(partStr, "S") == 0) { // slave data
				partType = pt_slaveData;
			}
			else {
				result = RESULT_ERR_INVALID_PART;
				break;
			}
		}

		if (fields.empty()) {
			firstName = name;
			firstComment = comment;
		}

		const string typeStr = *it++; // type[:len]
		if (typeStr.empty()) {
			if (!name.empty() || hasPartStr)
				result = RESULT_ERR_MISSING_TYPE;
			break;
		}

		map<unsigned int, string> values;
		if (it != end) {
			const string divisorStr = *it++; // [divisor|values]
			if (!divisorStr.empty()) {
				if (divisorStr.find('=') == string::npos)
					divisor = parseSignedInt(divisorStr.c_str(), 10, -MAX_DIVISOR, MAX_DIVISOR, result);
				else { // '=' is present => value list found
					istringstream stream(divisorStr);
					while (getline(stream, token, VALUE_SEPARATOR) != 0) {
						FileReader::trim(token);
						const char* str = token.c_str();
						char* strEnd = NULL;
						unsigned long int id;
						if (strncasecmp(str, "0x", 2) == 0)
							id = strtoul(str+2, &strEnd, 16); // hexadecimal
						else
							id = strtoul(str, &strEnd, 10); // decimal
						if (strEnd == NULL || strEnd == str || id > MAX_VALUE) {
							result = RESULT_ERR_INVALID_LIST;
							break;
						}

						// remove blanks around '=' sign
						while (*strEnd == ' ') strEnd++;
						if (*strEnd != '=') {
							result = RESULT_ERR_INVALID_LIST;
							break;
						}
						string val = string(strEnd + 1);
						if (val.length()>0) {
							size_t val_start = val.find_first_not_of(' ');
							val=val.substr(val_start);
						}

						values[(unsigned int)id] = val;
					}
				}
				if (result != RESULT_OK)
					break;
			}
		}

		if (it == end)
			unit = "";
		else {
			const string str = *it++; // [unit]
			if (strcasecmp(str.c_str(), NULL_VALUE) == 0)
				unit = "";
			else
				unit = str;
		}

		if (it == end)
			comment = "";
		else {
			const string str = *it++; // [comment]
			if (strcasecmp(str.c_str(), NULL_VALUE) == 0)
				comment = "";
			else
				comment = str;
		}

		bool firstType = true;
		istringstream stream(typeStr);
		while (result == RESULT_OK && getline(stream, token, VALUE_SEPARATOR) != 0) {
			FileReader::trim(token);
			DataField* templ = templates->get(token);
			unsigned char length;
			if (templ == NULL) {
				size_t pos = token.find(LENGTH_SEPARATOR);
				if (pos == string::npos)
					length = 0; // no length specified
				else if (pos+2==token.length() && token[pos+1]=='*') {
					length = REMAIN_LEN;
				} else {
					length = (unsigned char)parseInt(token.substr(pos+1).c_str(), 10, 1, maxFieldLength, result);
					if (result != RESULT_OK)
						break;
				}
				string typeName = token.substr(0, pos);
				SingleDataField* add = NULL;
				result = SingleDataField::create(typeName.c_str(), length, firstType ? name : "", firstType ? comment : "", firstType ? unit : "", partType, divisor, values, add);
				if (add != NULL)
					fields.push_back(add);
				else if (result == RESULT_OK)
					result = RESULT_ERR_NOTFOUND; // type not found
			}
			else {
				bool lastType = stream.eof();
				result = templ->derive((firstType && lastType) ? name : "", firstType ? comment : "", firstType ? unit : "", partType, divisor, values, fields);
			}
			firstType = false;
		}
	}

	if (result != RESULT_OK) {
		while (!fields.empty()) { // cleanup already created fields
			delete fields.back();
			fields.pop_back();
		}
		return result;
	}

	if (fields.size() == 1)
		returnField = fields[0];
	else {
		returnField = new DataFieldSet(firstName, firstComment, fields);
	}
	return RESULT_OK;
}

void DataField::dumpString(ostream& output, const string str, const bool prependFieldSeparator)
{
	if (prependFieldSeparator)
		output << FIELD_SEPARATOR;
	if (str.find_first_of(FIELD_SEPARATOR) == string::npos) {
		output << str;
	} else {
		output << TEXT_SEPARATOR << str << TEXT_SEPARATOR;
	}
}

result_t SingleDataField::create(const char* typeNameStr, const unsigned char length,
	const string name, const string comment, const string unit,
	const PartType partType, int divisor, map<unsigned int, string> values,
	SingleDataField* &returnField)
{
	for (size_t i = 0; i < sizeof(dataTypes) / sizeof(dataType_t); i++) {
		const dataType_t* dataType = &dataTypes[i];
		if (strcasecmp(typeNameStr, dataType->name) != 0)
			continue;

		unsigned char bitCount = dataType->bitCount;
		unsigned char byteCount = (unsigned char)((bitCount + 7) / 8);
		if ((dataType->flags & ADJ) != 0) { // adjustable length
			if ((bitCount % 8) != 0) {
				if (length == 0)
					bitCount = 1; // default bit count: 1 bit
				else if (length <= bitCount)
					bitCount = length;
				else
					return RESULT_ERR_OUT_OF_RANGE; // invalid length

				byteCount = (unsigned char)((bitCount + 7) / 8);
			}
			else if (length == 0)
				byteCount = 1; //default byte count: 1 byte
			else if (length <= byteCount || length == REMAIN_LEN)
				byteCount = length;
			else
				return RESULT_ERR_OUT_OF_RANGE; // invalid length
		}
		else if (length > 0 && length != byteCount)
			continue; // check for another one with same name but different length

		switch (dataType->type)
		{
		case bt_str:
		case bt_hexstr:
		case bt_dat:
		case bt_tim:
			if (divisor != 0 || !values.empty())
				return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for string field
			returnField = new StringDataField(name, comment, unit, *dataType, partType, byteCount);
			return RESULT_OK;
		case bt_num:
			if (values.empty() && (dataType->flags & DAY) != 0) {
				for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++)
					values[dataType->minValue + i] = dayNames[i];
			}
			if (values.empty() || (dataType->flags & LST) == 0) {
				if (divisor == 0)
					divisor = 1;

				if ((dataType->bitCount % 8) == 0) {
					if (divisor < 0) {
						if (dataType->divisorOrFirstBit > 1)
							return RESULT_ERR_INVALID_ARG;

						if (dataType->divisorOrFirstBit < 0)
							divisor *= -dataType->divisorOrFirstBit;
					} else if (dataType->divisorOrFirstBit < 0) {
						if (divisor > 1)
							return RESULT_ERR_INVALID_ARG;

						if (divisor < 0)
							divisor *= -dataType->divisorOrFirstBit;
					} else
						divisor *= dataType->divisorOrFirstBit;

					if (-MAX_DIVISOR > divisor || divisor > MAX_DIVISOR)
						return RESULT_ERR_OUT_OF_RANGE;
				}

				returnField = new NumberDataField(name, comment, unit, *dataType, partType, byteCount, bitCount, divisor);
				return RESULT_OK;
			}
			if (values.begin()->first < dataType->minValue || values.rbegin()->first > dataType->maxValue)
				return RESULT_ERR_OUT_OF_RANGE;

			if (divisor != 0)
				return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
			//TODO add special field for fixed values (exactly one value in the list of values)
			returnField = new ValueListDataField(name, comment, unit, *dataType, partType, byteCount, bitCount, values);
			return RESULT_OK;
		}
	}
	return RESULT_ERR_NOTFOUND;
}

void SingleDataField::dump(ostream& output)
{
	output << setw(0) << dec; // initialize formatting
	dumpString(output, m_name, false);
	output << FIELD_SEPARATOR;
	if (m_partType == pt_masterData)
		output << "m";
	else if (m_partType == pt_slaveData)
		output << "s";
	dumpString(output, m_dataType.name);
}


result_t SingleDataField::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	if (partType != m_partType)
		return RESULT_EMPTY;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
		bool remainder = m_length==REMAIN_LEN && (m_dataType.flags & ADJ)!=0;
		if (!remainder && offset + m_length > data.size()) {
			return RESULT_ERR_INVALID_POS;
		}
		return RESULT_EMPTY;
	}
	return readRawValue(data, offset, output);
}

result_t SingleDataField::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, OutputFormat outputFormat, signed char outputIndex,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
		bool remainder = m_length==REMAIN_LEN && (m_dataType.flags & ADJ)!=0;
		if (!remainder && offset + m_length > data.size()) {
			return RESULT_ERR_INVALID_POS;
		}
		return RESULT_EMPTY;
	}

	if (outputFormat & OF_JSON) {
		if (leadingSeparator)
			output << ",";
		if (outputIndex>=0 || m_name.empty())
			output << "\n    \"" << static_cast<signed int>(outputIndex<0?0:outputIndex) << "\": {\"name\": \"" << m_name << "\"" << ", \"value\": ";
		else
			output << "\n    \"" << m_name << "\": {\"value\": ";
	} else {
		if (leadingSeparator)
			output << UI_FIELD_SEPARATOR;
		if (outputFormat & OF_VERBOSE)
			output << m_name << "=";
	}

	result_t result = readSymbols(data, offset, output, outputFormat);
	if (result != RESULT_OK)
		return result;

	if (outputFormat & OF_VERBOSE) {
		if (m_unit.length() > 0) {
			if (outputFormat & OF_JSON)
				output << ", \"unit\": \"" << m_unit << '"';
			else
				output << " " << m_unit;
		}
		if (m_comment.length() > 0) {
			if (outputFormat & OF_JSON)
				output << ", \"comment\": \"" << m_comment << '"';
			else
				output << " [" << m_comment << "]";
		}
	}
	if (outputFormat & OF_JSON)
		output << "}";
	return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator, unsigned char* length)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	return writeSymbols(input, offset, data, length);
}

StringDataField* StringDataField::clone()
{
	return new StringDataField(*this);
}

result_t StringDataField::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (divisor != 0 || !values.empty())
		return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for string field
	if (name.empty())
		name = m_name;
	if (comment.empty())
		comment = m_comment;
	if (unit.empty())
		unit = m_unit;

	fields.push_back(new StringDataField(name, comment, unit, m_dataType, partType, m_length));

	return RESULT_OK;
}

bool StringDataField::hasField(const char* fieldName, bool numeric)
{
	return !numeric && (fieldName==NULL || fieldName==m_name);
}

void StringDataField::dump(ostream& output)
{
	SingleDataField::dump(output);
	if ((m_dataType.flags & ADJ) != 0) {
		if (m_length==REMAIN_LEN)
			output << ":*";
		else
			output << ":" << static_cast<unsigned>(m_length);
	}
	output << FIELD_SEPARATOR; // no value list, no divisor
	dumpString(output, m_unit);
	dumpString(output, m_comment);
}

result_t StringDataField::readRawValue(SymbolString& input, const unsigned char offset, unsigned int& value)
{
	return RESULT_EMPTY;
}

result_t StringDataField::readSymbols(SymbolString& input, const unsigned char baseOffset,
		ostringstream& output, OutputFormat outputFormat)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch, last = 0, hour = 0;
	if (count==REMAIN_LEN && input.size()>baseOffset) {
		count = input.size()-baseOffset;
	} else if (baseOffset + count > input.size()) {
		return RESULT_ERR_INVALID_POS;
	}
	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	if (outputFormat & OF_JSON)
		output << '"';
	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		if (m_length == 4 && i == 2 && m_dataType.type == bt_dat)
			continue; // skip weekday in between
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0 && ((m_dataType.flags & REQ) != 0 || ch != m_dataType.replacement)) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD
			ch = (unsigned char)((ch >> 4) * 10 + (ch & 0x0f));
		}
		switch (m_dataType.type)
		{
		case bt_hexstr:
			if (i > 0)
				output << ' ';
			output << setw(2) << hex << setfill('0') << static_cast<unsigned>(ch);
			break;
		case bt_dat:
			if ((m_dataType.flags & REQ) == 0 && ch == m_dataType.replacement) {
				if (i + 1 != m_length) {
					output << NULL_VALUE << ".";
					break;
				}
				else if (last == m_dataType.replacement) {
					output << NULL_VALUE;
					break;
				}
			}
			if (i + 1 == m_length)
				output << (2000 + ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date
			else
				output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case bt_tim:
			if ((m_dataType.flags & REQ) == 0 && ch == m_dataType.replacement) {
				if (m_length == 1) { // truncated time
					output << NULL_VALUE << ":" << NULL_VALUE;
					break;
				}
				if (i > 0)
					output << ":";
				output << NULL_VALUE;
				break;
			}
			if (m_length == 1) { // truncated time
				if (i == 0) {
					ch = (unsigned char)(ch/(60/m_dataType.divisorOrFirstBit)); // convert to hours
					offset -= incr; // repeat for minutes
					count++;
				}
				else
					ch = (unsigned char)((ch % (60/m_dataType.divisorOrFirstBit)) * m_dataType.divisorOrFirstBit); // convert to minutes
			}
			if (i == 0) {
				if (ch > 24)
					return RESULT_ERR_OUT_OF_RANGE; // invalid hour
				hour = ch;
			}
			else if (ch > 59 || (hour == 24 && ch > 0))
				return RESULT_ERR_OUT_OF_RANGE; // invalid time
			if (i > 0)
				output << ":";
			output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch);
			break;
		default:
			if (ch < 0x20)
				ch = (unsigned char)m_dataType.replacement;
			output << setw(0) << dec << static_cast<char>(ch);
			break;
		}
		last = ch;
	}
	if (outputFormat & OF_JSON)
		output << '"';

	return RESULT_OK;
}

result_t StringDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output, unsigned char* length)
{
	size_t start = 0, count = m_length;
	bool remainder = count==REMAIN_LEN && (m_dataType.flags & ADJ)!=0;
	int incr = 1;
	unsigned int value = 0, last = 0, lastLast = 0;
	string token;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}
	if (isIgnored() && (m_dataType.flags & REQ) == 0) {
		if (remainder) {
			count = 1;
		}
		for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
			output[baseOffset + offset] = (unsigned char)m_dataType.replacement; // fill up with replacement
		}
		if (length!=NULL)
			*length = (unsigned char)count;
		return RESULT_OK;
	}
	result_t result;
	size_t i = 0, offset;
	for (offset = start; i < count; offset += incr, i++) {
		switch (m_dataType.type)
		{
		case bt_hexstr:
			while (!input.eof() && input.peek() == ' ')
				input.get();
			if (input.eof()) { // no more digits
				value = m_dataType.replacement; // fill up with replacement
			} else {
				token.clear();
				token.push_back((unsigned char)input.get());
				if (input.eof())
					return RESULT_ERR_INVALID_NUM; // too short hex value
				token.push_back((unsigned char)input.get());
				if (input.eof())
					return RESULT_ERR_INVALID_NUM; // too short hex value

				value = parseInt(token.c_str(), 16, 0, 0xff, result);
				if (result != RESULT_OK)
					return result; // invalid hex value
			}
			break;
		case bt_dat:
			if (m_length == 4 && i == 2)
				continue; // skip weekday in between
			if (input.eof() || getline(input, token, '.') == 0)
				return RESULT_ERR_EOF; // incomplete
			if ((m_dataType.flags & REQ) == 0 && strcmp(token.c_str(), NULL_VALUE) == 0) {
				value = m_dataType.replacement;
				break;
			}
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
						return RESULT_ERR_INVALID_NUM; // invalid date
					unsigned char daysSinceSunday = (unsigned char)t.tm_wday; // Sun=0
					if ((m_dataType.flags & BCD) != 0)
						output[baseOffset + offset - incr] = (unsigned char)((6+daysSinceSunday) % 7); // Sun=0x06
					else
						output[baseOffset + offset - incr] = (unsigned char)(daysSinceSunday==0 ? 7 : daysSinceSunday); // Sun=0x07
				}
				if (value >= 2000)
					value -= 2000;
				else if (value > 99)
					return RESULT_ERR_OUT_OF_RANGE; // invalid year
			}
			else if (value < 1 || (i == 0 && value > 31) || (i == 1 && value > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date part
			break;
		case bt_tim:
			if (input.eof() || getline(input, token, LENGTH_SEPARATOR) == 0)
				return RESULT_ERR_EOF; // incomplete
			if ((m_dataType.flags & REQ) == 0 && strcmp(token.c_str(), NULL_VALUE) == 0) {
				value = m_dataType.replacement;
				if (m_length == 1) { // truncated time
					if (i == 0) {
						last = value;
						offset -= incr; // repeat for minutes
						count++;
						continue;
					}
					if (last != m_dataType.replacement)
						return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				}
				break;
			}
			value = parseInt(token.c_str(), 10, 0, 59, result);
			if (result != RESULT_OK)
				return result; // invalid time part
			if ((i == 0 && value > 24) || (i > 0 && (last == 24 && value > 0) ))
				return RESULT_ERR_OUT_OF_RANGE; // invalid time part
			if (m_length == 1) { // truncated time
				if (i == 0) {
					last = value;
					offset -= incr; // repeat for minutes
					count++;
					continue;
				}

				if ((value % m_dataType.divisorOrFirstBit) != 0)
					return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				value = (last * 60 + value)/m_dataType.divisorOrFirstBit;
				if (value > 24 * 6)
					return RESULT_ERR_OUT_OF_RANGE; // invalid time
			}
			break;
		default:
			if (input.eof())
				value = m_dataType.replacement;
			else {
				value = input.get();
				if (input.eof() || value < 0x20)
					value = m_dataType.replacement;
			}
			break;
		}
		if (remainder && input.eof() && i > 0)
			break;

		lastLast = last;
		last = value;
		if ((m_dataType.flags & BCD) != 0 && ((m_dataType.flags & REQ) != 0 || value != m_dataType.replacement)) {
			if (value > 99)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD
			value = ((value / 10) << 4) | (value % 10);
		}
		if (value > 0xff)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
		output[baseOffset + offset] = (unsigned char)value;
	}

	if (!remainder && i < count)
		return RESULT_ERR_EOF; // input too short
	if (length!=NULL)
		*length = (unsigned char)((offset-start)*incr);
	return RESULT_OK;
}


bool NumericDataField::hasFullByteOffset(bool after)
{
	return m_length > 1 || (m_bitCount % 8) == 0
		|| (after && m_bitOffset + (m_bitCount % 8) >= 8);
}

bool NumericDataField::hasField(const char* fieldName, bool numeric)
{
	return numeric && (fieldName==NULL || fieldName==m_name);
}

void NumericDataField::dump(ostream& output)
{
	SingleDataField::dump(output);
	if ((m_dataType.flags & ADJ) != 0) {
		if ((m_dataType.bitCount % 8) != 0)
			output << ":" << static_cast<unsigned>(m_bitCount);
		else
			output << ":" << static_cast<unsigned>(m_length);
	}
	output << FIELD_SEPARATOR;
}

result_t NumericDataField::readRawValue(SymbolString& input,
		unsigned char baseOffset, unsigned int& value)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch;

	if (baseOffset + m_length > input.size())
		return RESULT_ERR_INVALID_POS; // not enough data available

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	value = 0;
	unsigned int exp = 1;
	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0) {
			if ((m_dataType.flags & REQ) == 0 && ch == (m_dataType.replacement & 0xff)) {
				value = m_dataType.replacement;
				return RESULT_OK;
			}
			if ((m_dataType.flags & HCD) == 0) {
				if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
					return RESULT_ERR_OUT_OF_RANGE; // invalid BCD

				ch = (unsigned char)((ch >> 4) * 10 + (ch & 0x0f));
			} else if (ch > 0x63)
				return RESULT_ERR_OUT_OF_RANGE; // invalid HCD
			value += ch * exp;
			exp *= 100;
		} else {
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
		unsigned char baseOffset, SymbolString& output, unsigned char* length)
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
			return RESULT_ERR_OUT_OF_RANGE;

		value <<= m_bitOffset;
	}
	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		if ((m_dataType.flags & BCD) != 0) {
			if ((m_dataType.flags & REQ) == 0 && value == m_dataType.replacement)
				ch = m_dataType.replacement & 0xff;
			else {
				ch = (unsigned char)((value / exp) % 100);
				if ((m_dataType.flags & HCD) == 0)
					ch = (unsigned char)(((ch / 10) << 4) | (ch % 10));
			}
			exp *= 100;
		} else {
			ch = (value / exp) & 0xff;
			exp <<= 8;
		}
		if (offset == start && (m_bitCount % 8) != 0 && baseOffset + offset < output.size())
			output[baseOffset + offset] |= ch;
		else
			output[baseOffset + offset] = ch;
	}
	if (length!=NULL)
		*length = m_length;
	return RESULT_OK;
}


NumberDataField::NumberDataField(const string name, const string comment,
		const string unit, const dataType_t dataType, const PartType partType,
		const unsigned char length, const unsigned char bitCount,
		const int divisor)
	: NumericDataField(name, comment, unit, dataType, partType, length, bitCount,
			(unsigned char)((dataType.bitCount % 8) == 0 ? 0 : dataType.divisorOrFirstBit)),
	m_divisor(divisor), m_precision(0)
{
	if (divisor > 1)
		for (unsigned int exp = 1; exp < MAX_DIVISOR; exp *= 10, m_precision++)
			if (exp >= (unsigned int)divisor)
				break;
}

NumberDataField* NumberDataField::clone()
{
	return new NumberDataField(*this);
}

result_t NumberDataField::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (name.empty())
		name = m_name;
	if (comment.empty())
		comment = m_comment;
	if (unit.empty())
		unit = m_unit;
	if (!values.empty()) {
		if (divisor != 0 || m_divisor != 1)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

		fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));
	}
	else {
		if (divisor == 0)
			divisor = m_divisor;
		else if ((m_dataType.bitCount % 8) == 0) {
			if (divisor < 0) {
				if (m_divisor > 1)
					return RESULT_ERR_INVALID_ARG;

				if (m_divisor < 0)
					divisor *= -m_divisor;
			} else if (m_divisor < 0) {
				if (divisor > 1)
					return RESULT_ERR_INVALID_ARG;

				if (divisor < 0)
					divisor *= -m_divisor;
			} else
				divisor *= m_divisor;

			if (-MAX_DIVISOR > divisor || divisor > MAX_DIVISOR) {
				return RESULT_ERR_OUT_OF_RANGE;
			}
		}
		fields.push_back(new NumberDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, divisor));
	}
	return RESULT_OK;
}

void NumberDataField::dump(ostream& output)
{
	NumericDataField::dump(output);
	if ((m_dataType.bitCount % 8) == 0 && m_dataType.divisorOrFirstBit != m_divisor)
		output << static_cast<unsigned>(m_divisor / m_dataType.divisorOrFirstBit) << FIELD_SEPARATOR;
	dumpString(output, m_unit);
	dumpString(output, m_comment);
}

result_t NumberDataField::readSymbols(SymbolString& input, const unsigned char baseOffset,
		ostringstream& output, OutputFormat outputFormat)
{
	unsigned int value = 0;
	int signedValue;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	output << setw(0) << dec; // initialize output

	if ((m_dataType.flags & REQ) == 0 && value == m_dataType.replacement) {
		if (outputFormat & OF_JSON)
			output << "null";
		else
			output << NULL_VALUE;
		return RESULT_OK;
	}

	bool negative = (m_dataType.flags & SIG) != 0 && (value & (1 << (m_bitCount - 1))) != 0;
	if (m_bitCount == 32) {
		if (!negative) {
			if (m_divisor < 0)
				output << static_cast<float>((float)value * (float)(-m_divisor));
			else if (m_divisor <= 1)
				output << static_cast<unsigned>(value);
			else
				output << setprecision(m_precision)
				       << fixed << static_cast<float>((float)value / (float)m_divisor);
			return RESULT_OK;
		}
		signedValue = (int) value; // negative signed value
	}
	else if (negative) // negative signed value
		signedValue = (int) value - (1 << m_bitCount);
	else
		signedValue = (int) value;

	if (m_divisor < 0)
		output << static_cast<float>((float)signedValue * (float)(-m_divisor));
	else if (m_divisor <= 1) {
		if ((m_dataType.flags & (FIX|BCD)) == (FIX|BCD)) {
			if (outputFormat & OF_JSON) {
				output << '"';
				output << setw(m_length * 2) << setfill('0');
				output << '"';
				return RESULT_OK;
			}
			output << setw(m_length * 2) << setfill('0');
		}
		output << static_cast<int>(signedValue) << setw(0);
	}
	else
		output << setprecision(m_precision)
		       << fixed << static_cast<float>((float)signedValue / (float)m_divisor);

	return RESULT_OK;
}

result_t NumberDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output, unsigned char* length)
{
	unsigned int value;

	const char* str = input.str().c_str();
	if ((m_dataType.flags & REQ) == 0 && (isIgnored() || strcasecmp(str, NULL_VALUE) == 0))
		value = m_dataType.replacement; // replacement value
	else if (str == NULL || *str == 0)
		return RESULT_ERR_EOF; // input too short
	else {
		char* strEnd = NULL;
		if (m_divisor == 1) {
			if ((m_dataType.flags & SIG) != 0) {
				long int signedValue = strtol(str, &strEnd, 10);
				if (signedValue < 0 && m_bitCount != 32)
					value = (unsigned int)(signedValue + (1 << m_bitCount));
				else
					value = (unsigned int)signedValue;
			}
			else
				value = (unsigned int)strtoul(str, &strEnd, 10);
			if (strEnd == NULL || strEnd == str || (*strEnd != 0 && *strEnd != '.'))
				return RESULT_ERR_INVALID_NUM; // invalid value
		} else {
			char* strEnd = NULL;
			double dvalue = strtod(str, &strEnd);
			if (strEnd == NULL || strEnd == str || *strEnd != 0)
				return RESULT_ERR_INVALID_NUM; // invalid value
			if (m_divisor < 0)
				dvalue = round(dvalue / -m_divisor);
			else
				dvalue = round(dvalue * m_divisor);
			if ((m_dataType.flags & SIG) != 0) {
				if (dvalue < -(1LL << (8 * m_length)) || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				if (dvalue < 0 && m_bitCount != 32)
					value = (unsigned int)(dvalue + (1 << m_bitCount));
				else
					value = (unsigned int)dvalue;
			}
			else {
				if (dvalue < 0.0 || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				value = (unsigned int) dvalue;
			}
		}

		if ((m_dataType.flags & SIG) != 0) { // signed value
			if ((value & (1 << (m_bitCount - 1))) != 0) { // negative signed value
				if (value < m_dataType.minValue)
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
			}
			else if (value > m_dataType.maxValue)
				return RESULT_ERR_OUT_OF_RANGE; // value out of range
		}
		else if (value < m_dataType.minValue || value > m_dataType.maxValue)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
	}

	return writeRawValue(value, baseOffset, output, length);
}


ValueListDataField* ValueListDataField::clone()
{
	return new ValueListDataField(*this);
}

result_t ValueListDataField::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (name.empty())
		name = m_name;
	if (comment.empty())
		comment = m_comment;
	if (unit.empty())
		unit = m_unit;
	if (divisor != 0 && divisor != 1)
		return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

	if (!values.empty()) {
		if (values.begin()->first < m_dataType.minValue || values.rbegin()->first > m_dataType.maxValue)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
	}
	else
		values = m_values;

	fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));

	return RESULT_OK;
}

void ValueListDataField::dump(ostream& output)
{
	NumericDataField::dump(output);
	bool first = true;
	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++) {
		if (first)
			first = false;
		else
			output << VALUE_SEPARATOR;
		output << static_cast<unsigned>(it->first) << "=" << it->second;
	}
	dumpString(output, m_unit);
	dumpString(output, m_comment);
}

result_t ValueListDataField::readSymbols(SymbolString& input, const unsigned char baseOffset,
		ostringstream& output, OutputFormat outputFormat)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	map<unsigned int, string>::iterator it = m_values.find(value);
	if (it == m_values.end() && value != m_dataType.replacement) {
		// fall back to raw value in input
		output << setw(0) << dec << static_cast<int>(value);
		return RESULT_OK;
	}
	if (it == m_values.end()) {
		if (outputFormat & OF_JSON)
			output << "null";
		else if (value == m_dataType.replacement)
			output << NULL_VALUE;
	} else if (outputFormat & OF_NUMERIC)
		output << setw(0) << dec << static_cast<int>(value);
	else if (outputFormat & OF_JSON)
		output << '"' << it->second << '"';
	else
		output << it->second;
	return RESULT_OK;
}

result_t ValueListDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output, unsigned char* length)
{
	if (isIgnored())
		return writeRawValue(m_dataType.replacement, baseOffset, output, length); // replacement value

	const char* str = input.str().c_str();

	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return writeRawValue(it->first, baseOffset, output, length);

	if (strcasecmp(str, NULL_VALUE) == 0)
		return writeRawValue(m_dataType.replacement, baseOffset, output, length); // replacement value

	char* strEnd = NULL; // fall back to raw value in input
	unsigned int value;
	value = (unsigned int)strtoul(str, &strEnd, 10);
	if (strEnd == NULL || strEnd == str || (*strEnd != 0 && *strEnd != '.'))
		return RESULT_ERR_INVALID_NUM; // invalid value
	if (m_values.find(value) != m_values.end())
		return writeRawValue(value, baseOffset, output, length);

	return RESULT_ERR_NOTFOUND; // value assignment not found
}


DataFieldSet* DataFieldSet::s_identFields = NULL;

DataFieldSet* DataFieldSet::getIdentFields()
{
	if (s_identFields==NULL) {
		map<unsigned int, string> manufacturers;
		manufacturers[0x06] = "Dungs";
		manufacturers[0x0f] = "FH Ostfalia";
		manufacturers[0x10] = "TEM";
		manufacturers[0x11] = "Lamberti";
		manufacturers[0x14] = "CEB";
		manufacturers[0x15] = "Landis-Staefa";
		manufacturers[0x16] = "FERRO";
		manufacturers[0x17] = "MONDIAL";
		manufacturers[0x18] = "Wikon";
		manufacturers[0x19] = "Wolf";
		manufacturers[0x20] = "RAWE";
		manufacturers[0x30] = "Satronic";
		manufacturers[0x40] = "ENCON";
		manufacturers[0x50] = "Kromschroeder";
		manufacturers[0x60] = "Eberle";
		manufacturers[0x65] = "EBV";
		manufacturers[0x75] = "Graesslin";
		manufacturers[0x85] = "ebm-papst";
		manufacturers[0x95] = "SIG";
		manufacturers[0xa5] = "Theben";
		manufacturers[0xa7] = "Thermowatt";
		manufacturers[0xb5] = "Vaillant";
		manufacturers[0xc0] = "Toby";
		manufacturers[0xc5] = "Weishaupt";
		manufacturers[0xfd] = "ebusd.eu";
		vector<SingleDataField*> fields;
		fields.push_back(new ValueListDataField("MF", "", "", uchDataType, pt_slaveData, 1, 8, manufacturers));
		fields.push_back(new StringDataField("ID", "", "", stringDataType, pt_slaveData, 5));
		fields.push_back(new NumberDataField("SW", "", "", pinDataType, pt_slaveData, 2, 16, 1));
		fields.push_back(new NumberDataField("HW", "", "", pinDataType, pt_slaveData, 2, 16, 1));
		s_identFields = new DataFieldSet("ident", "", fields);
	}
	return s_identFields;
}

DataFieldSet::~DataFieldSet()
{
	while (!m_fields.empty()) {
		delete m_fields.back();
		m_fields.pop_back();
	}
}

DataFieldSet* DataFieldSet::clone()
{
	vector<SingleDataField*> fields;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		fields.push_back((*it)->clone());
	}
	return new DataFieldSet(m_name, m_comment, fields);
}

unsigned char DataFieldSet::getLength(PartType partType, unsigned char maxLength)
{
	unsigned char length = 0;

	bool previousFullByteOffset[] = { true, true, true, true };

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->getPartType() == partType) {
			if (!previousFullByteOffset[partType] && !field->hasFullByteOffset(false))
				length--;

			unsigned char fieldLength = field->getLength(partType, maxLength);
			if (fieldLength>=maxLength)
				maxLength = 0;
			else
				maxLength = (unsigned char)(maxLength-fieldLength);
			length = (unsigned char)(length + fieldLength);

			previousFullByteOffset[partType] = field->hasFullByteOffset(true);
		}
	}

	return length;
}

result_t DataFieldSet::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (!values.empty())
		return RESULT_ERR_INVALID_ARG; // value list not allowed in set derive
	bool first = true;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result = (*it)->derive("", first?comment:"", first?unit:"", partType, divisor, values, fields);
		if (result != RESULT_OK)
			return result;
		first = false;
	}

	return RESULT_OK;
}

bool DataFieldSet::hasField(const char* fieldName, bool numeric)
{
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->hasField(fieldName, numeric)==0)
			return true;
	}
	return false;
}

void DataFieldSet::dump(ostream& output)
{
	bool first = true;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		if (first)
			first = false;
		else
			output << FIELD_SEPARATOR;
		(*it)->dump(output);
	}
}

result_t DataFieldSet::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result = field->read(partType, data, offset, output, fieldName, fieldIndex);

		if (result < RESULT_OK)
			return result;

		offset = (unsigned char)(offset + field->getLength(partType, (unsigned char)(data.size()-offset)));
		previousFullByteOffset = field->hasFullByteOffset(true);
		if (result != RESULT_EMPTY) {
			found = true;
		}
		if (findFieldIndex && fieldName == field->getName()) {
			if (fieldIndex == 0) {
				if (!found)
					return RESULT_ERR_NOTFOUND;
				break;
			}
			fieldIndex--;
		}
	}

	if (!found) {
		return RESULT_EMPTY;
	}

	return RESULT_OK;
}

result_t DataFieldSet::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, OutputFormat outputFormat, signed char outputIndex,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
	if (!m_uniqueNames && outputIndex<0)
		outputIndex = 0;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType) {
			if (outputIndex>=0 && !field->isIgnored())
				outputIndex++;
			continue;
		}
		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result = field->read(partType, data, offset, output, outputFormat, outputIndex, leadingSeparator, fieldName, fieldIndex);

		if (result < RESULT_OK)
			return result;

		offset = (unsigned char)(offset + field->getLength(partType, (unsigned char)(data.size()-offset)));
		previousFullByteOffset = field->hasFullByteOffset(true);
		if (result != RESULT_EMPTY) {
			found = true;
			leadingSeparator = true;
		}
		if (findFieldIndex && fieldName == field->getName()) {
			if (fieldIndex == 0) {
				if (!found)
					return RESULT_ERR_NOTFOUND;
				break;
			}
			fieldIndex--;
		}
		if (outputIndex>=0 && !field->isIgnored())
			outputIndex++;
	}

	if (!found) {
		return RESULT_EMPTY;
	}
	if (m_comment.length() > 0 && (outputFormat & OF_VERBOSE)) {
		if (outputFormat & OF_JSON)
			output << ",\"comment\": \"" << m_comment << '"';
		else
			output << " [" << m_comment << "]";
	}

	return RESULT_OK;
}

result_t DataFieldSet::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator, unsigned char* length)
{
	string token;

	bool previousFullByteOffset = true;
	unsigned char baseOffset = offset;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result;
		unsigned char fieldLength;
		if (m_fields.size() > 1) {
			if (field->isIgnored())
				token.clear();
			else if (getline(input, token, separator) == 0)
				token.clear();

			istringstream single(token);
			result = (*it)->write(single, partType, data, offset, separator, &fieldLength);
		}
		else
			result = (*it)->write(input, partType, data, offset, separator, &fieldLength);

		if (result != RESULT_OK)
			return result;

		offset = (unsigned char)(offset+fieldLength);
		previousFullByteOffset = field->hasFullByteOffset(true);
	}

	if (length!=NULL)
		*length = (unsigned char)(offset-baseOffset);
	return RESULT_OK;
}


DataFieldTemplates::DataFieldTemplates(DataFieldTemplates& other)
	: FileReader::FileReader(false)
{
	for (map<string, DataField*>::iterator it = other.m_fieldsByName.begin(); it != other.m_fieldsByName.end(); it++) {
		m_fieldsByName[it->first] = it->second->clone();
	}
}

void DataFieldTemplates::clear()
{
	for (map<string, DataField*>::iterator it = m_fieldsByName.begin(); it != m_fieldsByName.end(); it++) {
		delete it->second;
		it->second = NULL;
	}
	m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(DataField* field, string name, bool replace)
{
	if (name.length() == 0)
		name = field->getName();
	map<string, DataField*>::iterator it = m_fieldsByName.find(name);
	if (it != m_fieldsByName.end()) {
		if (!replace)
			return RESULT_ERR_DUPLICATE_NAME; // duplicate key

		delete it->second;
		it->second = field;

		return RESULT_OK;
	}

	m_fieldsByName[name] = field;

	return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
	vector< vector<string> >* defaults,
	const string& filename, unsigned int lineNo)
{
	vector<string>::iterator restart = begin;
	DataField* field = NULL;
	string name;
	if (begin != end) {
		size_t colon = begin->find(':');
		if (colon!=string::npos) {
			name = begin->substr(0, colon);
			begin->erase(0, colon+1);
		}
	}
	result_t result = DataField::create(begin, end, this, field, false, true, false);
	if (result != RESULT_OK)
		return result;

	result = add(field, name, true);
	if (result==RESULT_ERR_DUPLICATE_NAME)
		begin = restart+1; // mark name as invalid
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

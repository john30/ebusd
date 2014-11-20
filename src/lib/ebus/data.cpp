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
#include "decode.h"
#include "encode.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <typeinfo>
#include <math.h>

/** the known data field types. */
static const dataType_t dataTypes[] = {
	{"STR",16*8,bt_str,     ADJ,        ' ',          1,         16,    0, 0}, // >= 1 byte character string filled up with space
	{"HEX",16*8,bt_hexstr,  ADJ,          0,          2,         47,    0, 0}, // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	{"BDA", 32, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is ignored weekday)
	{"BDA", 24, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99)
	{"HDA", 32, bt_dat,       0,          0,         10,         10,    0, 0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is ignored weekday) // TODO remove duplicate of BDA
	{"HDA", 24, bt_dat,       0,          0,         10,         10,    0, 0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99) // TODO remove duplicate of BDA
	{"BTI", 24, bt_tim, BCD|REV,          0,          8,          8,    0, 0}, // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	{"HTM", 16, bt_tim,       0,          0,          5,          5,    0, 0}, // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x17,0x3b)
	{"TTM",  8, bt_tim,       0,       0x90,          5,          5,    0, 0}, // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	{"BDY",  8, bt_num, DAY|LST,       0x07,          0,          6,    1, 0}, // weekday, "Mon" - "Sun"
	{"HDY",  8, bt_num, DAY|LST,       0x00,          1,          7,    1, 0}, // weekday, "Mon" - "Sun"
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
	{"BI0",  1, bt_num,     LST,          0,          0,        0x1,    1, 0}, // single bit 0
	{"BI1",  1, bt_num,     LST,          0,          0,        0x1,    1, 1}, // single bit 1
	{"BI2",  1, bt_num,     LST,          0,          0,        0x1,    1, 2}, // single bit 2
	{"BI3",  1, bt_num,     LST,          0,          0,        0x1,    1, 3}, // single bit 3
	{"BI4",  1, bt_num,     LST,          0,          0,        0x1,    1, 4}, // single bit 4
	{"BI5",  1, bt_num,     LST,          0,          0,        0x1,    1, 5}, // single bit 5
	{"BI6",  1, bt_num,     LST,          0,          0,        0x1,    1, 6}, // single bit 6
	{"BI7",  1, bt_num,     LST,          0,          0,        0x1,    1, 7}, // single bit 7
	{"B01",  2, bt_num,     LST,          0,          0,        0x3,    1, 0}, // two bits 0-1
	{"B12",  2, bt_num,     LST,          0,          0,        0x3,    1, 1}, // two bits 1-2
	{"B23",  2, bt_num,     LST,          0,          0,        0x3,    1, 2}, // two bits 2-3
	{"B34",  2, bt_num,     LST,          0,          0,        0x3,    1, 3}, // two bits 3-4
	{"B45",  2, bt_num,     LST,          0,          0,        0x3,    1, 4}, // two bits 4-5
	{"B56",  2, bt_num,     LST,          0,          0,        0x3,    1, 5}, // two bits 5-6
	{"B67",  2, bt_num,     LST,          0,          0,        0x3,    1, 6}, // two bits 6-7
	{"B02",  3, bt_num,     LST,          0,          0,        0x7,    1, 0}, // three bits 0-2
	{"B13",  3, bt_num,     LST,          0,          0,        0x7,    1, 1}, // three bits 1-3
	{"B24",  3, bt_num,     LST,          0,          0,        0x7,    1, 2}, // three bits 2-4
	{"B35",  3, bt_num,     LST,          0,          0,        0x7,    1, 3}, // three bits 3-5
	{"B46",  3, bt_num,     LST,          0,          0,        0x7,    1, 4}, // three bits 4-6
	{"B57",  3, bt_num,     LST,          0,          0,        0x7,    1, 5}, // three bits 5-7
	{"B03",  4, bt_num,     LST,          0,          0,        0xf,    1, 0}, // four bits 0-3
	{"B14",  4, bt_num,     LST,          0,          0,        0xf,    1, 1}, // four bits 1-4
	{"B25",  4, bt_num,     LST,          0,          0,        0xf,    1, 2}, // four bits 2-5
	{"B36",  4, bt_num,     LST,          0,          0,        0xf,    1, 3}, // four bits 3-6
	{"B47",  4, bt_num,     LST,          0,          0,        0xf,    1, 4}, // four bits 4-7
	{"B04",  5, bt_num,     LST,          0,          0,       0x1f,    1, 0}, // five bits 0-4
	{"B15",  5, bt_num,     LST,          0,          0,       0x1f,    1, 1}, // five bits 1-5
	{"B26",  5, bt_num,     LST,          0,          0,       0x1f,    1, 2}, // five bits 2-6
	{"B37",  5, bt_num,     LST,          0,          0,       0x1f,    1, 3}, // five bits 3-7
	{"B05",  6, bt_num,     LST,          0,          0,       0x3f,    1, 0}, // six bits 0-5
	{"B16",  6, bt_num,     LST,          0,          0,       0x3f,    1, 1}, // six bits 1-6
	{"B27",  6, bt_num,     LST,          0,          0,       0x3f,    1, 2}, // six bits 2-7
	{"B06",  7, bt_num,     LST,          0,          0,       0x7f,    1, 0}, // seven bits 0-6
	{"B17",  7, bt_num,     LST,          0,          0,       0x7f,    1, 1}, // seven bits 1-7
};


/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

#define FIELD_SEPARATOR ';'
#define VALUE_SEPARATOR ','
#define NULL_VALUE "-"

result_t DataField::create(std::vector<std::string>::iterator& it,
		const std::vector<std::string>::iterator end,
		const std::map< std::string, DataField*> templates,
		DataField*& returnField, const bool isSetMessage,
		const unsigned char dstAddress)
{
	std::vector<SingleDataField*> fields;
	std::string firstName, firstComment;
	result_t result = RESULT_OK;
	while (it != end && result == RESULT_OK) {
		std::string unit, comment;
		PartType partType;
		unsigned int divisor = 0;
		unsigned char offset, length, maxPos = 16, offsetCnt = 0;
		const bool isTemplate = dstAddress == SYN;
		std::string token;
		if (it == end)
			break;

		// name;[pos];type[;[divisor|values][;[unit][;[comment]]]]
		const std::string name = *it++;
		if (it == end)
			break;

		const char* posStr = (*it++).c_str();
		if (it == end)
			break;

		if (fields.empty() == true) {
			firstName = name;
			firstComment = comment;
		}
		if (dstAddress == BROADCAST || isMaster(dstAddress)
			|| (isTemplate == false && isSetMessage == true && posStr[0] != 0 && posStr[0] <= '9')
			|| posStr[0] == 'm') { // master data
			partType = pt_masterData;
			if (posStr[0] == 'm')
				posStr++;
		}
		else if ((isTemplate == false && isSetMessage == false && posStr[0] != 0 && posStr[0] <= '9')
			|| posStr[0] == 's') { // slave data
			partType = pt_slaveData;
			if (posStr[0] == 's')
				posStr++;
		}
		else if (isTemplate) {
			partType = pt_template;
		}
		else {
			result = RESULT_ERR_INVALID_ARG;
			break;
		}

		if (posStr[0] == 0) {
			if (fields.empty() == false)
				offset = fields.back()->getNextOffset();
			else
				offset = 0;
			length = 0;
		}
		else {
			offset = 0;
			length = 0;
			std::istringstream stream(posStr);
			while (std::getline(stream, token, '-') != 0) {
				if (++offsetCnt > 2)
					return RESULT_ERR_INVALID_ARG; //invalid pos definition

				const char* start = token.c_str();
				char* end = NULL;
				unsigned int pos = strtoul(start, &end, 10) - 1; // 1-based
				if (end != start + strlen(start)) {
					result = RESULT_ERR_INVALID_ARG; // invalid pos definition
					break;
				}

				if (pos > maxPos) {
					result = RESULT_ERR_INVALID_ARG; // invalid pos definition
					break;
				}

				if (offsetCnt == 1)
					offset = pos;
				else if (pos >= offset)
					length = pos + 1 - offset;
				else { // wrong order e.g. 4-3
					length = offset - (pos + 1);
					offset = pos;
				}
			}
			if (result != RESULT_OK)
				break;
		}

		const char* typeStr = (*it++).c_str();
		if (typeStr[0] == 0) {
			break;
		}

		std::map<unsigned int, std::string> values;
		if (it != end) {
			std::string divisorStr = *it++;
			if (divisorStr.empty() == false) {
				if (divisorStr.find_first_not_of("0123456789") == std::string::npos) {
					const char* start = divisorStr.c_str();
					char* end = NULL;
					divisor = strtoul(start, &end, 10);
					if (end != start + strlen(start)) {
						result = RESULT_ERR_INVALID_ARG;
						break;
					}
				}
				else {
					std::istringstream stream(divisorStr);
					while (std::getline(stream, token, VALUE_SEPARATOR) != 0) {
						const char* start = token.c_str();
						char* end = NULL;
						unsigned int id = strtoul(start, &end, 10);
						if (end == NULL || end == start || *end != '=') {
							result = RESULT_ERR_INVALID_ARG;
							break;
						}

						values[id] = std::string(end + 1);
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

			if (unit.length() == 1 && unit[0] == '-')
				unit.clear();
		}

		if (it == end)
			comment = "";
		else {
			comment = *it++;
			if (comment.length() == 1 && comment[0] == '-')
				comment.clear();
		}

		// check for reference(s) to templates
		if (templates.empty() == false) {
			std::istringstream stream(typeStr);
			bool found = false;
			while (std::getline(stream, token, VALUE_SEPARATOR) != 0) {
				std::map< std::string, DataField*>::const_iterator ref = templates.find(token);
				if (ref == templates.end()) {
					if (found == false)
						break; // fallback to direct definition
					result = RESULT_ERR_INVALID_ARG; // cannot mix reference and direct definition
					break;
				}
				if (length > 1) {
					result = RESULT_ERR_INVALID_ARG; // different length not possible for derivation
					break;
				}
				found = true;
				result = ref->second->derive(name, comment, unit, partType, offset, divisor, values, fields);
				if (result != RESULT_OK)
					break;
				offset = fields.back()->getNextOffset();
			}
			if (found == true || result != RESULT_OK)
				break;
		}
		SingleDataField* add = NULL;
		for (size_t i = 0; result == RESULT_OK && add == NULL && i < sizeof(dataTypes) / sizeof(dataTypes[0]); i++) {
			dataType_t dataType = dataTypes[i];
			if (strcasecmp(typeStr, dataType.name) == 0) {
				unsigned char numBytes = (dataType.numBits + 7) / 8;
				unsigned char useLength = length;
				if ((dataType.flags & ADJ) != 0) {
					if (useLength == 0)
						useLength = 1; // minimum length defaults to 1
					else if (useLength > numBytes) {
						result = RESULT_ERR_INVALID_ARG; // invalid length
						break;
					}
				}
				else if (useLength == 0)
					useLength = numBytes;
				else if (useLength != numBytes)
					continue; // check for another one with same name but different length

				switch (dataType.type)
				{
				case bt_str:
				case bt_hexstr:
				case bt_dat:
				case bt_tim:
					add = new StringDataField(name, comment, unit, dataType, partType, offset, useLength);
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

						add = new NumberDataField(name, comment, unit, dataType, partType, offset, useLength, divisor);
						break;
					}
					if (values.begin()->first < dataType.minValueOrLength
							|| values.rbegin()->first > dataType.maxValueOrLength) {
						result = RESULT_ERR_INVALID_ARG;
						break;
					}

					add = new ValueListDataField(name, comment, unit, dataType, partType, offset, useLength, values);
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


unsigned char SingleDataField::getNextOffset()
{
	unsigned char offset = m_offset + m_length;
	if ((m_dataType.numBits % 8) != 0
			&& m_dataType.precisionOrFirstBit + (m_dataType.numBits % 8) < 8)
		offset--; // not all bits of last offset fully consumed

	return offset;
}

result_t SingleDataField::read(SymbolString& masterData, SymbolString& slaveData,
		std::ostringstream& output, bool verbose, char separator)
{
	SymbolString& input = m_partType == pt_masterData ? masterData : slaveData;
	unsigned char baseOffset;
	switch (m_partType)
	{
	case pt_masterData:
		baseOffset = 5; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		baseOffset = 1; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_ARG; // invalid part type
	}

	if (verbose)
		output << m_name << "=";

	result_t result = readSymbols(input, baseOffset, output);
	if (result != RESULT_OK)
		return result;

	if (verbose && m_unit.length() > 0)
		output << " " << m_unit;
	if (verbose && m_comment.length() > 0)
		output << " [" << m_comment << "]";

	return RESULT_OK;
}

result_t SingleDataField::write(std::istringstream& input, SymbolString& masterData,
		SymbolString& slaveData, char separator)
{
	SymbolString& output = m_partType == pt_masterData ? masterData : slaveData;
	unsigned char baseOffset;
	switch (m_partType)
	{
	case pt_masterData:
		baseOffset = 5; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		baseOffset = 1; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_ARG;
	}
	return writeSymbols(input, baseOffset, output);
}


result_t StringDataField::derive(std::string name, std::string comment,
		std::string unit, const PartType partType, unsigned char offset,
		unsigned int divisor, std::map<unsigned int, std::string> values,
		std::vector<SingleDataField*>& fields)
{
	if (m_partType != pt_template && partType == pt_template)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // cannot set values for string field
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	offset += m_offset;

	fields.push_back(new StringDataField(name, comment, unit, m_dataType, partType, offset, m_length));

	return RESULT_OK;
}

result_t StringDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, std::ostringstream& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch, last = 0;

	if (baseOffset + end > input.size()) {
		return RESULT_ERR_INVALID_ARG;
	}
	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	for (size_t offset = start, i = 0; offset != end; offset += incr, i++) {
		if (m_length == 4 && i == 2 && m_dataType.type == bt_dat)
			continue; // skip weekday in between
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_INVALID_ARG; // invalid BCD
			ch = (ch >> 4) * 10 + (ch & 0x0f);
		}
		switch (m_dataType.type)
		{
		case bt_hexstr:
			if (i > 0)
				output << ' ';
			output << std::nouppercase << std::setw(2) << std::hex
			       << std::setfill('0') << static_cast<unsigned>(ch);
			break;
		case bt_dat:
			if (i + 1 == m_length)
				output << (2000 + ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_INVALID_ARG; // invalid date
			else
				output << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case bt_tim:
			if (m_length == 1) { // truncated time
				if (i == 0) {
					ch /= 6; // hours
					offset -= incr; // repeat for minutes
				}
				else
					ch = (ch % 6) * 10; // minutes
			}
			if ((i == 0 && ch > 24) || (i > 0 && (ch > 59 || ( last == 24 && ch > 0) )))
				return RESULT_ERR_INVALID_ARG; // invalid time
			if (i > 0)
				output << ":";
			output << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ch);
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

result_t StringDataField::writeSymbols(std::istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	const char* str;
	char* strEnd;
	unsigned long int value = 0, last = 0;
	std::string token;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	size_t i = 0;
	for (size_t offset = start; offset != end; offset += incr, i++) {
		switch (m_dataType.type)
		{
		case bt_hexstr:
			while (input.peek() == ' ')
				input.get();
			if (input.eof() == true) // no more digits
				value = m_dataType.replacement; // fill up with replacement
			else {
				token.clear();
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_ARG;
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_ARG; // invalid hex value

				str = token.c_str();
				strEnd = NULL;
				value = strtoul(str, &strEnd, 16);
				if (strEnd != str + strlen(str))
					return RESULT_ERR_INVALID_ARG; // invalid hex value
			}
			break;
		case bt_dat:
			if (m_length == 4 && i == 2)
				continue; // skip weekday in between
			if (std::getline(input, token, '.') == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete
			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 10);
			if (strEnd != str + strlen(str))
				return RESULT_ERR_INVALID_ARG; // invalid date part
			if (i + 1 == m_length && value >= 2000)
				value -= 2000;
			else if (value < 1 || (i == 0 && value > 31) || (i == 1 && value > 12))
				return RESULT_ERR_INVALID_ARG; // invalid date part
			break;
		case bt_tim:
			if (std::getline(input, token, ':') == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete
			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 10);
			if (strEnd != str + strlen(str))
				return RESULT_ERR_INVALID_ARG; // invalid time part
			if ((i == 0 && value > 24) || (i > 0 && (value > 59 || ( last == 24 && value > 0) )))
				return RESULT_ERR_INVALID_ARG; // invalid time part
			if (m_length == 1) { // truncated time
				if (i == 0) {
					offset -= incr; // repeat for minutes
					last = value;
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
			value = input.get();
			if (input.eof() == true || value < 0x20)
				value = m_dataType.replacement;
			break;
		}
		if ((m_dataType.flags & BCD) != 0) {
			if (value > 99)
				return RESULT_ERR_INVALID_ARG; // invalid BCD
			value = (value / 10) << 4 | (value % 10);
		}
		if (value > 0xff)
			return RESULT_ERR_INVALID_ARG; // value out of range
		output[baseOffset + offset] = (unsigned char)value;
		last = value;
	}

	if (i < m_length)
		return RESULT_ERR_INVALID_ARG; // input too short

	return RESULT_OK;
}


result_t NumericDataField::readRawValue(SymbolString& input,
		unsigned char baseOffset, unsigned int& value)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch;

	if (baseOffset + end > input.size())
		return RESULT_ERR_INVALID_ARG; // not enough data available

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	value = 0;
	for (size_t offset = start, exp = 1; offset != end; offset += incr) {
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
		if ((m_dataType.numBits % 8) != 0)
			value &= (1 << m_dataType.numBits) - 1;
	}
	return RESULT_OK;
}

result_t NumericDataField::writeRawValue(unsigned int value,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	if ((m_dataType.flags & BCD) == 0) {
		if ((m_dataType.numBits % 8) != 0)
			value &= (1 << m_dataType.numBits) - 1;
		value <<= m_bitOffset;
	}
	for (size_t offset = start, exp = 1; offset != end; offset += incr) {
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
		if (offset == start && (m_dataType.numBits % 8) != 0 && baseOffset + offset < output.size())
			output[baseOffset + offset] |= ch;
		else
			output[baseOffset + offset] = ch;
	}

	return RESULT_OK;
}


result_t NumberDataField::derive(std::string name, std::string comment,
		std::string unit, const PartType partType, unsigned char offset,
		unsigned int divisor, std::map<unsigned int, std::string> values,
		std::vector<SingleDataField*>& fields)
{
	if (m_partType != pt_template && partType == pt_template)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	offset += m_offset;
	if (divisor == 0)
		divisor = m_divisor;
	else
		divisor *= m_dataType.divisor;
	if (values.empty() == false) {
		if (divisor != 1)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

		fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, offset, m_length, values));
	}
	else
		fields.push_back(new NumberDataField(name, comment, unit, m_dataType, partType, offset, m_length, divisor));

	return RESULT_OK;
}

result_t NumberDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, std::ostringstream& output)
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

	bool negative = (m_dataType.flags & SIG) != 0 && (value & (1 << (m_dataType.numBits - 1))) != 0;
	if (m_dataType.numBits == 32) {
		if (negative == false) {
			if (m_divisor <= 1)
				output << static_cast<unsigned>(value);
			else
				output << std::setprecision((m_dataType.numBits % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
				       << std::fixed << static_cast<float>(value / (float) m_divisor);
			return RESULT_OK;
		}
		signedValue = (int) value; // negative signed value
	}
	else if (negative) // negative signed value
		signedValue = (int) value - (1 << m_dataType.numBits);
	else
		signedValue = (int) value;

	if (m_divisor <= 1)
		output << static_cast<int>(signedValue);
	else
		output << std::setprecision((m_dataType.numBits % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
		       << std::fixed << static_cast<float>(signedValue / (float) m_divisor);

	return RESULT_OK;
}

result_t NumberDataField::writeSymbols(std::istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	unsigned int value;

	const char* str = input.str().c_str();
	size_t len = strlen(str);
	if (strcasecmp(str, NULL_VALUE) == 0)
		// replacement value
		value = m_dataType.replacement;
	else if (len == 0)
		return RESULT_ERR_INVALID_ARG; // input too short
	else {
		char* strEnd = NULL;
		if (m_divisor <= 1) {
			if ((m_dataType.flags & SIG) != 0) {
				int signedValue = strtol(str, &strEnd, 10);
				if (signedValue < 0 && m_dataType.numBits != 32)
					value = (unsigned int) (signedValue + (1 << m_dataType.numBits));
				else
					value = (unsigned int) signedValue;
			}
			else
				value = strtoul(str, &strEnd, 10);
			if (strEnd != str + len)
				return RESULT_ERR_INVALID_ARG; // invalid value
		}
		else {
			char* strEnd = NULL;
			double dvalue = strtod(str, &strEnd);
			if (strEnd != str + len)
				return RESULT_ERR_INVALID_ARG; // invalid value
			dvalue = round(dvalue * m_divisor);
			if ((m_dataType.flags & SIG) != 0) {
				if (dvalue < -(1LL << (8 * m_length)) || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_INVALID_ARG; // value out of range
				if (dvalue < 0 && m_dataType.numBits != 32)
					value = (unsigned int) (dvalue + (1 << m_dataType.numBits));
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
			if ((value & (1 << (m_dataType.numBits - 1))) != 0) { // negative signed value
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


result_t ValueListDataField::derive(std::string name, std::string comment,
		std::string unit, const PartType partType, unsigned char offset,
		unsigned int divisor, std::map<unsigned int, std::string> values,
		std::vector<SingleDataField*>& fields)
{
	if (m_partType != pt_template && partType == pt_template)
		return RESULT_ERR_INVALID_ARG; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	offset += m_offset;
	if (divisor != 0 && divisor != 1)
		return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

	if (values.empty() == false) {
		if (values.begin()->first < m_dataType.minValueOrLength
			|| values.rbegin()->first > m_dataType.maxValueOrLength)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
	}
	else
		values = m_values;

	fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, offset, m_length, values));

	return RESULT_OK;
}

result_t ValueListDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, std::ostringstream& output)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	std::map<unsigned int, std::string>::iterator it = m_values.find(value);
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

result_t ValueListDataField::writeSymbols(std::istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	const char* str = input.str().c_str();

	for (std::map<unsigned int, std::string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return writeRawValue(it->first, baseOffset, output);

	if (strcasecmp(str, NULL_VALUE) == 0) // replacement value
		return writeRawValue(m_dataType.replacement, baseOffset, output);

	return RESULT_ERR_INVALID_ARG; // value assignment not found
}

DataFieldSet::~DataFieldSet()
{
	while (m_fields.empty() == false) {
		delete m_fields.back();
		m_fields.pop_back();
	}
}

unsigned char DataFieldSet::getNextOffset()
{
	return m_fields.back()->getNextOffset();
}

result_t DataFieldSet::derive(std::string name, std::string comment,
		std::string unit, const PartType partType, unsigned char offset,
		unsigned int divisor, std::map<unsigned int, std::string> values,
		std::vector<SingleDataField*>& fields)
{
	if (values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // value list not allowed in set derive

	for (std::vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result = (*it)->derive("", "", "", partType, offset, divisor, values, fields);
		if (result != RESULT_OK)
			return result;
	}

	return RESULT_OK;
}

result_t DataFieldSet::read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
		bool verbose, char separator)
{
	if (verbose)
		output << m_name << "={ ";

	bool first = true;
	for (std::vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		if (first)
			first = false;
		else
			output << separator;

		result_t result = (*it)->read(masterData, slaveData, output, verbose);

		if (result != RESULT_OK)
			return result;
	}

	if (verbose) {
		if (m_comment.length() > 0)
			output << " [" << m_comment << "]";
		output << "}";
	}

	return RESULT_OK;
}

result_t DataFieldSet::write(std::istringstream& input, SymbolString& masterData, SymbolString& slaveData,
		char separator)
{
	std::string token;

	for (std::vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result;
		if (m_fields.size() > 1) {
			if (std::getline(input, token, separator) == 0)
				return RESULT_ERR_INVALID_ARG; // incomplete
			std::istringstream single(token);
			result = (*it)->write(single, masterData, slaveData);
		}
		else
			result = (*it)->write(input, masterData, slaveData);

		if (result != RESULT_OK)
			return result;
	}

	return RESULT_OK;
}

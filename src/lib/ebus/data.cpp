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

namespace libebus
{

/** the known data field types. */
static const dataType_t dataTypes[] = {
	{"STR", 16, bt_str,     ADJ,        ' ',          1,         16,    0}, // >= 1 byte character string filled up with space
	{"HEX", 16, bt_hexstr,  ADJ,          0,          2,         47,    0}, // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	{"BDA",  4, bt_dat,     BCD,          0,         10,         10,    0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is ignored weekday)
	{"BDA",  3, bt_dat,     BCD,          0,         10,         10,    0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99)
	{"HDA",  4, bt_dat,       0,          0,         10,         10,    0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is ignored weekday) // TODO remove duplicate of BDA
	{"HDA",  3, bt_dat,       0,          0,         10,         10,    0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99) // TODO remove duplicate of BDA
	{"BTI",  3, bt_tim, BCD|REV,          0,          8,          8,    0}, // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	{"TTM",  1, bt_tim,       0,          0,          5,          5,    0}, // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	{"BDY",  1, bt_num, DAY|LST,          0,          0,          6,    1}, // weekday, "Mon" - "Sun"
	{"HDY",  1, bt_num, DAY|LST,          0,          1,          7,    1}, // weekday, "Mon" - "Sun"
	{"BCD",  1, bt_num, BCD|LST,       0xff,          0,       0x99,    1}, // unsigned decimal in BCD, 0 - 99
	{"UCH",  1, bt_num,     LST,       0xff,          0,       0xff,    1}, // unsigned integer, 0 - 255
	{"SCH",  1, bt_num,     SIG,       0x80,       0x80,       0x7f,    1}, // signed integer, -128 - +127
	{"D1B",  1, bt_num,     SIG,       0x80,       0x81,       0x7f,    1}, // signed integer, -127 - +127
	{"D1C",  1, bt_num,       0,       0xff,       0x00,       0xc8,    2}, // unsigned number (fraction 1/2), 0 - 100 (0x00 - 0xc8, replacement 0xff)
	{"UIN",  2, bt_num,     LST,     0xffff,          0,     0xffff,    1}, // unsigned integer, 0 - 65535
	{"SIN",  2, bt_num,     SIG,     0x8000,     0x8000,     0x7fff,    1}, // signed integer, -32768 - +32767
	{"FLT",  2, bt_num,     SIG,     0x8000,     0x8000,     0x7fff, 1000}, // signed number (fraction 1/1000), -32.768 - +32.767
	{"D2B",  2, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,  256}, // signed number (fraction 1/256), -127.99 - +127.99
	{"D2C",  2, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,   16}, // signed number (fraction 1/16), -2047.9 - +2047.9
	{"ULG",  4, bt_num,     LST, 0xffffffff,          0, 0xffffffff,    1}, // unsigned integer, 0 - 4294967295
	{"SLG",  4, bt_num,     SIG, 0x80000000, 0x80000000, 0xffffffff,    1}, // signed integer, -2147483648 - +2147483647
};

/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

#define FIELD_SEPARATOR ';'
#define VALUE_SEPARATOR ','

result_t DataField::create(const unsigned char dstAddress, const bool isSetMessage,
		std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end,
		const std::map<std::string, DataField*> predefined, std::vector<DataField*>& fields,
		unsigned char& nextPos) {
	std::string name, unit, comment;
	PartType partType;
	unsigned int divisor;
	unsigned char baseOffset, offset, length, maxPos = 16, offsetCnt = 0;
	std::string token;

	if (it == end)
		return RESULT_ERR_EOF;

	name = *it++;
	if (it == end || name.empty() == true)
		return RESULT_ERR_EOF;

	const char* posStr = (*it++).c_str();
	if (it == end)  {
		// no more input: check for reference to predefined type
		if (posStr[0] == 0)
			return RESULT_ERR_EOF;

		std::map<std::string, DataField*>::const_iterator ref = predefined.find(name);
		if (ref != predefined.end()) {
			fields.push_back(ref->second);
			return RESULT_OK;
		}
		// check for sequence of reference to predefined type
		std::istringstream stream(name);
		while (std::getline(stream, token, VALUE_SEPARATOR) != 0) {
			ref = predefined.find(token);
			if (ref == predefined.end())
				return RESULT_ERR_INVALID_ARG;

			fields.push_back(ref->second);
		}
		return RESULT_OK;
	}
	if (dstAddress == BROADCAST
	|| isMaster(dstAddress)
	|| (isSetMessage == true && posStr[0] <= '9')
	|| posStr[0] == 'm') { // master data
		partType = pt_masterData;
		baseOffset = 5; // skip QQ ZZ PB SB NN
		if (posStr[0] == 'm')
			posStr++;
	} else if ((isSetMessage == false && posStr[0] <= '9')
	|| posStr[0] == 's') { // slave data
		partType = pt_slaveData;
		baseOffset = 1;
		if (posStr[0] == 's')
			posStr++;
	} else
		return RESULT_ERR_INVALID_ARG;

	if (posStr[0] == 0) {
		if (nextPos == 0)
			offset = baseOffset;
		else if (nextPos < baseOffset)
			return RESULT_ERR_INVALID_ARG; // missing pos definition
		else
			offset = nextPos;
		length = 0;
	} else {
		offset = 0;
		length = 0;
		std::istringstream stream(posStr);
		while (std::getline(stream, token, '-') != 0) {
			if (++offsetCnt > 2)
				return RESULT_ERR_INVALID_ARG; //invalid pos definition

			const char* start = token.c_str();
			char* end = NULL;
			unsigned int pos = strtoul(start, &end, 10)-1; // 1-based
			if (end != start+strlen(start))
				return RESULT_ERR_INVALID_ARG; // invalid pos definition

			if (baseOffset+pos > maxPos)
				return RESULT_ERR_INVALID_ARG; // invalid pos definition

			if (offsetCnt==1)
				offset = baseOffset+pos;
			else if (baseOffset+pos >= offset)
				length = baseOffset+pos+1-offset;
			else { // wrong order e.g. 4-3
				length = offset-(baseOffset+pos+1);
				offset = baseOffset+pos;
			}
		}
		if (offset < baseOffset)
			return RESULT_ERR_INVALID_ARG; //invalid pos definition
	}

	const char* typeStr = (*it++).c_str();

	std::map<unsigned int, std::string> values;
	if (it == end)
		divisor = 1;
	else {
		std::string divisorStr = *it++;
		if (divisorStr.empty() == true)
			divisor = 1;
		else if (divisorStr.find_first_not_of("0123456789") == std::string::npos) {
			const char* start = divisorStr.c_str();
			char* end = NULL;
			divisor = strtoul(start, &end, 10);
			if (end != start+strlen(start))
				return RESULT_ERR_INVALID_ARG;
		}
		else {
			divisor = 1;
			std::istringstream stream(divisorStr);
			while (std::getline(stream, token, VALUE_SEPARATOR) != 0) {
				const char* start = token.c_str();
				char* end = NULL;
				unsigned int id = strtoul(start, &end, 10);
				if (end == NULL || end == start || *end != '=')
					return RESULT_ERR_INVALID_ARG;

				values[id] = std::string(end+1);
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
//TODO derive more specific subtypes
	for (size_t i = 0; i < sizeof(dataTypes)/sizeof(dataTypes[0]); i++) {
		dataType_t dataType = dataTypes[i];
		if (strcasecmp(typeStr, dataType.name) == 0) {
			if ((dataType.flags&ADJ) != 0) {
				if (length == 0)
					length = 1; // minimum length defaults to 1
				else if (length > dataType.numBytes)
					return RESULT_ERR_INVALID_ARG; // invalid length
			}
			else if (length == 0)
				length = dataType.numBytes;
			else if (length != dataType.numBytes)
				continue; // check for another one with same name but different length

			switch (dataType.type) {
			case bt_str:
			case bt_hexstr:
			case bt_dat:
			case bt_tim:
				fields.push_back(new StringDataField(name, partType, offset, length, dataType, unit, comment));
				nextPos = offset+length;
				return RESULT_OK;
			case bt_num:
				if (values.empty() == true && (dataType.flags&DAY) != 0) {
					for (unsigned int i=0; i<sizeof(dayNames)/sizeof(dayNames[0]); i++)
						values[dataType.minValueOrLength + i] = dayNames[i];
				}
				if (values.empty() == true || (dataType.flags&LST) == 0) {
					fields.push_back(new NumberDataField(name, partType, offset, length, dataType, unit, comment, divisor));
					nextPos = offset+length;
					return RESULT_OK;
				}
				if (values.begin()->first < dataType.minValueOrLength)
					return RESULT_ERR_INVALID_ARG;
				std::map<unsigned int, std::string>::iterator end = values.end();
				end--;
				if (end->first > dataType.maxValueOrLength)
					return RESULT_ERR_INVALID_ARG;
				fields.push_back(new ValueListDataField(name, partType, offset, length, dataType, unit, comment, values));
				nextPos = offset+length;
				return RESULT_OK;
			}
		}
	}
	return RESULT_ERR_INVALID_ARG;
}

result_t DataField::read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output, bool verbose)
{
	SymbolString& input = m_partType == pt_masterData ? masterData : slaveData;
	switch (m_partType) {
	case pt_masterData:
		break;
	case pt_slaveData:
		break;
	default:
		return RESULT_ERR_INVALID_ARG; // invalid part type
	}
	if (verbose)
		output << m_name << "=";

	result_t result = readSymbols(input, output);
	if (result != RESULT_OK)
		return result;

	if (verbose && m_unit.length() > 0)
		output << " " << m_unit;
	if (verbose && m_comment.length() > 0)
		output << " [" << m_comment << "]";

	return RESULT_OK;
}

result_t DataField::write(const std::string& value, SymbolString& masterData, SymbolString& slaveData)
{
	SymbolString& output = m_partType == pt_masterData ? masterData : slaveData;
	switch (m_partType) {
	case pt_masterData:
	case pt_slaveData:
		break;
	default:
		return RESULT_ERR_INVALID_ARG;
	}
	std::istringstream input(value);
	return writeSymbols(input, output);
}



result_t StringDataField::readSymbols(SymbolString& input, std::ostringstream& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch;

	if (end > input.size())
		return RESULT_ERR_INVALID_ARG;

	if ((m_dataType.flags&REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	for (size_t pos = start, i = 0; pos != end; pos += incr, i++) {
		if (m_length == 4 && i == 2 && m_dataType.type == bt_dat)
			continue; // skip weekday in between
		ch = input[pos];
		if ((m_dataType.flags & BCD) != 0) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_INVALID_ARG; // invalid BCD
			ch = (ch >> 4) * 10 + (ch & 0x0f);
		}
		switch (m_dataType.type) {
		case bt_hexstr:
			if (i > 0)
				output << ' ';
			output << std::nouppercase << std::setw(2) << std::hex
			       << std::setfill('0') << static_cast<unsigned>(ch);
			break;
		case bt_dat:
			if (i + 1 == m_length)
				output << (2000+ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_INVALID_ARG; // invalid date
			else
				output << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case bt_tim:
			if (m_length == 1) { // truncated time
				if (i == 0) {
					ch /= 6; // hours
					pos -= incr; // repeat for minutes
				}
				else
					ch = (ch%6) * 10; // minutes
			}
			if ((i == 0 && ch > 23) || (i > 0 && ch > 59))
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
	}

	return RESULT_OK;
}

result_t StringDataField::writeSymbols(std::istringstream& input, SymbolString& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	const char* str;
	char* strEnd;
	unsigned long int value = 0, hours = 0;
	std::string token;

	if ((m_dataType.flags&REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	for (size_t pos = start, i = 0; pos != end; pos += incr, i++) {
		switch (m_dataType.type) {
		case bt_hexstr:
			while (input.peek()==' ')
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
				if (strEnd != str+strlen(str))
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
			if (strEnd != str+strlen(str))
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
			if (strEnd != str+strlen(str))
				return RESULT_ERR_INVALID_ARG; // invalid time part
			if ((i == 0 && value > 23) || (i > 0 && value > 59))
				return RESULT_ERR_INVALID_ARG; // invalid time part
			if (m_length == 1) { // truncated time
				if (i == 0) {
					pos -= incr; // repeat for minutes
					hours = value;
					continue;
				}
				if ((value % 10) != 0)
					return RESULT_ERR_INVALID_ARG; // invalid truncated time minutes
				value = hours*6 + (value / 10);
				if (value > 24*6)
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
			value = (value/10)<<4 | (value%10);
		}
		if (value > 0xff)
			return RESULT_ERR_INVALID_ARG; // value out of range
		output[pos] = (unsigned char)value;
	}

	return RESULT_OK;
}


result_t NumericDataField::readRawValue(SymbolString& input, unsigned int& value)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch;

	if (end > input.size())
		return RESULT_ERR_INVALID_ARG; // not enough data available

	if ((m_dataType.flags&REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	value = 0;
	for (size_t pos = start, exp = 1; pos != end; pos += incr) {
		ch = input[pos];
		if ((m_dataType.flags & BCD) != 0) {
			if (ch == m_dataType.replacement) {
				value = m_dataType.replacement;
				return RESULT_OK;
			}
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_INVALID_ARG; // invalid BCD

			ch = (ch >> 4) * 10 + (ch & 0x0f);
			value += ch*exp;
			exp = exp*100;
		}
		else {
			value |= ch*exp;
			exp = exp<<8;
		}
	}
	return RESULT_OK;
}

result_t NumericDataField::writeRawValue(unsigned int value, SymbolString& output)
{
	size_t start = m_offset, end = m_offset + m_length;
	int incr = 1;
	unsigned char ch;

	if ((m_dataType.flags&REV) != 0) { // reverted binary representation (most significant byte first)
		end = start - 1;
		start = m_offset + m_length - 1;
		incr = -1;
	}

	for (size_t pos = start, exp = 1; pos != end; pos += incr) {
		if ((m_dataType.flags & BCD) != 0) {
			if (value == m_dataType.replacement)
				ch = m_dataType.replacement;
			else {
				ch = (value/exp)%100;
				ch = ((ch/10)<<4) | (ch%10);
			}
			exp = exp*100;
		}
		else {
			ch = (value/exp)&0xff;
			exp = exp<<8;
		}
		output[pos] = ch;
	}

	return RESULT_OK;
}


result_t NumberDataField::readSymbols(SymbolString& input, std::ostringstream& output)
{
	unsigned int value = 0;
	int signedValue;

	result_t result = readRawValue(input, value);
	if (result != RESULT_OK)
		return result;

	if (value == m_dataType.replacement) {
		output << "-";
		return RESULT_OK;
	}
	bool negative = (m_dataType.flags&SIG) != 0 && (value & (1 << (m_dataType.numBytes*8 - 1))) != 0;
	if (m_dataType.numBytes == 4) {
		if (negative == false) {
			if (m_divisor <= 1)
				output << static_cast<unsigned>(value);
			else
				output << std::setprecision(3) << std::fixed << static_cast<float>(value / (float)m_divisor);
			return RESULT_OK;
		}
		signedValue = (int)value; // negative signed value
	}
	else
		if (negative) // negative signed value
			signedValue = (int)value - (1 << (m_dataType.numBytes*8));
		else
			signedValue = (int)value;

	if (m_divisor <= 1)
		output << static_cast<int>(signedValue);
	else
		output << std::setprecision(3) << std::fixed << static_cast<float>(signedValue / (float)m_divisor);

	return RESULT_OK;
}

result_t NumberDataField::writeSymbols(std::istringstream& input, SymbolString& output)
{
	unsigned int value;

	const char* str = input.str().c_str();
	if (strcasecmp(str, "-") == 0)
		// replacement value
		value = m_dataType.replacement;
	else {
		char* strEnd = NULL;
		if (m_divisor <= 1) {
			if ((m_dataType.flags&SIG) != 0) {
				int signedValue = strtol(str, &strEnd, 10);
				if (signedValue < 0 && m_dataType.numBytes != 4)
					value = (unsigned int)(signedValue + (1<<(m_dataType.numBytes*8)));
				else
					value = (unsigned int)signedValue;
			}
			else
				value = strtoul(str, &strEnd, 10);
			if (strEnd != str+strlen(str))
				return RESULT_ERR_INVALID_ARG; // invalid value
		}
		else {
			char* strEnd = NULL;
			double dvalue = strtod(str, &strEnd);
			if (strEnd != str+strlen(str))
				return RESULT_ERR_INVALID_ARG; // invalid value
			dvalue = dvalue * m_divisor + 0.5; // round
			if ((m_dataType.flags&SIG) != 0) {
				if (dvalue < -(1LL<<(8*m_length)) || dvalue >= (1LL<<(8*m_length)))
					return RESULT_ERR_INVALID_ARG; // value out of range
				if (dvalue < 0 && m_dataType.numBytes != 4)
					value = (unsigned int)(dvalue + (1<<(m_dataType.numBytes*8)));
				else
					value = (unsigned int)dvalue;
			}
			else {
				if (dvalue < 0.0 || dvalue >= (1LL<<(8*m_length)))
					return RESULT_ERR_INVALID_ARG; // value out of range
				value = (unsigned int)dvalue;
			}
		}

		if ((m_dataType.flags&SIG) != 0) { // signed value
			if ((value & (1 << (m_dataType.numBytes*8 - 1))) != 0) { // negative signed value
				if (value < m_dataType.minValueOrLength)
						return RESULT_ERR_INVALID_ARG; // value out of range
			}
			else if (value > m_dataType.maxValueOrLength)
					return RESULT_ERR_INVALID_ARG; // value out of range
		}
		else if (value < m_dataType.minValueOrLength || value > m_dataType.maxValueOrLength)
			return RESULT_ERR_INVALID_ARG; // value out of range
	}


	return writeRawValue(value, output);
}


result_t ValueListDataField::readSymbols(SymbolString& input, std::ostringstream& output)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, value);
	if (result != RESULT_OK)
		return result;

	if (value == m_dataType.replacement) {
		output << "-";
		return RESULT_OK;
	}

	std::map<unsigned int, std::string>::iterator it = m_values.find(value);
	if (it == m_values.end())
		return RESULT_ERR_INVALID_ARG; // value assignment not found

	output << it->second;
	return RESULT_OK;
}

result_t ValueListDataField::writeSymbols(std::istringstream& input, SymbolString& output)
{
	std::string str;
	input >> str;

	for (std::map<unsigned int, std::string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return writeRawValue(it->first, output);

	return RESULT_ERR_INVALID_ARG; // value assignment not found
}


} //namespace


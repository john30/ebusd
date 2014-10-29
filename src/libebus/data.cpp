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


static const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};


DataField* DataField::create(const unsigned char dstAddress, const bool isSetMessage,
		std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end) {
	std::string name, unit, comment;
	PartType partType;
	float factor;
	size_t baseOffset = 0, offset = 0, length = 0, maxPos = 16, offsetCnt = 0;

	if (it == end)
		return NULL;
	name = *it++;
	if (it == end)
		return NULL;
	const char* posStr = (*it++).c_str();
	if (it == end)
		return NULL;
	if (posStr[0] >= 'a' && posStr[1] == 0) {
		if (posStr[0] == 's') { // slave ACK
			// QQ ZZ PB SB NN + Dx + CRC
			//offset = 5+command.getMasterDataLength()+1; // skip QQ ZZ PB SB NN Dx CRC
		} else if (posStr[0] == 'm') { // master ACK
			//offset = 5+command.getMasterDataLength()+3+command.getSlaveDataLength()+1; // skip QQ ZZ PB SB NN Dx CRC ACK NN Dx CRC
		} else {
			return NULL; // TODO error code: invalid pos definition
		}
	} else {
		if (dstAddress == BROADCAST
		|| isMaster(dstAddress)
		|| (isSetMessage == true && posStr[0] <= '9')
		|| posStr[0] == 'm') { // master data
			partType = pt_masterData;
			baseOffset = 5; // skip QQ ZZ PB SB NN
			//len = command.getMasterDataLength();
			if (posStr[0] == 'm')
				posStr++;
		} else if ((isSetMessage == false && posStr[0] <= '9')
		|| posStr[0] == 's') { // slave data
			baseOffset = 1;
			//offset = 5+command.getMasterDataLength()+3; // skip QQ ZZ PB SB NN Dx CRC ACK NN
			//len = command.getSlaveDataLength();
			if (posStr[0] == 's')
				posStr++;
		} else {
			return NULL; // TODO error code: invalid pos definition
		}
		std::string token;
		std::istringstream stream(posStr);
		while (std::getline(stream, token, '-') != 0) {
			if (++offsetCnt > 2)
				return NULL; // TODO error code: invalid pos definition
			const char* start = token.c_str();
			char* end = NULL;
			int pos = strtoul(start, &end, 10)-1; // 1-based
			if (end != start+strlen(start))
				return NULL; // TODO error code: invalid pos definition
			if (pos < 0 || baseOffset+pos > maxPos)
				return NULL; // TODO error code: invalid pos definition
			else if (offsetCnt==1)
				offset = baseOffset+pos;
			else if (baseOffset+pos >= offset)
				length = baseOffset+pos+1-offset;
			else { // wrong order e.g. 4-3
				length = offset-(baseOffset+pos+1);
				offset = baseOffset+pos;
			}
		}
	}

	const char* typeStr = (*it++).c_str();

	if (it == end)
		factor = 1.0;
	else {
		std::string factorStr = *it++;
		if (factorStr.length() > 0 && factorStr.find_first_not_of("0123456789.") == std::string::npos)
			factor = static_cast<float>(strtod(factorStr.c_str(), NULL));
		else
			factor = 1.0;
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

	if (strcasecmp(typeStr, "STR") == 0) {
		if (length == 0)
			length = 1;
		return new StringDataField(name, partType, offset, length, dt_string, unit, comment);
	}
	if (strcasecmp(typeStr, "HEX") == 0) {
		if (length == 0)
			length = 1;
		return new StringDataField(name, partType, offset, length, dt_hex, unit, comment);
	}
	if (strcasecmp(typeStr, "UCH") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 1, dt_uchar, unit, comment, factor, 0xff);
	}
	if (strcasecmp(typeStr, "SCH") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 1, dt_schar, unit, comment, factor, 0x80);
	}
	if (strcasecmp(typeStr, "BCD") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 1, dt_bcd, unit, comment, factor, 0xff); // TODO max value 99
	}
	if (strcasecmp(typeStr, "D1B") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 1, dt_d1b, unit, comment, factor, 0x80);
	}
	if (strcasecmp(typeStr, "D1C") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 1, dt_d1c, unit, comment, factor*0.5, 0xff); // TODO max value 100
	}
	if (strcasecmp(typeStr, "UIN") == 0) {
		if (length != 0 && length != 2)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 2, dt_uint, unit, comment, factor, 0xffff);
	}
	if (strcasecmp(typeStr, "SIN") == 0) {
		if (length != 0 && length != 2)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 2, dt_sint, unit, comment, factor, 0x8000);
	}
	if (strcasecmp(typeStr, "D2B") == 0) {
		if (length != 0 && length != 2)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 2, dt_d2b, unit, comment, factor/256.0, 0x8000);
	}
	if (strcasecmp(typeStr, "D2C") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 2, dt_d2c, unit, comment, factor/16.0, 0x8000);
	}
	if (strcasecmp(typeStr, "ULG") == 0) {
		if (length != 0 && length != 4)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 4, dt_ulong, unit, comment, factor, 0xffffffff);
	}
	if (strcasecmp(typeStr, "SLG") == 0) {
		if (length != 0 && length != 4)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 4, dt_slong, unit, comment, factor, 0x80000000);
	}
	if (strcasecmp(typeStr, "FLT") == 0) {
		if (length != 0 && length != 2)
			return NULL; // TODO error code: invalid pos definition
		return new NumericDataField(name, partType, offset, 2, dt_float, unit, comment, factor/1000.0, 0x8000); // TODO replacement
	}
	if (strcasecmp(typeStr, "BDA") == 0 || strcasecmp(typeStr, "HDA") == 0) {
		if (length == 0)
			length = 4;
		else if (length != 3 && length != 4)
			return NULL; // TODO error code: invalid pos definition
		return new StringDataField(name, partType, offset, length, dt_date, unit, comment); // TODO better numeric?
	}
	if (strcasecmp(typeStr, "BDY") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new StringDataField(name, partType, offset, 1, dt_day, unit, comment); // TODO better numeric?
	}
	if (strcasecmp(typeStr, "BTI") == 0 || strcasecmp(typeStr, "HTI") == 0) {
		if (length != 0 && length != 3)
			return NULL; // TODO error code: invalid pos definition
		return new StringDataField(name, partType, offset, 3, dt_time, unit, comment); // TODO better numeric?
	}
	if (strcasecmp(typeStr, "TTM") == 0) {
		if (length != 0 && length != 1)
			return NULL; // TODO error code: invalid pos definition
		return new StringDataField(name, partType, offset, 1, dt_tTime, unit, comment); // TODO better numeric?
	}
	return NULL; // TODO error code: invalid type definition
}

const std::string DataField::parseSymbols(SymbolString& masterData, SymbolString& slaveData, bool verbose)
{
	SymbolString& data = m_partType == pt_masterData ? masterData : slaveData;
	switch (m_partType) {
	case pt_masterData:
		break;
	case pt_slaveData:
		break;
	default:
		return "invalid part type";
	}
	std::ostringstream output;
	if (verbose)
		output << m_name << "=";

	if (parse(data, output) == false)
		return "unable to parse";

	if (verbose && m_unit.length() > 0)
		output << " " << m_unit;
	if (verbose && m_comment.length() > 0)
		output << " [" << m_comment << "]";
	return output.str();
}

bool DataField::formatSymbols(const std::string& value, SymbolString& masterData, SymbolString& slaveData)
{
	SymbolString& data = m_partType == pt_masterData ? masterData : slaveData;
	switch (m_partType) {
	case pt_masterData:
		break;
	case pt_slaveData:
		break;
	default:
		return false; // TODO error code
	}
	std::istringstream input(value);
	if (format(data, input) == false)
		return false; // TODO error code
	return true;
}



bool StringDataField::parse(SymbolString& data, std::ostringstream& output)
{
	size_t start = m_offset, end = m_offset+m_length;
	unsigned char ch;

	if (end > data.size()) {
		return false; // TODO error not enough data available
	}
	if (m_dataType == dt_time) { // reverse order
		for (size_t i = end-1; i >= start; i--) {
			ch = data[i];
			if ((ch&0xf0) > 0x90 || (ch&0x0f) > 0x09)
				return false; // invalid BCD
			if ((i == end-1 && ch > 0x23) || (i != end-1 && ch > 0x59))
				return false; // invalid time
			if (i != end-1)
				output << ":";
			output << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(ch);
		}
		return true;
	}

	for (size_t i = start; i < end; i++) {
		ch = data[i];
		switch (m_dataType) {
		case dt_hex:
			if (i != start)
				output << " ";
			output << std::nouppercase << std::setw(2) << std::hex
			       << std::setfill('0') << static_cast<unsigned>(ch);
			break;
		case dt_date:
			if (m_length == 4u && i == m_offset+2u)
				break; // skip weekday in between
			if ((ch&0xf0) > 0x90 || (ch&0x0f) > 0x09)
				return false; // invalid BCD
			if (i == end-1)
				output << std::hex << (0x2000+ch);
			else if (ch < 0x01 || (i == start && ch > 0x31) || (i == start+1 && ch > 0x12))
				return false; // invalid date
			else
				output << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(ch) << ".";
			break;
		case dt_day:
			if (ch < 1 || ch > 7)
				return false; // invalid day
			output << days[ch-1];
			break;
		case dt_tTime:
			if (ch/6 > 23)
				return false; // invalid time
			output << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ch/6) << ":"
			       << std::setw(2) << std::setfill('0') << static_cast<unsigned>((ch%6)*10);
			break;
		default:
			if (ch < 0x20)
				ch = 0x20;
			output << static_cast<char>(ch);
			break;
		}
	}

	return true;
}

bool StringDataField::format(SymbolString& data, std::istringstream& input)
{
	size_t start = m_offset, end = m_offset+m_length;
	const char* str;
	char* strEnd;
	unsigned long int value, hours;
	unsigned char ch;

	std::string token;
	if (m_dataType == dt_time) { // reverse order
		for (size_t i = end-1; i >=start; i--) {
			if (std::getline(input, token, ':') == 0)
				return false;
			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 16);
			if (strEnd != str+strlen(str))
				return false; // TODO error code: invalid value
			if ((value&0xf0) > 0x90 || (value&0x0f) > 0x09)
				return false; // invalid BCD
			if ((i == end-1 && value > 0x23) || (i != end-1 && value > 0x59))
				return false; // invalid time
			data[i] = (unsigned char)value;
		}
		return true;
	}

	for (size_t i = start; i < end; i++) {
		switch (m_dataType) {
		case dt_hex:
			while (input.peek()==' ')
				input.get();
			token.clear();
			token.push_back(input.get());
			if (input.eof() == true)
				return false; // TODO error code: invalid value
			token.push_back(input.get());
			if (input.eof() == true)
				return false; // TODO error code: invalid value

			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 16);
			if (strEnd != str+strlen(str))
				return false; // TODO error code: invalid value
			data[i] = (unsigned char)value;
			break;
		case dt_date:
			if (m_length == 4u && i == m_offset+2u)
				break; // skip weekday in between
			if (std::getline(input, token, '.') == 0)
				return false;
			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 16);
			if (strEnd != str+strlen(str))
				return false; // TODO error code: invalid value
			if ((value&0xf0) > 0x90 || (value&0x0f) > 0x09)
				return false; // invalid BCD
			if (i == end-1) {
				if (value>=0x2000)
					value -= 0x2000;
				if (value>=0x100)
					return false; // invalid year
			}
			else if (value < 1 || (i == start && value > 0x31) || (i == start+1 && value > 0x12))
				return false; // invalid date
			data[i] = (unsigned char)value;
			break;
		case dt_day:
			str = input.str().c_str();
			for (ch = 0; ch < 7; ch++)
				if (strcasecmp(days[ch], str) == 0)
					break;
			if (ch == 7)
				return false; // invalid day
			data[i] = ch+1;
			break;
		case dt_tTime:
			if (std::getline(input, token, ':') == 0)
				return false;
			str = token.c_str();
			strEnd = NULL;
			hours = strtoul(str, &strEnd, 10);
			if (strEnd != str+strlen(str))
				return false; // TODO error code: invalid value
			if (hours > 23)
				return false; // invalid time

			if (std::getline(input, token, ':') == 0)
				return false;
			str = token.c_str();
			strEnd = NULL;
			value = strtoul(str, &strEnd, 10);
			if (strEnd != str+strlen(str))
				return false; // TODO error code: invalid value
			if (value > 59 || (value%10) != 0)
				return false; // invalid time
			data[i] = (unsigned char)(hours*6 + value/10);
			break;
		default:
			ch = input.get();
			if (input.eof() == true || ch < 0x20)
				ch = 0x20;
			data[i] = ch;
			break;
		}
	}

	return true;
}


bool NumericDataField::parse(SymbolString& data, std::ostringstream& output)
{
	size_t start = m_offset, end = m_offset+m_length;
	unsigned int value = 0;
	int signedValue;
	unsigned char ch;

	if (end > data.size())
		return false; // TODO error not enough data available

	for (size_t i = start, exp = 1; i < end; i++) {
		ch = data[i];
		switch (m_dataType) {
		case dt_bcd:
			if (ch == m_replacement) {
				output << "-";
				return true;
			}
			else if ((ch&0xf0) > 0x90 || (ch&0x0f) > 0x09)
				return false; // invalid BCD
			else
				value |= ((ch>>4)*10 + (ch&0x0f))*exp;
			exp = exp*100;
			break;
		default:
			value |= ch*exp;
			exp = exp<<8;
			break;
		}
	}
	if (value == m_replacement) {
		output << "-";
		return true;
	}

	switch (m_dataType) {
	case dt_schar:
	case dt_d1b:
		signedValue = (char)value;
		break;
	case dt_d2b:
	case dt_d2c:
	case dt_sint:
		signedValue = (short)value;
		break;
	case dt_slong:
		signedValue = (int)value;
		break;
	case dt_ulong:
		if (m_factor == 1.0)
			output << std::setprecision(0) << std::fixed << static_cast<float>(value);
		else
			output << std::setprecision(3) << std::fixed << static_cast<float>(value * m_factor);
		return true;
	default:
		signedValue = value;
		break;
	}
	if (m_factor == 1.0)
		output << std::setprecision(0) << std::fixed << static_cast<float>(signedValue);
	else
		output << std::setprecision(3) << std::fixed << static_cast<float>(signedValue * m_factor);

	return true;
}

bool NumericDataField::format(SymbolString& data, std::istringstream& input)
{
	size_t start = m_offset, end = m_offset+m_length;
	unsigned int value;
	unsigned char ch;

	const char* str = input.str().c_str();
	if (strcasecmp(str, "-") == 0)
		// replacement value
		value = m_replacement;
	else {
		char* strEnd = NULL;
		double dvalue = strtod(str, &strEnd);
		if (strEnd != str+strlen(str))
			return false; // TODO error code: invalid value
		dvalue /= m_factor;
		switch (m_dataType) {
		case dt_schar:
		case dt_sint:
		case dt_slong:
		case dt_d1b:
		case dt_d2b:
		case dt_d2c:
			if (dvalue < -(1LL<<(8*m_length)) || dvalue >= (1LL<<(8*m_length)))
				return false; // TODO error code: invalid value
			break;
		default:
			// no special handling
			if (dvalue < 0.0 || dvalue >= (1LL<<(8*m_length)))
				return false; // TODO error code: invalid value
			break;
		}
		value = (unsigned int)dvalue;
	}

	for (size_t i = start, exp = 1; i < end; i++) {
		switch (m_dataType) {
		case dt_bcd:
			if (value == m_replacement)
				ch = m_replacement;
			else {
				ch = (value/exp)%100;
				ch = ((ch/10)<<4) | (ch%10);
			}
			exp = exp*100;
			break;
		default:
			ch = (value/exp)&0xff;
			exp = exp<<8;
			break;
		}
		data[i] = ch;
	}

	return true;
}


} //namespace


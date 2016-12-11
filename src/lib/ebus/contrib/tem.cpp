/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "datatype.h"
#include "tem.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>
#include <typeinfo>

using namespace std;

void contrib_tem_register() {
	DataTypeList::getInstance()->add(new TemParamDataType("TEM_P"));
}

result_t TemParamDataType::derive(int divisor, unsigned char bitCount, NumberDataType* &derived)
{
	if (divisor == 0) {
		divisor = 1;
	}
	if (bitCount == 0) {
		bitCount = m_bitCount;
	}
	if (divisor == 1 && bitCount == 16) {
		derived = this;
		return RESULT_OK;
	}
	return RESULT_ERR_INVALID_ARG;
}

result_t TemParamDataType::readSymbols(SymbolString& input, const bool isMaster,
		const unsigned char offset, const unsigned char length,
		ostringstream& output, OutputFormat outputFormat)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, offset, length, value);
	if (result != RESULT_OK) {
		return result;
	}

	if (value == m_replacement) {
		if (outputFormat & OF_JSON) {
			output << "null";
		} else {
			output << NULL_VALUE;
		}
		return RESULT_OK;
	}
	int grp = 0, num = 0;
	if (isMaster) {
		grp = (value & 0x1f); // grp in bits 0...5
		num = ((value >> 8) & 0x7f); // num in bits 8...13
	} else {
		grp = ((value >> 7) & 0x1f); // grp in bits 7...11
		num = (value & 0x7f); // num in bits 0...6
	}
	if (outputFormat & OF_JSON) {
		output << '"';
	}
	output << setfill('0') << setw(2) << dec << static_cast<int>(grp) << '-' << setw(3) << static_cast<int>(num);
	if (outputFormat & OF_JSON) {
		output << '"';
	}
	output << setfill(' ') << setw(0); // reset
	return RESULT_OK;
}

result_t TemParamDataType::writeSymbols(istringstream& input,
	const unsigned char offset, const unsigned char length,
	SymbolString& output, const bool isMaster, unsigned char* usedLength)
{
	unsigned int value;
	int grp, num;
	string token;

	const char* str = input.str().c_str();
	if (strcasecmp(str, NULL_VALUE) == 0) {
		value = m_replacement; // replacement value
	} else {
		if (input.eof() || !getline(input, token, '-')) {
			return RESULT_ERR_EOF; // incomplete
		}
		str = token.c_str();
		if (str == NULL || *str == 0) {
			return RESULT_ERR_EOF; // input too short
		}
		char* strEnd = NULL;
		grp = (unsigned int)strtoul(str, &strEnd, 10);
		if (strEnd == NULL || strEnd == str || *strEnd != 0) {
			return RESULT_ERR_INVALID_NUM; // invalid value
		}
		if (input.eof() || !getline(input, token, '-')) {
			return RESULT_ERR_EOF; // incomplete
		}
		str = token.c_str();
		if (str == NULL || *str == 0) {
			return RESULT_ERR_EOF; // input too short
		}
		strEnd = NULL;
		num = (unsigned int)strtoul(str, &strEnd, 10);
		if (strEnd == NULL || strEnd == str || *strEnd != 0) {
			return RESULT_ERR_INVALID_NUM; // invalid value
		}
		if (grp < 0 || grp > 0x1f || num < 0 || num > 0x7f) {
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
		}
		if (isMaster) {
			value = grp | (num<<8); // grp in bits 0...5, num in bits 8...13
		} else {
			value = (grp<<7) | num; // grp in bits 7...11, num in bits 0...6
		}
	}
	if (value < getMinValue() || value > getMaxValue()) {
		return RESULT_ERR_OUT_OF_RANGE; // value out of range
	}
	return writeRawValue(value, offset, length, output, usedLength);
}

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "datatype.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>
#include <typeinfo>

using namespace std;

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


bool DataType::dump(ostream& output, const unsigned char length) const
{
	output << m_id;
	if (isAdjustableLength()) {
		if (length==REMAIN_LEN)
			output << ":*";
		else
			output << ":" << static_cast<unsigned>(length);
	}
	output << FIELD_SEPARATOR;
	return false;
}


result_t StringDataType::readRawValue(SymbolString& input, const unsigned char offset,
		const unsigned char length, unsigned int& value)
{
	return RESULT_EMPTY;
}

result_t StringDataType::readSymbols(SymbolString& input, const unsigned char baseOffset,
		const unsigned char length, ostringstream& output, OutputFormat outputFormat)
{
	size_t start = 0, count = length;
	int incr = 1;
	unsigned char ch;
	bool terminated = false;
	if (count==REMAIN_LEN && input.size()>baseOffset) {
		count = input.size()-baseOffset;
	} else if (baseOffset + count > input.size()) {
		return RESULT_ERR_INVALID_POS;
	}
	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}

	if (outputFormat & OF_JSON)
		output << '"';
	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		ch = input[baseOffset + offset];
		if (m_isHex) {
			if (i > 0)
				output << ' ';
			output << setw(2) << hex << setfill('0') << static_cast<unsigned>(ch);
		} else {
			if (ch < 0x20)
				ch = (unsigned char)m_replacement;
			if (ch == 0x00)
				terminated = true;
			else if (!terminated)
				output << setw(0) << dec << static_cast<char>(ch);
		}
	}
	if (outputFormat & OF_JSON)
		output << '"';

	return RESULT_OK;
}

result_t StringDataType::writeSymbols(istringstream& input,
		unsigned char baseOffset, const unsigned char length,
		SymbolString& output, unsigned char* usedLength)
{
	size_t start = 0, count = length;
	bool remainder = count==REMAIN_LEN && hasFlag(ADJ);
	int incr = 1;
	unsigned int value = 0;
	string token;

	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}
	if (isIgnored() && !hasFlag(REQ)) {
		if (remainder) {
			count = 1;
		}
		for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
			output[baseOffset + offset] = (unsigned char)m_replacement; // fill up with replacement
		}
		if (usedLength!=NULL)
			*usedLength = (unsigned char)count;
		return RESULT_OK;
	}
	result_t result;
	size_t i = 0, offset;
	for (offset = start; i < count; offset += incr, i++) {
		if (m_isHex) {
			while (!input.eof() && input.peek() == ' ')
				input.get();
			if (input.eof()) { // no more digits
				value = m_replacement; // fill up with replacement
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
		} else {
			if (input.eof()) {
				value = m_replacement;
			} else {
				value = input.get();
				if (input.eof() || value < 0x20)
					value = m_replacement;
			}
		}
		if (remainder && input.eof() && i > 0) {
			if (value == 0x00 && !m_isHex) {
				output[baseOffset + offset] = 0;
				offset += incr;
			}
			break;
		}
		if (value > 0xff)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
		output[baseOffset + offset] = (unsigned char)value;
	}

	if (!remainder && i < count)
		return RESULT_ERR_EOF; // input too short
	if (usedLength!=NULL)
		*usedLength = (unsigned char)((offset-start)*incr);
	return RESULT_OK;
}


result_t DateTimeDataType::readRawValue(SymbolString& input, const unsigned char offset,
		const unsigned char length, unsigned int& value)
{
	return RESULT_EMPTY;
}

result_t DateTimeDataType::readSymbols(SymbolString& input, const unsigned char baseOffset,
		const unsigned char length, ostringstream& output, OutputFormat outputFormat)
{
	size_t start = 0, count = length;
	int incr = 1;
	unsigned char ch, last = 0, hour = 0;
	if (count==REMAIN_LEN && input.size()>baseOffset) {
		count = input.size()-baseOffset;
	} else if (baseOffset + count > input.size()) {
		return RESULT_ERR_INVALID_POS;
	}
	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}

	if (outputFormat & OF_JSON)
		output << '"';
	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		if (length == 4 && i == 2 && m_isDate)
			continue; // skip weekday in between
		ch = input[baseOffset + offset];
		if (hasFlag(BCD) && (hasFlag(REQ) || ch != m_replacement)) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD
			ch = (unsigned char)((ch >> 4) * 10 + (ch & 0x0f));
		}
		switch (m_isDate)
		{
		case true:
			if (length == 2) { // number of days since 01.01.1900
				if (i == 0) {
					break;
				}
				/*int days = last*256 + ch + 15020; // 01.01.1900
				int y = days(value < 100 ? value + 2000 : value) - 1900;
				int l = (last==1 || last==2) ? 1 : 0;
				int mjd = 14956 + lastLast + (int)((y-l)*365.25) + (int)((last+1+l*12)*30.6001);
				int daysSinceSunday = (mjd+3) % 7; // Sun=0
				if ((m_dataType.flags & BCD) != 0)
					output[baseOffset + offset - incr] = (unsigned char)((6+daysSinceSunday) % 7); // Sun=0x06
				else
					output[baseOffset + offset - incr] = (unsigned char)(daysSinceSunday==0 ? 7 : daysSinceSunday); // Sun=0x07*/
				output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch) << ".";
			}
			if (!hasFlag(REQ) && ch == m_replacement) {
				if (i + 1 != length) {
					output << NULL_VALUE << ".";
					break;
				}
				else if (last == m_replacement) {
					output << NULL_VALUE;
					break;
				}
			}
			if (i + 1 == length)
				output << (2000 + ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date
			else
				output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case false:
			if (!hasFlag(REQ) && ch == m_replacement) {
				if (length == 1) { // truncated time
					output << NULL_VALUE << ":" << NULL_VALUE;
					break;
				}
				if (i > 0)
					output << ":";
				output << NULL_VALUE;
				break;
			}
			if (length == 1) { // truncated time
				if (i == 0) {
					ch = (unsigned char)(ch/(60/m_resolution)); // convert to hours
					offset -= incr; // repeat for minutes
					count++;
				}
				else
					ch = (unsigned char)((ch % (60/m_resolution)) * m_resolution); // convert to minutes
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
		}
		last = ch;
	}
	if (outputFormat & OF_JSON)
		output << '"';

	return RESULT_OK;
}

result_t DateTimeDataType::writeSymbols(istringstream& input,
		unsigned char baseOffset, const unsigned char length,
		SymbolString& output, unsigned char* usedLength)
{
	size_t start = 0, count = length;
	bool remainder = count==REMAIN_LEN && hasFlag(ADJ);
	int incr = 1;
	unsigned int value = 0, last = 0, lastLast = 0;
	string token;

	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}
	if (isIgnored() && !hasFlag(REQ)) {
		if (remainder) {
			count = 1;
		}
		for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
			output[baseOffset + offset] = (unsigned char)m_replacement; // fill up with replacement
		}
		if (usedLength!=NULL)
			*usedLength = (unsigned char)count;
		return RESULT_OK;
	}
	result_t result;
	size_t i = 0, offset;
	for (offset = start; i < count; offset += incr, i++) {
		switch (m_isDate)
		{
		case true:
			if (length == 4 && i == 2)
				continue; // skip weekday in between
			if (input.eof() || !getline(input, token, '.'))
				return RESULT_ERR_EOF; // incomplete
			if (!hasFlag(REQ) && strcmp(token.c_str(), NULL_VALUE) == 0) {
				value = m_replacement;
				break;
			}
			value = parseInt(token.c_str(), 10, 0, 2099, result);
			if (result != RESULT_OK) {
				return result; // invalid date part
			}
			if (i + 1 == length) {
				if (length == 4) {
					// calculate local week day
					int y = (value < 100 ? value + 2000 : value) - 1900;
					int l = (last==1 || last==2) ? 1 : 0;
					int mjd = 14956 + lastLast + (int)((y-l)*365.25) + (int)((last+1+l*12)*30.6001);
					int daysSinceSunday = (mjd+3) % 7; // Sun=0
					if (hasFlag(BCD))
						output[baseOffset + offset - incr] = (unsigned char)((6+daysSinceSunday) % 7); // Sun=0x06
					else
						output[baseOffset + offset - incr] = (unsigned char)(daysSinceSunday==0 ? 7 : daysSinceSunday); // Sun=0x07
				}
				if (value >= 2000)
					value -= 2000;
				if (value > 99)
					return RESULT_ERR_OUT_OF_RANGE; // invalid year
			}
			else if (value < 1 || (i == 0 && value > 31) || (i == 1 && value > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date part
			break;
		case false:
			if (input.eof() || !getline(input, token, LENGTH_SEPARATOR))
				return RESULT_ERR_EOF; // incomplete
			if (!hasFlag(REQ) && strcmp(token.c_str(), NULL_VALUE) == 0) {
				value = m_replacement;
				if (length == 1) { // truncated time
					if (i == 0) {
						last = value;
						offset -= incr; // repeat for minutes
						count++;
						continue;
					}
					if (last != m_replacement)
						return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				}
				break;
			}
			value = parseInt(token.c_str(), 10, 0, 59, result);
			if (result != RESULT_OK)
				return result; // invalid time part
			if ((i == 0 && value > 24) || (i > 0 && (last == 24 && value > 0) ))
				return RESULT_ERR_OUT_OF_RANGE; // invalid time part
			if (length == 1) { // truncated time
				if (i == 0) {
					last = value;
					offset -= incr; // repeat for minutes
					count++;
					continue;
				}

				if ((value % m_resolution) != 0)
					return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				value = (last * 60 + value)/m_resolution;
				if (value > 24 * 6)
					return RESULT_ERR_OUT_OF_RANGE; // invalid time
			}
			break;
		}
		if (remainder && input.eof() && i > 0) {
			if (value == 0x00) {
				output[baseOffset + offset] = 0;
				offset += incr;
			}
			break;
		}
		lastLast = last;
		last = value;
		if (hasFlag(BCD) && (hasFlag(REQ) || value != m_replacement)) {
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
	if (usedLength!=NULL)
		*usedLength = (unsigned char)((offset-start)*incr);
	return RESULT_OK;
}


NumberDataType::NumberDataType(const char* id, const unsigned char bitCount, const unsigned short flags, const unsigned int replacement,
		const unsigned int minValue, const unsigned int maxValue, const int divisor)
	: DataType(id, bitCount, flags, replacement), m_minValue(minValue), m_maxValue(maxValue), m_divisor(divisor), m_firstBit(0), m_baseType(NULL)
{
	unsigned char precision = 0;
	if (divisor > 1)
		for (unsigned int exp = 1; exp < MAX_DIVISOR; exp *= 10, precision++)
			if (exp >= (unsigned int)divisor)
				break;
	m_precision = precision;
}

bool NumberDataType::dump(ostream& output, unsigned char length) const
{
	if (m_bitCount < 8) {
		DataType::dump(output, m_bitCount);
	} else {
		DataType::dump(output, length);
	}
	if (m_baseType) {
		if (m_baseType->m_divisor != m_divisor) {
			output << static_cast<int>(m_divisor / m_baseType->m_divisor);
			return true;
		}
	} else if (m_divisor!=1) {
		output << static_cast<int>(m_divisor);
		return true;
	}
	return false;
}

result_t NumberDataType::derive(int divisor, unsigned char bitCount, NumberDataType* &derived)
{
	if (divisor == 0) {
		divisor = 1;
	}
	if (bitCount <= 0) {
		bitCount = m_bitCount;
	} else if (bitCount!=m_bitCount && (m_bitCount%8)!=0) { // todo check bitCount%8=0
		if (bitCount+m_firstBit>8) {
			return RESULT_ERR_OUT_OF_RANGE;
		}
	}
	if (m_divisor != 1) {
		if (divisor == 1) {
			divisor = m_divisor;
		} else if (divisor < 0) {
			/*if ((divisor < 0) != (m_divisor < 0) {
				return RESULT_ERR_INVALID_ARG;
			}*/
			if (m_divisor > 1)
				return RESULT_ERR_INVALID_ARG;

			divisor *= -m_divisor;
		} else if (m_divisor < 0) {
			if (divisor > 1)
				return RESULT_ERR_INVALID_ARG;

			divisor *= -m_divisor;
		} else
			divisor *= m_divisor;
	}
	if (divisor==m_divisor && bitCount==m_bitCount) {
		derived = this;
		return RESULT_OK;
	}
	if (-MAX_DIVISOR > divisor || divisor > MAX_DIVISOR)
		return RESULT_ERR_OUT_OF_RANGE;

	if (m_bitCount < 8)
		derived = new NumberDataType(m_id, bitCount, m_flags, m_replacement,
			m_firstBit, divisor);
	else
		derived = new NumberDataType(m_id, bitCount, m_flags, m_replacement,
			m_minValue, m_maxValue, divisor);
	derived->m_baseType = m_baseType ? m_baseType : this;
	return RESULT_OK;
}

result_t NumberDataType::readRawValue(SymbolString& input,
		unsigned char baseOffset, const unsigned char length,
		unsigned int& value)
{
	size_t start = 0, count = length;
	int incr = 1;
	unsigned char ch;

	if (baseOffset + length > input.size())
		return RESULT_ERR_INVALID_POS; // not enough data available

	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}

	value = 0;
	unsigned int exp = 1;
	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		ch = input[baseOffset + offset];
		if (hasFlag(BCD)) {
			if (!hasFlag(REQ) && ch == (m_replacement & 0xff)) {
				value = m_replacement;
				return RESULT_OK;
			}
			if (!hasFlag(HCD)) {
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

	if (m_firstBit > 0) {
		value >>= m_firstBit;
	}
	if (m_bitCount < 8) {
		value &= (1 << m_bitCount) - 1;
	}

	return RESULT_OK;
}

result_t NumberDataType::readSymbols(SymbolString& input,
		const unsigned char baseOffset, const unsigned char length,
		ostringstream& output, OutputFormat outputFormat)
{
	unsigned int value = 0;
	int signedValue;

	result_t result = readRawValue(input, baseOffset, length, value);
	if (result != RESULT_OK) {
		return result;
	}
	output << setw(0) << dec; // initialize output

	if (!hasFlag(REQ) && value == m_replacement) {
		if (outputFormat & OF_JSON) {
			output << "null";
		} else {
			output << NULL_VALUE;
		}
		return RESULT_OK;
	}

	bool negative = hasFlag(SIG) && (value & (1 << (m_bitCount - 1))) != 0;
	if (m_bitCount == 32) {
		if (hasFlag(EXP)) {  // IEEE 754 binary32
			float val;
#ifdef HAVE_DIRECT_FLOAT_FORMAT
#	if HAVE_DIRECT_FLOAT_FORMAT == 2
			value = __builtin_bswap32(value);
#	endif
			unsigned char* pval = (unsigned char*)&value;
			val = *((float*)pval);
#else
			int exp = (value >> 23) & 0xff; // 8 bits, signed
			if (exp == 0) {
				val = 0.0;
			} else {
				exp -= 127;
				unsigned int sig = value & ((1 << 23) - 1);
				val = (1.0f + (float)(sig / exp2(23))) * (float)exp2(exp);
				if (negative) {
					val = -val;
				}
			}
#endif
			if (val != 0.0) {
				if (m_divisor < 0) {
					val *= (float)-m_divisor;
				} else if (m_divisor > 1) {
					val /= (float)m_divisor;
				}
			}
			if (m_precision != 0) {
				output << fixed << setprecision(m_precision+6);
			} else if (val == 0) {
				output << fixed << setprecision(1);
			}
			output << static_cast<double>(val);
			return RESULT_OK;
		}
		if (!negative) {
			if (m_divisor < 0) {
				output << static_cast<float>((float)value * (float)(-m_divisor));
			} else if (m_divisor <= 1) {
				output << static_cast<unsigned>(value);
			} else {
				output << setprecision(m_precision)
				       << fixed << static_cast<float>((float)value / (float)m_divisor);
			}
			return RESULT_OK;
		}
		signedValue = (int)value; // negative signed value
	} else if (negative) { // negative signed value
		signedValue = (int)value - (1 << m_bitCount);
	} else {
		signedValue = (int)value;
	}
	if (m_divisor < 0) {
		output << static_cast<float>((float)signedValue * (float)(-m_divisor));
	} else if (m_divisor <= 1) {
		if (hasFlag(FIX) && hasFlag(BCD)) {
			if (outputFormat & OF_JSON) {
				output << '"';
				output << setw(length * 2) << setfill('0');
				output << static_cast<signed>(signedValue) << setw(0);
				output << '"';
				return RESULT_OK;
			}
			output << setw(length * 2) << setfill('0');
		}
		output << static_cast<signed>(signedValue) << setw(0);
	} else {
		output << setprecision(m_precision)
		       << fixed << static_cast<float>((float)signedValue / (float)m_divisor);
	}
	return RESULT_OK;
}

result_t NumberDataType::writeRawValue(unsigned int value,
		const unsigned char baseOffset, const unsigned char length,
		SymbolString& output, unsigned char* usedLength)
{
	size_t start = 0, count = length;
	int incr = 1;
	unsigned char ch;

	if (m_bitCount < 8 && (value & ~((1 << m_bitCount) - 1)) != 0) {
		return RESULT_ERR_OUT_OF_RANGE;
	}
	if (m_firstBit > 0) {
		value <<= m_firstBit;
	}

	if (hasFlag(REV)) { // reverted binary representation (most significant byte first)
		start = length - 1;
		incr = -1;
	}

	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		if (hasFlag(BCD)) {
			if (!hasFlag(REQ) && value == m_replacement)
				ch = m_replacement & 0xff;
			else {
				ch = (unsigned char)((value / exp) % 100);
				if (!hasFlag(HCD))
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
	if (usedLength!=NULL)
		*usedLength = length;
	return RESULT_OK;
}

result_t NumberDataType::writeSymbols(istringstream& input,
		const unsigned char baseOffset, const unsigned char length,
		SymbolString& output, unsigned char* usedLength)
{
	unsigned int value;

	const char* str = input.str().c_str();
	if (!hasFlag(REQ) && (isIgnored() || strcasecmp(str, NULL_VALUE) == 0)) {
		value = m_replacement; // replacement value
	} else if (str == NULL || *str == 0) {
		return RESULT_ERR_EOF; // input too short
	} else if (hasFlag(EXP)) { // IEEE 754 binary32
		char* strEnd = NULL;
		double dvalue = strtod(str, &strEnd);
		if (strEnd == NULL || strEnd == str || *strEnd != 0) {
			return RESULT_ERR_INVALID_NUM; // invalid value
		}
		if (m_divisor < 0)
			dvalue /= -m_divisor;
		else if (m_divisor > 1)
			dvalue *= m_divisor;
#ifdef HAVE_DIRECT_FLOAT_FORMAT
		float val = (float)dvalue;
		unsigned char* pval = (unsigned char*)&val;
		value = *((int32_t*)pval);
#	if HAVE_DIRECT_FLOAT_FORMAT == 2
		value = __builtin_bswap32(value);
#	endif
#else
		value = 0;
		if (dvalue != 0) {
			bool negative = dvalue < 0;
			if (negative) {
				dvalue = -dvalue;
			}
			int exp = ilogb(dvalue);
			if (exp < -126 || exp > 127)
				return RESULT_ERR_INVALID_NUM; // invalid value
			dvalue = scalbln(dvalue, -exp) - 1.0;
			unsigned int sig = (unsigned int)(dvalue * exp2(23));
			exp += 127;
			value = (exp << 23) | sig;
			if (negative) {
				value |= 0x80000000;
			}
		}
#endif
	} else {
		char* strEnd = NULL;
		if (m_divisor == 1) {
			if (hasFlag(SIG)) {
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
			if (hasFlag(SIG)) {
				if (dvalue < -(1LL << (8 * length)) || dvalue >= (1LL << (8 * length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				if (dvalue < 0 && m_bitCount != 32)
					value = (int)(dvalue + (1 << m_bitCount));
				else
					value = (int)dvalue;
			} else {
				if (dvalue < 0.0 || dvalue >= (1LL << (8 * length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				value = (unsigned int)dvalue;
			}
		}

		if (hasFlag(SIG)) { // signed value
			if ((value & (1 << (m_bitCount - 1))) != 0) { // negative signed value
				if (value < m_minValue)
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
			}
			else if (value > m_maxValue)
				return RESULT_ERR_OUT_OF_RANGE; // value out of range
		}
		else if (value < m_minValue || value > m_maxValue)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
	}

	return writeRawValue(value, baseOffset, length, output, usedLength);
}

/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "decode.h"
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>

using namespace std;

Decode::Decode(const string& data, const string& factor)
	: m_data(data)
{
	if ((factor.find_first_not_of("0123456789.") == string::npos) == true)
		m_factor = static_cast<float>(strtod(factor.c_str(), NULL));
	else
		m_factor = 1.0;
}


string DecodeHEX::decode()
{
	ostringstream result;

	for (size_t i = 0; i < m_data.length()/2; i++)
		result << m_data.substr(i*2, 2) << " ";

	return result.str().substr(0, result.str().length()-1);
}

string DecodeUCH::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned short x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed << static_cast<float>(x * m_factor);

	return result.str();
}

string DecodeSCH::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned short x;
	ss >> x;

	ostringstream result;
	if ((x & 0x80) == 0x80)
		result << setprecision(3) << fixed
		       << static_cast<float>(static_cast<short>(- ( ((unsigned char) (~ x)) + 1) ) * m_factor);
	else
		result << setprecision(3) << fixed
		       << static_cast<float>(static_cast<short>(x) * m_factor);

	return result.str();
}

string DecodeUIN::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned short x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed << static_cast<float>(x * m_factor);

	return result.str();
}

string DecodeSIN::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned short x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed << static_cast<float>(static_cast<short>(x) * m_factor);

	return result.str();
}

string DecodeULG::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned int x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed << static_cast<float>(x * m_factor);

	return result.str();
}

string DecodeSLG::decode()
{
	stringstream ss;
	ss << hex << m_data;

	unsigned int x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed <<static_cast<float>(static_cast<int>(x) * m_factor);

	return result.str();
}

string DecodeFLT::decode()
{
	stringstream ss;
	ss << hex << m_data;

	short x;
	ss >> x;

	ostringstream result;
	result << setprecision(3) << fixed << static_cast<float>(x / 1000.0 * m_factor);

	return result.str();
}

string DecodeSTR::decode()
{
	ostringstream result;

	for (size_t i = 0; i <= m_data.length()/2; i++) {
		char tmp = static_cast<char>(strtol(m_data.substr(i*2, 2).c_str(), NULL, 16));
		if (tmp == 0x00) tmp = 0x20;
		result << tmp;
	}

	return result.str().substr(0, result.str().length()-1);
}

string DecodeBCD::decode()
{
	ostringstream result;
	unsigned char src = strtol(m_data.c_str(), NULL, 16);

	if ((src & 0x0F) > 0x09 || ((src >> 4) & 0x0F) > 0x09)
		result << static_cast<short>(0xFF);
	else
		result << static_cast<short>(( ( ((src & 0xF0) >> 4) * 10) + (src & 0x0F) ) * m_factor);

	return result.str();
}

string DecodeD1B::decode()
{
	ostringstream result;
	unsigned char src = strtol(m_data.c_str(), NULL, 16);

	if ((src & 0x80) == 0x80)
		result << static_cast<short>((- ( ((unsigned char) (~ src)) + 1) ) * m_factor);
	else
		result << static_cast<short>(src * m_factor);

	return result.str();
}

string DecodeD1C::decode()
{
	ostringstream result;
	unsigned char src = strtol(m_data.c_str(), NULL, 16);

	if (src > 0xC8)
		result << static_cast<float>(0xFF);
	else
		result << static_cast<float>((src / 2.0) * m_factor);

	return result.str();
}

string DecodeD2B::decode()
{
	ostringstream result;
	unsigned char src_lsb = static_cast<char>(strtol(m_data.substr(0, 2).c_str(), NULL, 16));
	unsigned char src_msb = static_cast<char>(strtol(m_data.substr(2, 2).c_str(), NULL, 16));

	if ((src_msb & 0x80) == 0x80)
		result << static_cast<float>
			((- ( ((unsigned char) (~ src_msb)) +
			 (  ( ((unsigned char) (~ src_lsb)) + 1) / 256.0) ) ) * m_factor);

	else
		result << static_cast<float>((src_msb + (src_lsb / 256.0)) * m_factor);

	return result.str();
}

string DecodeD2C::decode()
{
	ostringstream result;
	unsigned char src_lsb = static_cast<char>(strtol(m_data.substr(0, 2).c_str(), NULL, 16));
	unsigned char src_msb = static_cast<char>(strtol(m_data.substr(2, 2).c_str(), NULL, 16));

	if ((src_msb & 0x80) == 0x80)
		result << static_cast<float>
		((- ( ( ( ((unsigned char) (~ src_msb)) * 16.0) ) +
		      ( ( ((unsigned char) (~ src_lsb)) & 0xF0) >> 4) +
		    ( ( ( ((unsigned char) (~ src_lsb)) & 0x0F) +1 ) / 16.0) ) ) * m_factor);

	else
		result << static_cast<float>(( (src_msb * 16.0) + ((src_lsb & 0xF0) >> 4) +
					      ((src_lsb & 0x0F) / 16.0) ) * m_factor);

	return result.str();
}

string DecodeBDA::decode()
{
	ostringstream result;
	Decode* decode;
	short array[3];
	for (int i = 0; i < 3; i++) {
		decode = new DecodeBCD(m_data.substr(i*2, 2), "1.0");
		array[i] = static_cast<short>(strtol(decode->decode().c_str(), NULL, 10));
		delete decode;
	}

	result << setw(2) << setfill('0') << array[0] << "."
	       << setw(2) << setfill('0') << array[1] << "."
	       << array[2] + 2000;

	return result.str();
}

string DecodeHDA::decode()
{
	ostringstream result;
	short dd = static_cast<short>(strtol(m_data.substr(0, 2).c_str(), NULL, 16));
	short mm = static_cast<short>(strtol(m_data.substr(2, 2).c_str(), NULL, 16));
	short yy = static_cast<short>(strtol(m_data.substr(4, 2).c_str(), NULL, 16));

	result << setw(2) << setfill('0') << dd << "."
	       << setw(2) << setfill('0') << mm << "."
	       << yy + 2000;

	return result.str();
}

string DecodeBTI::decode()
{
	ostringstream result;
	Decode* decode;
	short array[3];
	for (int i = 0; i < 3; i++) {
		decode = new DecodeBCD(m_data.substr(i*2, 2), "1.0");
		array[i] = static_cast<short>(strtol(decode->decode().c_str(), NULL, 10));
		delete decode;
	}

	result << setw(2) << setfill('0') << array[0] << ":"
	       << setw(2) << setfill('0') << array[1] << ":"
	       << setw(2) << setfill('0') << array[2];

	return result.str();
}

string DecodeHTI::decode()
{
	ostringstream result;
	short hh = static_cast<short>(strtol(m_data.substr(0, 2).c_str(), NULL, 16));
	short mm = static_cast<short>(strtol(m_data.substr(2, 2).c_str(), NULL, 16));
	short ss = static_cast<short>(strtol(m_data.substr(4, 2).c_str(), NULL, 16));

	result << setw(2) << setfill('0') << hh << ":"
	       << setw(2) << setfill('0') << mm << ":"
	       << setw(2) << setfill('0') << ss;

	return result.str();
}

string DecodeBDY::decode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};

	ostringstream result;
	short day = static_cast<short>(strtol(m_data.c_str(), NULL, 16));

	if (day < 0 || day > 6)
		day = 7;

	result << days[day];

	return result.str();
}

string DecodeHDY::decode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};

	ostringstream result;
	short day = static_cast<short>(strtol(m_data.c_str(), NULL, 16)) - 1;

	if (day < 0 || day > 6)
		day = 7;

	result << days[day];

	return result.str();
}

string DecodeTTM::decode()
{
	ostringstream result;
	short hh = static_cast<short>(strtol(m_data.c_str(), NULL, 16)) / 6;
	short mm = static_cast<short>(strtol(m_data.c_str(), NULL, 16)) % 6 * 10;

	result << setw(2) << setfill('0') << hh << ":"
	       << setw(2) << setfill('0') << mm;

	return result.str();
}


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

#include "encode.h"
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>

using namespace std;

Encode::Encode(const string& data, const string& factor)
	: m_data(data)
{
	if ((factor.find_first_not_of("0123456789.") == string::npos) == true)
		m_factor = static_cast<float>(strtod(factor.c_str(), NULL));
	else
		m_factor = 1.0;
}


string EncodeHEX::encode()
{
	m_data.erase(remove_if(m_data.begin(), m_data.end(), ::isspace), m_data.end());

	return m_data;
}

string EncodeUCH::encode()
{
	ostringstream result;
	unsigned short src = static_cast<unsigned short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << setw(2) << hex << setfill('0') << src;

	return result.str().substr(result.str().length()-2,2);
}

string EncodeSCH::encode()
{
	ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127 || src > 127)
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x80);
	else
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(src);

	return result.str().substr(result.str().length()-2,2);
}

string EncodeUIN::encode()
{
	ostringstream result;
	unsigned short src = static_cast<unsigned short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << setw(4) << hex << setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

string EncodeSIN::encode()
{
	ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << setw(4) << hex << setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

string EncodeULG::encode()
{
	ostringstream result;
	unsigned long src = static_cast<unsigned long>(strtod(m_data.c_str(), NULL) / m_factor);
	result << setw(8) << hex << setfill('0') << src;

	return result.str().substr(6,2) + result.str().substr(4,2) +
	       result.str().substr(2,2) + result.str().substr(0,2);
}

string EncodeSLG::encode()
{
	ostringstream result;
	int src = static_cast<int>(strtod(m_data.c_str(), NULL) / m_factor);
	result << setw(8) << hex << setfill('0') << src;

	return result.str().substr(6,2) + result.str().substr(4,2) +
	       result.str().substr(2,2) + result.str().substr(0,2);
}

string EncodeFLT::encode()
{
	ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) * 1000.0 / m_factor);
	result << setw(4) << hex << setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

string EncodeSTR::encode()
{
	ostringstream result;

	for (size_t i = 0; i < m_data.length(); i++)
		result << setw(2) << hex << setfill('0') << static_cast<short>(m_data[i]);

	return result.str();
}

string EncodeBCD::encode()
{
	ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src > 99)
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0xFF);
	else
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>( ((src / 10) << 4) | (src % 10) );

	return result.str().substr(result.str().length()-2,2);
}

string EncodeD1B::encode()
{
	ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127 || src > 127)
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x80);
	else
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(src);

	return result.str().substr(result.str().length()-2,2);
}

string EncodeD1C::encode()
{
	ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < 0.0 || src > 100.0)
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0xFF);
	else
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(src * 2.0);

	return result.str();
}

string EncodeD2B::encode()
{
	ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127.999 || src > 127.999) {
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x80)
		       << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x00);
	} else {
		unsigned char tgt_lsb = static_cast<unsigned>((src - ((short) src)) * 256.0);
		unsigned char tgt_msb;

		if (src < 0.0 && tgt_lsb != 0x00)
			tgt_msb = static_cast<unsigned>((short) src - 1);
		else
			tgt_msb = static_cast<unsigned>((short) src);

		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(tgt_msb)
		       << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(tgt_lsb);
	}

	return result.str();
}

string EncodeD2C::encode()
{
	ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -2047.999 || src > 2047.999) {
		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x80)
		       << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(0x00);
	} else {
		unsigned char tgt_lsb = static_cast<unsigned>(
			((unsigned char) ( ((short) src) % 16) << 4) +
			((unsigned char) ( (src - ((short) src)) * 16.0)) );

		unsigned char tgt_msb;

		if (src < 0.0 && tgt_lsb != 0x00)
			tgt_msb = static_cast<unsigned>((short) (src / 16.0) - 1);
		else
			tgt_msb = static_cast<unsigned>((short) src / 16.0);

		result << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(tgt_msb)
		       << setw(2) << hex << setfill('0')
		       << static_cast<unsigned>(tgt_lsb);
	}

	return result.str();
}

string EncodeBDA::encode()
{
	// prepare data
	string token;
	istringstream stream(m_data);
	vector<string> data;

	while (getline(stream, token, '.') != 0)
		data.push_back(token);

	ostringstream result;
	result << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL) - 2000);

	return result.str();
}

string EncodeHDA::encode()
{
	// prepare data
	string token;
	istringstream stream(m_data);
	vector<string> data;

	while (getline(stream, token, '.') != 0)
		data.push_back(token);

	ostringstream result;
	result << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL) - 2000);

	return result.str();
}

string EncodeBTI::encode()
{
	// prepare data
	string token;
	istringstream stream(m_data);
	vector<string> data;

	while (getline(stream, token, ':') != 0)
		data.push_back(token);

	ostringstream result;
	result << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << setw(2) << dec << setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL));

	return result.str();
}

string EncodeHTI::encode()
{
	// prepare data
	string token;
	istringstream stream(m_data);
	vector<string> data;

	while (getline(stream, token, ':') != 0)
		data.push_back(token);

	ostringstream result;
	result << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << setw(2) << hex << setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL));

	return result.str();
}

string EncodeBDY::encode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};
	short day = 7;

	for (short i = 0; i < 7; i++)
		if (strcasecmp(days[i], m_data.c_str()) == 0)
			day = i;

	ostringstream result;
	result << setw(2) << hex << setfill('0') << day;

	return result.str();
}

string EncodeHDY::encode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};
	short day = 8;

	for (short i = 0; i < 7; i++)
		if (strcasecmp(days[i], m_data.c_str()) == 0)
			day = i + 1;

	ostringstream result;
	result << setw(2) << hex << setfill('0') << day;

	return result.str();
}

string EncodeTTM::encode()
{
	// prepare data
	string token;
	istringstream stream(m_data);
	vector<string> data;

	while (getline(stream, token, ':') != 0)
		data.push_back(token);

	ostringstream result;
	result << setw(2) << hex << setfill('0')
	       << static_cast<short>( (strtod(data[0].c_str(), NULL) * 6)
				    + (strtod(data[1].c_str(), NULL) / 10) );

	return result.str();
}


/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 * crc calculations from http://www.mikrocontroller.net/topic/75698
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

#include "encode.h"
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>

namespace libebus
{


Encode::Encode(const std::string& data, const std::string& factor)
	: m_data(data)
{
	if ((factor.find_first_not_of("0123456789.") == std::string::npos) == true)
		m_factor = static_cast<float>(strtod(factor.c_str(), NULL));
	else
		m_factor = 1.0;
}


std::string EncodeHEX::encode()
{
	m_data.erase(std::remove_if(m_data.begin(), m_data.end(), isspace), m_data.end());

	return m_data;
}

std::string EncodeUCH::encode()
{
	std::ostringstream result;
	unsigned short src = static_cast<unsigned short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << std::setw(2) << std::hex << std::setfill('0') << src;

	return result.str().substr(result.str().length()-2,2);
}

std::string EncodeSCH::encode()
{
	std::ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127 || src > 127)
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0x80);
	else
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(src);

	return result.str().substr(result.str().length()-2,2);
}

std::string EncodeUIN::encode()
{
	std::ostringstream result;
	unsigned short src = static_cast<unsigned short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << std::setw(4) << std::hex << std::setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

std::string EncodeSIN::encode()
{
	std::ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);
	result << std::setw(4) << std::hex << std::setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

std::string EncodeULG::encode()
{
	std::ostringstream result;
	unsigned long src = static_cast<unsigned long>(strtod(m_data.c_str(), NULL) / m_factor);
	result << std::setw(8) << std::hex << std::setfill('0') << src;

	return result.str().substr(6,2) + result.str().substr(4,2) +
	       result.str().substr(2,2) + result.str().substr(0,2);
}

std::string EncodeSLG::encode()
{
	std::ostringstream result;
	int src = static_cast<int>(strtod(m_data.c_str(), NULL) / m_factor);
	result << std::setw(8) << std::hex << std::setfill('0') << src;

	return result.str().substr(6,2) + result.str().substr(4,2) +
	       result.str().substr(2,2) + result.str().substr(0,2);
}

std::string EncodeFLT::encode()
{
	std::ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) * 1000.0 / m_factor);
	result << std::setw(4) << std::hex << std::setfill('0') << src;

	return result.str().substr(2,2) + result.str().substr(0,2);
}

std::string EncodeSTR::encode()
{
	std::ostringstream result;

	for (size_t i = 0; i < m_data.length(); i++)
		result << std::setw(2) << std::hex << std::setfill('0') << static_cast<short>(m_data[i]);

	return result.str();
}

std::string EncodeBCD::encode()
{
	std::ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src > 99)
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0xFF);
	else
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>( ((src / 10) << 4) | (src % 10) );

	return result.str().substr(result.str().length()-2,2);
}

std::string EncodeD1B::encode()
{
	std::ostringstream result;
	short src = static_cast<short>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127 || src > 127)
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0x80);
	else
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(src);

	return result.str().substr(result.str().length()-2,2);
}

std::string EncodeD1C::encode()
{
	std::ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < 0.0 || src > 100.0)
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0xFF);
	else
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(src * 2.0);

	return result.str();
}

std::string EncodeD2B::encode()
{
	std::ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -127.999 || src > 127.999) {
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0x80)
		       << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0x00);
	} else {
		unsigned char tgt_lsb = static_cast<unsigned>((src - ((short) src)) * 256.0);
		unsigned char tgt_msb;

		if (src < 0.0 && tgt_lsb != 0x00)
			tgt_msb = static_cast<unsigned>((short) src - 1);
		else
			tgt_msb = static_cast<unsigned>((short) src);

		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(tgt_msb)
		       << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(tgt_lsb);
	}

	return result.str();
}

std::string EncodeD2C::encode()
{
	std::ostringstream result;
	float src = static_cast<float>(strtod(m_data.c_str(), NULL) / m_factor);

	if (src < -2047.999 || src > 2047.999) {
		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(0x80)
		       << std::setw(2) << std::hex << std::setfill('0')
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

		result << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(tgt_msb)
		       << std::setw(2) << std::hex << std::setfill('0')
		       << static_cast<unsigned>(tgt_lsb);
	}

	return result.str();
}

std::string EncodeBDA::encode()
{
	// prepare data
	std::string token;
	std::istringstream stream(m_data);
	std::vector<std::string> data;

	while (std::getline(stream, token, '.') != 0)
		data.push_back(token);

	std::ostringstream result;
	result << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL) - 2000);

	return result.str();
}

std::string EncodeHDA::encode()
{
	// prepare data
	std::string token;
	std::istringstream stream(m_data);
	std::vector<std::string> data;

	while (std::getline(stream, token, '.') != 0)
		data.push_back(token);

	std::ostringstream result;
	result << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL) - 2000);

	return result.str();
}

std::string EncodeBTI::encode()
{
	// prepare data
	std::string token;
	std::istringstream stream(m_data);
	std::vector<std::string> data;

	while (std::getline(stream, token, ':') != 0)
		data.push_back(token);

	std::ostringstream result;
	result << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << std::setw(2) << std::dec << std::setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL));

	return result.str();
}

std::string EncodeHTI::encode()
{
	// prepare data
	std::string token;
	std::istringstream stream(m_data);
	std::vector<std::string> data;

	while (std::getline(stream, token, ':') != 0)
		data.push_back(token);

	std::ostringstream result;
	result << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[0].c_str(), NULL))
	       << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[1].c_str(), NULL))
	       << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>(strtod(data[2].c_str(), NULL));

	return result.str();
}

std::string EncodeBDY::encode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};
	short day = 7;

	for (short i = 0; i < 7; i++)
		if (strcasecmp(days[i], m_data.c_str()) == 0)
			day = i;

	std::ostringstream result;
	result << std::setw(2) << std::hex << std::setfill('0') << day;

	return result.str();
}

std::string EncodeHDY::encode()
{
	const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Err"};
	short day = 8;

	for (short i = 0; i < 7; i++)
		if (strcasecmp(days[i], m_data.c_str()) == 0)
			day = i + 1;

	std::ostringstream result;
	result << std::setw(2) << std::hex << std::setfill('0') << day;

	return result.str();
}

std::string EncodeTTM::encode()
{
	// prepare data
	std::string token;
	std::istringstream stream(m_data);
	std::vector<std::string> data;

	while (std::getline(stream, token, ':') != 0)
		data.push_back(token);

	std::ostringstream result;
	result << std::setw(2) << std::hex << std::setfill('0')
	       << static_cast<short>( (strtod(data[0].c_str(), NULL) * 6)
				    + (strtod(data[1].c_str(), NULL) / 10) );

	return result.str();
}


} //namespace


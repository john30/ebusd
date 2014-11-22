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

#include "command.h"
#include "decode.h"
#include "encode.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>

string Command::calcData()
{
	// encode - only first entry will be encoded
	// ToDo: if more parts are needed, they will be implemented
	encode(m_data, m_command[13], m_command[14]);

	if (m_error.length() > 0)
		m_result = m_error;

	return m_result;
}

string Command::calcResult(const cmd_t& cmd)
{
	int elements = strtol(m_command[9].c_str(), NULL, 10);

	if (cmd.size() > 3) {
		bool found = false;

		for (size_t i = 3; i < cmd.size(); i++) {
			int j;

			for (j = 0; j < elements; j++) {
				if (m_command[10 + j*8] == cmd[i]) {
					found = true;
					break;
				}
			}

			if (found == true) {
				found = false;

				// decode
				calcSub(m_command[11 + j*8], m_command[12 + j*8],
					m_command[13 + j*8], m_command[14 + j*8]);
			}

		}

	} else {
		for (int j = 0; j < elements; j++) {

			// decode
			calcSub(m_command[11 + j*8], m_command[12 + j*8],
				m_command[13 + j*8], m_command[14 + j*8]);
		}
	}

	if (m_error.length() > 0)
		m_result = m_error;

	return m_result;
}

void Command::calcSub(const string& part, const string& position,
		      const string& type, const string& factor)
{
	string data;

	// Master Data
	if (strcasecmp(part.c_str(), "MD") == 0) {
		// QQ ZZ PB SB NN
		int md_pos = 10;
		int md_len = strtol(m_command[7].c_str(), NULL, 10)*2;
		data = m_data.substr(md_pos, md_len);
	}

	// Slave Acknowledge
	else if (strcasecmp(part.c_str(), "SA") == 0) {
		// QQ ZZ PB SB NN + Dx + CRC
		int sa_pos = 10 + (strtol(m_command[7].c_str(), NULL, 10)*2) + 2;
		int sa_len = 2;
		data = m_data.substr(sa_pos, sa_len);
	}

	// Slave Data
	else if (strcasecmp(part.c_str(), "SD") == 0) {
		// QQ ZZ PB SB NN + Dx + CRC ACK NN
		int sd_pos = 10 + (strtol(m_command[7].c_str(), NULL, 10)*2) + 6;
		int sd_len = m_data.length() - (10 + (strtol(m_command[7].c_str(), NULL, 10)*2) + 6) - 4;
		data = m_data.substr(sd_pos, sd_len);
	}

	// Master Acknowledge
	else if (strcasecmp(part.c_str(), "MA") == 0) {
		// QQ ZZ PB SB NN + Dx + CRC ACK NN + Dx
		int ma_pos = m_data.length() - 2;
		int ma_len = 2;
		data = m_data.substr(ma_pos, ma_len);
	}

	decode(data, position, type, factor);
}

void Command::decode(const string& data, const string& position,
		     const string& type, const string& factor)
{
	ostringstream result, value;
	Decode* help = NULL;

	// prepare position
	string token;
	istringstream stream(position);
	vector<int> pos;

	while (getline(stream, token, ',') != 0)
		pos.push_back(strtol(token.c_str(), NULL, 10));

	if (strcasecmp(type.c_str(), "HEX") == 0) {
		if (pos.size() <= 1 || pos[1] < pos[0])
			pos[1] = pos[0];

		value << data.substr((pos[0]-1)*2, (pos[1]-pos[0]+1)*2);
		help = new DecodeHEX(value.str());
	}
	else if (strcasecmp(type.c_str(), "UCH") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeUCH(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "SCH") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeSCH(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "UIN") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2);
		help = new DecodeUIN(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "SIN") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2);
		help = new DecodeSIN(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "ULG") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2) << data.substr((pos[3]-1)*2, 2);
		help = new DecodeULG(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "SLG") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2) << data.substr((pos[3]-1)*2, 2);
		help = new DecodeSLG(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "FLT") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2);
		help = new DecodeFLT(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "STR") == 0) {
		if (pos.size() <= 1 || pos[1] < pos[0])
			pos[1] = pos[0];

		value << data.substr((pos[0]-1)*2, (pos[1]-pos[0]+1)*2);
		help = new DecodeSTR(value.str());
	}
	else if (strcasecmp(type.c_str(), "BCD") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeBCD(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "D1B") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeD1B(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "D1C") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeD1C(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "D2B") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2);
		help = new DecodeD2B(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "D2C") == 0) {
		value << data.substr((pos[0]-1)*2, 2) << data.substr((pos[1]-1)*2, 2);
		help = new DecodeD2C(value.str(), factor);
	}
	else if (strcasecmp(type.c_str(), "BDA") == 0) {
		value << data.substr((pos[0]-1)*2, 2)
		      << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2);
		help = new DecodeBDA(value.str());
	}
	else if (strcasecmp(type.c_str(), "HDA") == 0) {
		value << data.substr((pos[0]-1)*2, 2)
		      << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2);
		help = new DecodeHDA(value.str());
	}
	else if (strcasecmp(type.c_str(), "BTI") == 0) {
		value << data.substr((pos[0]-1)*2, 2)
		      << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2);
		help = new DecodeBTI(value.str());
	}
	else if (strcasecmp(type.c_str(), "HTI") == 0) {
		value << data.substr((pos[0]-1)*2, 2)
		      << data.substr((pos[1]-1)*2, 2)
		      << data.substr((pos[2]-1)*2, 2);
		help = new DecodeHTI(value.str());
	}
	else if (strcasecmp(type.c_str(), "BDY") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeBDY(value.str());
	}
	else if (strcasecmp(type.c_str(), "HDY") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeHDY(value.str());
	}
	else if (strcasecmp(type.c_str(), "TTM") == 0) {
		value << data.substr((pos[0]-1)*2, 2);
		help = new DecodeTTM(value.str());
	}

	if (help == NULL) {
		result << "type '" << type.c_str() << "' not implemented!";
		m_error = result.str();
	} else {
		result << help->decode();

		if (m_result.length() > 0)
			m_result += " ";

		m_result += result.str();
	}

	delete help;
}

void Command::encode(const string& data, const string& type,
		     const string& factor)
{
	ostringstream result;
	Encode* help = NULL;

	if (strcasecmp(type.c_str(), "HEX") == 0) {
		help = new EncodeHEX(data);
	}
	else if (strcasecmp(type.c_str(), "UCH") == 0) {
		help = new EncodeUCH(data, factor);
	}
	else if (strcasecmp(type.c_str(), "SCH") == 0) {
		help = new EncodeSCH(data, factor);
	}
	else if (strcasecmp(type.c_str(), "UIN") == 0) {
		help = new EncodeUIN(data, factor);
	}
	else if (strcasecmp(type.c_str(), "SIN") == 0) {
		help = new EncodeSIN(data, factor);
	}
	else if (strcasecmp(type.c_str(), "ULG") == 0) {
		help = new EncodeULG(data, factor);
	}
	else if (strcasecmp(type.c_str(), "SLG") == 0) {
		help = new EncodeSLG(data, factor);
	}
	else if (strcasecmp(type.c_str(), "FLT") == 0) {
		help = new EncodeSLG(data, factor);
	}
	else if (strcasecmp(type.c_str(), "STR") == 0) {
		help = new EncodeSTR(data);
	}
	else if (strcasecmp(type.c_str(), "BCD") == 0) {
		help = new EncodeBCD(data, factor);
	}
	else if (strcasecmp(type.c_str(), "D1B") == 0) {
		help = new EncodeD1B(data, factor);
	}
	else if (strcasecmp(type.c_str(), "D1C") == 0) {
		help = new EncodeD1C(data, factor);
	}
	else if (strcasecmp(type.c_str(), "D2B") == 0) {
		help = new EncodeD2B(data, factor);
	}
	else if (strcasecmp(type.c_str(), "D2C") == 0) {
		help = new EncodeD2C(data, factor);
	}
	else if (strcasecmp(type.c_str(), "BDA") == 0) {
		help = new EncodeBDA(data);
	}
	else if (strcasecmp(type.c_str(), "HDA") == 0) {
		help = new EncodeHDA(data);
	}
	else if (strcasecmp(type.c_str(), "BTI") == 0) {
		help = new EncodeBTI(data);
	}
	else if (strcasecmp(type.c_str(), "HTI") == 0) {
		help = new EncodeHTI(data);
	}
	else if (strcasecmp(type.c_str(), "BDY") == 0) {
		help = new EncodeBDY(data);
	}
	else if (strcasecmp(type.c_str(), "HDY") == 0) {
		help = new EncodeHDY(data);
	}
	else if (strcasecmp(type.c_str(), "TTM") == 0) {
		help = new EncodeTTM(data);
	}

	if (help == NULL) {
		result << "type '" << type.c_str() << "' not implemented!";
		m_error = result.str();
	} else {
		result << help->encode();

		if (m_result.length() > 0)
			m_result += " ";

		m_result += result.str();
	}

	delete help;
}


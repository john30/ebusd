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

#include "message.h"
#include "data.h"
#include "result.h"
#include "symbol.h"
#include <string>
#include <vector>
#include <cstring>

using namespace std;

result_t Message::create(vector<string>::iterator& it, const vector<string>::iterator end,
		const map<string, DataField*> templates, Message*& returnValue)
{
	result_t result;
	// [type];class;name;[comment];[QQ];ZZ;id;fields...
	if (it == end)
		return RESULT_ERR_EOF;

	const char* str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	bool isSetMessage, isActiveMessage;
	unsigned int pollPriority = 0;
	if (strcasecmp(str, "W") == 0) {
		isActiveMessage = true;
		isSetMessage = true;
	} else if (str[0] == 'C' || str[0] == 'c') {
		isActiveMessage = false;
		isSetMessage = str[1] == 'W' || str[1] == 'w';
	} else if (str[0] == 'P' || str[0] == 'p') {
		isActiveMessage = true;
		isSetMessage = false;
		if (str[1] == 0)
			pollPriority = 1;
		else {
			result_t result;
			pollPriority = parseInt(str+1, 10, 1, 9, result);
			if (result != RESULT_OK)
				return result;
		}
	} else {
		isActiveMessage = true;
		isSetMessage = false;
	}

	string clazz = *it++;
	if (it == end)
		return RESULT_ERR_EOF;

	string name = *it++;
	if (it == end)
		return RESULT_ERR_EOF;
	if (name.length() == 0)
		return RESULT_ERR_INVALID_ARG; // empty name

	string comment = *it++;
	if (it == end)
		return RESULT_ERR_EOF;

	str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	unsigned char srcAddress;
	if (*str == 0 || isActiveMessage == true)
		srcAddress = SYN; // no specific source defined, or ignore for active message
	else {
		srcAddress = parseInt(str, 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result;
		if (isMaster(srcAddress) == false)
			return RESULT_ERR_INVALID_ARG;
	}

	str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;

	unsigned char dstAddress = parseInt(str, 16, 0, 0xff, result);
	if (result != RESULT_OK)
		return result;
	if (isValidAddress(dstAddress) == false)
		return RESULT_ERR_INVALID_ARG;

	istringstream input(*it++); // message id (PBSB + optional master data)
	vector<unsigned char> id;
	string token;
	if (it == end)
		return RESULT_ERR_EOF;
	while (input.eof() == false) {
		while (input.peek() == ' ')
			input.get();
		if (input.eof() == true) // no more digits
			break;
		token.clear();
		token.push_back(input.get());
		if (input.eof() == true)
			return RESULT_ERR_INVALID_ARG; // too short hex
		token.push_back(input.get());

		unsigned char value = parseInt(token.c_str(), 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result; // invalid hex value
		id.push_back(value);
	}
	if (id.size() < 2 || id.size() > 6)
		return RESULT_ERR_INVALID_ARG; // missing/too short/too long ID

	DataField* data = NULL;
	result = DataField::create(it, end, templates, data, isSetMessage, dstAddress);
	if (result != RESULT_OK)
		return result;

	returnValue = new Message(clazz, name, isSetMessage, isActiveMessage, comment, srcAddress, dstAddress, id, data, pollPriority);
	return RESULT_OK;
}

result_t Message::prepare(const unsigned char srcAddress, SymbolString& masterData, istringstream& input, char separator)
{
	if (m_isActiveMessage == true) {
		masterData.clear();
		masterData.push_back(srcAddress, false);
		masterData.push_back(m_dstAddress, false);
		masterData.push_back(m_id[0], false);
		masterData.push_back(m_id[1], false);
		unsigned char addData = m_data->getLength(pt_masterData);
		masterData.push_back(m_id.size() - 2 + addData, false);
		for (size_t i=2; i<m_id.size(); i++)
			masterData.push_back(m_id[i], false);
		SymbolString slaveData;
		result_t result = m_data->write(input, masterData, m_id.size() - 2, slaveData, 0, separator);
		if (result != RESULT_OK)
			return result;
		masterData.push_back(masterData.getCRC(), false, false);
	}
	return RESULT_OK;
}

result_t Message::handle(SymbolString& masterData, SymbolString& slaveData,
		ostringstream& output, char separator, bool answer)
{
	if (m_isActiveMessage == true) {
		result_t result = m_data->read(masterData, m_id.size() - 2, slaveData, 0, output, false, separator);
		if (result != RESULT_OK)
			return result;
	}
	else if (answer == true) {
		istringstream input; // TODO create input from database of internal variables
		result_t result = m_data->write(input, masterData, m_id.size() - 2, slaveData, 0, separator);
		if (result != RESULT_OK)
			return result;
	}
	return RESULT_OK;
}

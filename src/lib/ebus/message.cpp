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

Message::Message(const string clazz, const string name, const bool isSet,
			const bool isActive, const string comment,
			const unsigned char srcAddress, const unsigned char dstAddress,
			const vector<unsigned char> id, DataField* data,
			const unsigned int pollPriority)
		: m_class(clazz), m_name(name), m_isSet(isSet),
		  m_isActive(isActive), m_comment(comment),
		  m_srcAddress(srcAddress), m_dstAddress(dstAddress),
		  m_id(id), m_data(data), m_pollPriority(pollPriority)
{
	int exp = 7;
	unsigned long long key = (unsigned long long)(id.size()-2) << (8 * exp + 5);
	if (isActive == true)
		key |= 0x1fLL << (8 * exp--);
	else
		key |= (unsigned long long)getMasterNumber(srcAddress) << (8 * exp--);
	key |= (unsigned long long)dstAddress << (8 * exp--);
	for (vector<unsigned char>::const_iterator it=id.begin(); it<id.end(); it++)
		key |= (unsigned long long)(*it) << (8 * exp--);
	m_key = key;
}

result_t Message::create(vector<string>::iterator& it, const vector<string>::iterator end,
		DataFieldTemplates* templates, Message*& returnValue)
{
	// [type];[class];name;[comment];[QQ];ZZ;id;fields...
	result_t result;
	bool isSet, isActive;
	unsigned int pollPriority = 0;
	if (it == end)
		return RESULT_ERR_EOF;

	const char* str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	if (strcasecmp(str, "W") == 0) {
		isActive = true;
		isSet = true;
	} else if (str[0] == 'C' || str[0] == 'c') {
		isActive = false;
		isSet = str[1] == 'W' || str[1] == 'w';
	} else if (str[0] == 'P' || str[0] == 'p') {
		isActive = true;
		isSet = false;
		if (str[1] == 0)
			pollPriority = 1;
		else {
			result_t result;
			pollPriority = parseInt(str+1, 10, 1, 9, result);
			if (result != RESULT_OK)
				return result;
		}
	} else {
		isActive = true;
		isSet = false;
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
	if (*str == 0 || isActive == true)
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
	result = DataField::create(it, end, templates, data, isSet, dstAddress);
	if (result != RESULT_OK)
		return result;

	returnValue = new Message(clazz, name, isSet, isActive, comment, srcAddress, dstAddress, id, data, pollPriority);
	return RESULT_OK;
}

result_t Message::prepare(const unsigned char srcAddress, SymbolString& masterData, istringstream& input, char separator)
{
	if (m_isActive == true) {
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
		return RESULT_OK;
	}
	return RESULT_ERR_INVALID_ARG; // prepare not possible
}

result_t Message::handle(SymbolString& masterData, SymbolString& slaveData,
		ostringstream& output, char separator, bool answer)
{
	if (m_isActive == true) {
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


result_t MessageMap::add(Message* message)
{
	string key = message->getClass().append(";").append(message->getName());
	if (message->isActive() == true)
		key.append(message->isSet() ? ";W" : ";R");
	else
		key.append(";C");
	map<string, Message*>::iterator nameIt = m_messagesByName.find(key);
	if (nameIt != m_messagesByName.end())
		return RESULT_ERR_INVALID_ARG; // duplicate key

	if (message->isActive() == false) {
		unsigned long long pkey = message->getKey();
		map<unsigned long long, Message*>::iterator keyIt = m_passiveMessagesByKey.find(pkey);
		if (keyIt != m_passiveMessagesByKey.end())
			return RESULT_ERR_DUPLICATE; // duplicate key

		unsigned char idLength = message->getId().size() - 2;
		if (idLength > m_maxIdLength)
			m_maxIdLength = idLength;
		m_passiveMessagesByKey[pkey] = message;
	}

	m_messagesByName[key] = message;

	return RESULT_OK;
}

result_t MessageMap::addFromFile(vector<string>& row, DataFieldTemplates* arg)
{
	Message* message = NULL;
	vector<string>::iterator it = row.begin();
	result_t result = Message::create(it, row.end(), arg, message);
	if (result != RESULT_OK)
		return result;

	result = add(message);
	if (result != RESULT_OK)
		delete message;

	return result;
}

Message* MessageMap::find(const string clazz, const string name, const bool isActive, const bool isSet)
{
	string key = clazz;
	for (int i=0; i<2; i++) {
		key.append(";").append(name);
		if (isActive == true)
			key.append(isSet ? ";W" : ";R");
		else
			key.append(";C");
		map<string, Message*>::iterator it = m_messagesByName.find(key);
		if (it != m_messagesByName.end())
			return it->second;
		key.clear(); // try again without class name
	}

	return NULL;
}

Message* MessageMap::find(SymbolString master) {
	if (master.size() < 5)
		return NULL;
	unsigned char maxIdLength =  master[4];
	if (maxIdLength > m_maxIdLength)
		maxIdLength = m_maxIdLength;
	if (master.size() < 5+maxIdLength)
		return NULL;

	unsigned long long sourceMask = 0x1fLL << (8 * 7);
	for (int idLength=maxIdLength; idLength>=0; idLength--) {
		int exp = 7;
		unsigned long long key = (unsigned long long)idLength << (8 * exp + 5);
		key |= (unsigned long long)getMasterNumber(master[0]) << (8 * exp--);
		key |= (unsigned long long)master[1] << (8 * exp--);
		key |= (unsigned long long)master[2] << (8 * exp--);
		key |= (unsigned long long)master[3] << (8 * exp--);
		for (unsigned char i=0; i<idLength; i++)
			key |= (unsigned long long)master[5 + i] << (8 * exp--);

		map<unsigned long long , Message*>::iterator it = m_passiveMessagesByKey.find(key);
		if (it != m_passiveMessagesByKey.end())
			return it->second;

		if ((key & sourceMask) != 0) {
			key &= ~sourceMask; // try again without specific source master
			it = m_passiveMessagesByKey.find(key);
			if (it != m_passiveMessagesByKey.end())
				return it->second;
		}
	}

	return NULL;
}

void MessageMap::clear()
{
	for (map<string, Message*>::iterator it=m_messagesByName.begin(); it!=m_messagesByName.end(); it++) {
		delete it->second;
		it->second = NULL;
	}
	m_messagesByName.clear();
	m_passiveMessagesByKey.clear();
	m_maxIdLength = 0;
}


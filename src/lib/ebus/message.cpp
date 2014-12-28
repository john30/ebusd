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

/** the bit mask of the source master number in the message key. */
#define ID_SOURCE_MASK (0x1fLL << (8 * 7))

Message::Message(const string clazz, const string name, const bool isSet,
		const bool isPassive, const string comment,
		const unsigned char srcAddress, const unsigned char dstAddress,
		const vector<unsigned char> id, DataField* data,
		const unsigned int pollPriority)
		: m_class(clazz), m_name(name), m_isSet(isSet),
		  m_isPassive(isPassive), m_comment(comment),
		  m_srcAddress(srcAddress), m_dstAddress(dstAddress),
		  m_id(id), m_data(data), m_pollPriority(pollPriority),
		  m_lastUpdateTime(0), m_pollCount(0), m_lastPollTime(0)
{
	int exp = 7;
	unsigned long long key = (unsigned long long)(id.size()-2) << (8 * exp + 5);
	if (isPassive == true)
		key |= (unsigned long long)getMasterNumber(srcAddress) << (8 * exp--); // 0..25
	else
		key |= 0x1fLL << (8 * exp--); // special value for active
	key |= (unsigned long long)dstAddress << (8 * exp--);
	for (vector<unsigned char>::const_iterator it=id.begin(); it<id.end(); it++)
		key |= (unsigned long long)*it << (8 * exp--);
	m_key = key;
}

Message::Message(const bool isSet, const bool isPassive,
		const unsigned char pb, const unsigned char sb,
		DataField* data)
		: m_class(), m_name(), m_isSet(isSet),
		  m_isPassive(isPassive), m_comment(),
		  m_srcAddress(SYN), m_dstAddress(SYN),
		  m_data(data), m_pollPriority(0),
		  m_lastUpdateTime(0), m_pollCount(0), m_lastPollTime(0)
{
	m_id.push_back(pb);
	m_id.push_back(sb);
	m_key = 0;
}

/**
 * @brief Helper method for getting a default if the value is empty.
 * @param value the value to check.
 * @param defaults a @a vector of defaults, or NULL.
 * @param pos the position in defaults.
 * @return the default if available and value is empty, or the value.
 */
string getDefault(const string value, vector<string>* defaults, size_t pos)
{
	if (value.length() > 0 || defaults == NULL || pos > defaults->size()) {
		return value;
	}

	return defaults->at(pos);
}

result_t Message::create(vector<string>::iterator& it, const vector<string>::iterator end,
		vector< vector<string> >* defaultsRows,
		DataFieldTemplates* templates, Message*& returnValue)
{
	// [type],[class],name,[comment],[QQ],[ZZ],id,fields...
	result_t result;
	bool isSet = false, isPassive = false;
	string defaultName;
	unsigned int pollPriority = 0;
	size_t defaultPos = 1;
	if (it == end)
		return RESULT_ERR_EOF;

	const char* str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	size_t len = strlen(str);
	if (len == 0) { // default: active get
		defaultName = "r";
	} else if (strncasecmp(str, "R", 1) == 0) { // active get
		char last = str[len-1];
		if (last >= '0' && last <= '9') { // poll priority (=active get)
			pollPriority = last - '0';
			defaultName = string(str).substr(0, len-1); // cut off priority digit
		}
		else
			defaultName = str;
	} else if (strncasecmp(str, "W", 1) == 0) { // active set
		isSet = true;
		defaultName = str;
	} else { // any other: passive set/get
		isPassive = true;
		isSet = strcasecmp(str+len-1, "W") == 0; // if type ends with "w" it is treated as passive set
		defaultName = str;
	}

	vector<string>* defaults = NULL;
	if (defaultsRows != NULL && defaultsRows->size() > 0) {
		for (vector< vector<string> >::reverse_iterator it = defaultsRows->rbegin(); it != defaultsRows->rend(); it++) {
			string check = (*it)[0];
			if (check == defaultName) {
				defaults = &(*it);
				break;
			}
		}
	}

	string clazz = getDefault(*it++, defaults, defaultPos++);
	if (it == end)
		return RESULT_ERR_EOF;

	string name = *it++;
	if (it == end)
		return RESULT_ERR_EOF;
	if (name.length() == 0)
		return RESULT_ERR_INVALID_ARG; // empty name
	defaultPos++;

	string comment = getDefault(*it++, defaults, defaultPos++);
	if (it == end)
		return RESULT_ERR_EOF;

	str = getDefault(*it++, defaults, defaultPos++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	unsigned char srcAddress;
	if (*str == 0)
		srcAddress = SYN; // no specific source
	else {
		srcAddress = parseInt(str, 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result;
		if (isMaster(srcAddress) == false)
			return RESULT_ERR_INVALID_ADDR;
	}

	str = getDefault(*it++, defaults, defaultPos++).c_str();
	if (it == end)
		 return RESULT_ERR_EOF;
	unsigned char dstAddress;
	if (*str == 0)
		dstAddress = SYN; // no specific destination
	else {
		dstAddress = parseInt(str, 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result;
		if (isValidAddress(dstAddress) == false)
			return RESULT_ERR_INVALID_ADDR;
	}

	vector<unsigned char> id;
	for (int pos=0, useDefaults=1; pos<2; pos++) { // message id (PBSB, optional master data)
		string token = *it++;
		if (useDefaults == 1) {
			if (pos == 0 && token.size() > 0) {
				useDefaults = 0;
			} else {
				token = getDefault("", defaults, defaultPos).append(token);
			}
		}
		istringstream input(token);
		if (it == end)
			return RESULT_ERR_EOF;
		while (input.eof() == false) {
			while (input.peek() == ' ')
				input.get();
			if (input.eof() == true) // no more digits
				break;
			token.clear();
			token.push_back(input.get());
			if (input.eof() == true) {
				return RESULT_ERR_INVALID_ARG; // too short hex
			}
			token.push_back(input.get());

			unsigned char value = parseInt(token.c_str(), 16, 0, 0xff, result);
			if (result != RESULT_OK) {
				return result; // invalid hex value
			}
			id.push_back(value);
		}
		if (pos == 0 && id.size() != 2) {
			return RESULT_ERR_INVALID_ARG; // missing/too short/too PBSB
		}
		defaultPos++;
	}
	if (id.size() < 2 || id.size() > 6) {
		return RESULT_ERR_INVALID_ARG; // missing/too short/too long ID
	}

	vector<string>::iterator realEnd = end;
	vector<string> newTypes;
	if (defaults!=NULL && defaults->size() > defaultPos + 2) { // need at least "[name];[part];type" (optional: "[divisor|values][;[unit][;[comment]]]]")
		while (defaults->size() > defaultPos + 2 && defaults->at(defaultPos + 2).size() > 0) {
			for (size_t i = 0; i < 6; i++) {
				if (defaults->size() > defaultPos)
					newTypes.push_back(defaults->at(defaultPos));
				else
					newTypes.push_back("");

				defaultPos++;
			}
		}
		if (newTypes.size() > 0) {
			while (it != end) {
				newTypes.push_back(*it++);
			}
			it = newTypes.begin();
			realEnd = newTypes.end();
		}
	}
	DataField* data = NULL;
	result = DataField::create(it, realEnd, templates, data, isSet, dstAddress==SYN ? ESC : dstAddress);
	if (result != RESULT_OK) {
		return result;
	}
	returnValue = new Message(clazz, name, isSet, isPassive, comment, srcAddress, dstAddress, id, data, pollPriority);
	return RESULT_OK;
}

result_t Message::prepareMaster(const unsigned char srcAddress, SymbolString& masterData, istringstream& input, char separator, const unsigned char dstAddress)
{
	if (m_isPassive == true)
		return RESULT_ERR_INVALID_ARG; // prepare not possible

	SymbolString master;
	result_t result = master.push_back(srcAddress, false, false);
	if (result != RESULT_OK)
		return result;
	if (dstAddress == SYN) {
		if (m_dstAddress == SYN)
			return RESULT_ERR_INVALID_ADDR;
		result = master.push_back(m_dstAddress, false, false);
	} else
		result = master.push_back(dstAddress, false, false);
	if (result != RESULT_OK)
		return result;
	result = master.push_back(m_id[0], false, false);
	if (result != RESULT_OK)
		return result;
	result = master.push_back(m_id[1], false, false);
	if (result != RESULT_OK)
		return result;
	unsigned char addData = m_data->getLength(pt_masterData);
	result = master.push_back(m_id.size() - 2 + addData, false, false);
	if (result != RESULT_OK)
		return result;
	for (size_t i=2; i<m_id.size(); i++) {
		result = master.push_back(m_id[i], false, false);
		if (result != RESULT_OK)
			return result;
	}
	result = m_data->write(input, pt_masterData, master, m_id.size() - 2, separator);
	if (result != RESULT_OK)
		return result;
	masterData = SymbolString(master, true);
	return result;
}

result_t Message::prepareSlave(SymbolString& slaveData)
{
	if (m_isPassive == false || m_isSet == true)
			return RESULT_ERR_INVALID_ARG; // prepare not possible

	SymbolString slave;
	unsigned char addData = m_data->getLength(pt_slaveData);
	result_t result = slave.push_back(addData, false, false);
	if (result != RESULT_OK)
		return result;
	istringstream input;
	result = m_data->write(input, pt_slaveData, slave, 0);
	if (result != RESULT_OK)
		return result;
	slaveData = SymbolString(slave, true);
	return result;
}

result_t Message::decode(const PartType partType, SymbolString& data,
		ostringstream& output, bool leadingSeparator,
		bool verbose, const char* filterName,
		char separator)
{
	unsigned char offset;
	if (partType == pt_masterData)
		offset = m_id.size() - 2;
	else
		offset = 0;
	int startPos = output.str().length();
	result_t result = m_data->read(partType, data, offset, output, leadingSeparator, verbose, filterName, separator);
	time(&m_lastUpdateTime);
	if (result < RESULT_OK) {
		m_lastValue.clear();
		return result;
	}
	m_lastValue = output.str().substr(startPos);
	/*if (m_isPassive == false && answer == true) {
		istringstream input; // TODO create input from database of internal variables
		result_t result = m_data->write(input, masterData, m_id.size() - 2, slaveData, 0, separator);
		if (result != RESULT_OK)
			return result;
	}*/
	return RESULT_OK;
}

bool Message::isLessPollWeight(Message* other) {
	if (m_pollPriority * m_pollCount < other->m_pollPriority * other->m_pollCount)
		return true;
	if (m_pollPriority < other->m_pollPriority)
		return true;
	if (m_lastPollTime < other->m_lastPollTime)
		return true;

	return false;
}


result_t MessageMap::add(Message* message)
{
	unsigned long long key = message->getKey();
	map<unsigned long long, Message*>::iterator keyIt = m_messagesByKey.find(key);
	if (keyIt != m_messagesByKey.end()) {
		return RESULT_ERR_DUPLICATE; // duplicate key
	}
	bool isPassive = message->isPassive();
	bool isSet = message->isSet();
	string clazz = message->getClass();
	string name = message->getName();
	string nameKey = string(isPassive ? "P" : (isSet ? "W" : "R")) + clazz + FIELD_SEPARATOR + name;
	map<string, Message*>::iterator nameIt = m_messagesByName.find(nameKey);
	if (nameIt != m_messagesByName.end()) {
		return RESULT_ERR_DUPLICATE; // duplicate key
	}

	m_messagesByName[nameKey] = message;
	m_messageCount++;
	if (isPassive == true)
		m_passiveMessageCount++;

	nameKey = string(isPassive ? "-P" : (isSet ? "-W" : "-R")) + name; // also store without class
	nameIt = m_messagesByName.find(nameKey);
	if (nameIt == m_messagesByName.end()) {
		m_messagesByName[nameKey] = message; // only store first key without class
	}

	unsigned char idLength = message->getId().size() - 2;
	if (idLength < m_minIdLength)
		m_minIdLength = idLength;
	if (idLength > m_maxIdLength)
		m_maxIdLength = idLength;
	m_messagesByKey[key] = message;

	if (message->getPollPriority() > 0)
		m_pollMessages.push(message);

	return RESULT_OK;
}

result_t MessageMap::addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end, DataFieldTemplates* arg, vector< vector<string> >* defaults, const string& filename, unsigned int lineNo)
{
	vector<string>::iterator restart = begin;
	string types = *restart;
	if (types.length() == 0)
		types.append("r");
	result_t result = RESULT_ERR_EOF;

	istringstream stream(types);
	string type;
	while (getline(stream, type, VALUE_SEPARATOR) != 0) {
		*restart = type;
		begin = restart;
		Message* message = NULL;
		result = Message::create(begin, end, defaults, arg, message);
		if (result != RESULT_OK)
			return result;
		result = add(message);
		if (result != RESULT_OK) {
			delete message;
			return result;
		}
		begin = restart;
	}
	return result;
}

Message* MessageMap::find(const string& clazz, const string& name, const bool isSet, const bool isPassive)
{
	for (int i=0; i<2; i++) {
		string key;
		if (i==0)
			key = string(isPassive ? "P" : (isSet ? "W" : "R")) + clazz + FIELD_SEPARATOR + name;
		else if (clazz.length() == 0)
			key = string(isPassive ? "-P" : (isSet ? "-W" : "-R")) + name; // second try: without class
		else
			continue; // not allowed without class
		map<string, Message*>::iterator it = m_messagesByName.find(key);
		if (it != m_messagesByName.end())
			return it->second;
	}

	return NULL;
}

deque<Message*> MessageMap::findAll(const string& clazz, const string& name, const short pb, const bool completeMatch)
{
	deque<Message*> ret;

	bool checkClass = clazz.length() > 0;
	bool checkName = name.length() > 0;
	bool checkPb = pb >= 0;
	for (map<string, Message*>::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // avoid duplicates: instances stored multiple times have a key starting with "-"
			continue;
		Message* message = it->second;
		if (checkClass == true) {
			string check = message->getClass();
			if (completeMatch ? (check != clazz) : (check.find(clazz) == check.npos))
				continue;
		}
		if (checkName == true) {
			string check = message->getName();
			if (completeMatch ? (check != name) : (check.find(name) == check.npos))
				continue;
		}
		if (checkPb == true && message->getId()[0] != pb)
			continue;
		ret.push_back(message);
	}

	return ret;
}

Message* MessageMap::find(SymbolString& master)
{
	if (master.size() < 5)
		return NULL;
	unsigned char maxIdLength = master[4];
	if (maxIdLength < m_minIdLength)
		return NULL;
	if (maxIdLength > m_maxIdLength)
		maxIdLength = m_maxIdLength;
	if (master.size() < 5+maxIdLength)
		return NULL;

	for (int idLength = maxIdLength; idLength >= m_minIdLength; idLength--) {
		int exp = 7;
		unsigned long long key = (unsigned long long)idLength << (8 * exp + 5);
		key |= (unsigned long long)getMasterNumber(master[0]) << (8 * exp--);
		key |= (unsigned long long)master[1] << (8 * exp--);
		key |= (unsigned long long)master[2] << (8 * exp--);
		key |= (unsigned long long)master[3] << (8 * exp--);
		for (unsigned char i=0; i<idLength; i++)
			key |= (unsigned long long)master[5 + i] << (8 * exp--);

		map<unsigned long long , Message*>::iterator it = m_messagesByKey.find(key);
		if (it != m_messagesByKey.end())
			return it->second;

		if ((key & ID_SOURCE_MASK) != 0) {
			it = m_messagesByKey.find(key & ~ID_SOURCE_MASK); // try again without specific source master
			if (it != m_messagesByKey.end())
				return it->second;
		}

		it = m_messagesByKey.find(key | ID_SOURCE_MASK); // try again with special value for active
		if (it != m_messagesByKey.end())
			return it->second;
	}

	return NULL;
}

void MessageMap::clear()
{
	// clear poll messages
	while (m_pollMessages.empty() == false) {
		m_pollMessages.top();
		m_pollMessages.pop();
	}
	// free message instances
	for (map<string, Message*>::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] != '-') // avoid double free: instances stored multiple times have a key starting with "-"
			delete it->second;
		it->second = NULL;
	}
	// clear messages by name
	m_messageCount = 0;
	m_messagesByName.clear();
	// clear messages by key
	m_messagesByKey.clear();
	m_minIdLength = 4;
	m_maxIdLength = 0;
}

Message* MessageMap::getNextPoll()
{
	if (m_pollMessages.empty() == true)
		return NULL;
	Message* ret = m_pollMessages.top();
	m_pollMessages.pop();
	ret->m_pollCount++;
	time(&(ret->m_lastPollTime));
	m_pollMessages.push(ret); // re-insert at new position
	return ret;
}

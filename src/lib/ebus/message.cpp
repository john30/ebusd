/*
 * Copyright (C) John Baier 2014-2015 <ebusd@ebusd.eu>
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
#include <algorithm>
#include <locale>
#include <iomanip>

using namespace std;

/** the bit mask of the source master number in the message key. */
#define ID_SOURCE_MASK (0x1fLL << (8 * 7))

Message::Message(const string circuit, const string name, const bool isWrite,
		const bool isPassive, const string comment,
		const unsigned char srcAddress, const unsigned char dstAddress,
		const vector<unsigned char> id, DataField* data, const bool deleteData,
		const unsigned char pollPriority)
		: m_circuit(circuit), m_name(name), m_isWrite(isWrite),
		  m_isPassive(isPassive), m_comment(comment),
		  m_srcAddress(srcAddress), m_dstAddress(dstAddress),
		  m_id(id), m_data(data), m_deleteData(deleteData),
		  m_pollPriority(pollPriority),
		  m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0)
{
	int exp = 7;
	unsigned long long key = (unsigned long long)(id.size()-2) << (8 * exp + 5);
	if (isPassive)
		key |= (unsigned long long)getMasterNumber(srcAddress) << (8 * exp--); // 0..25
	else
		key |= 0x1fLL << (8 * exp--); // special value for active
	key |= (unsigned long long)dstAddress << (8 * exp--);
	for (vector<unsigned char>::const_iterator it = id.begin(); it < id.end(); it++)
		key |= (unsigned long long)*it << (8 * exp--);
	m_key = key;
}

Message::Message(const bool isWrite, const bool isPassive,
		const unsigned char pb, const unsigned char sb,
		DataField* data)
		: m_circuit(), m_name(), m_isWrite(isWrite),
		  m_isPassive(isPassive), m_comment(),
		  m_srcAddress(SYN), m_dstAddress(SYN),
		  m_data(data), m_deleteData(true), m_pollPriority(0),
		  m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0)
{
	m_id.push_back(pb);
	m_id.push_back(sb);
	m_key = 0;
}

/**
 * Helper method for getting a default if the value is empty.
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
		DataFieldTemplates* templates, vector<Message*>& messages)
{
	// [type],[circuit],name,[comment],[QQ[;QQ]*],[ZZ],id,fields...
	result_t result;
	bool isWrite = false, isPassive = false;
	string defaultName;
	unsigned char pollPriority = 0;
	size_t defaultPos = 1;
	if (it == end)
		return RESULT_ERR_EOF;

	const char* str = (*it++).c_str();
	if (it == end)
		return RESULT_ERR_EOF;
	size_t len = strlen(str);
	if (len == 0) { // default: active read
		defaultName = "r";
	} else {
		defaultName = str;
		char type = str[0];
		if (type == 'r' || type == 'R') { // active read
			char poll = str[1];
			if (poll >= '0' && poll <= '9') { // poll priority (=active read)
				pollPriority = (unsigned char)(poll - '0');
				defaultName.erase(1, 1); // cut off priority digit
			}
		}
		else if (type == 'w' || type == 'W') { // active write
			isWrite = true;
		}
		else { // any other: passive read/write
			isPassive = true;
			type = str[1];
			isWrite = type == 'w' || type == 'W'; // if type continues with "w" it is treated as passive write
		}
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

	string circuit = getDefault(*it++, defaults, defaultPos++);
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
		srcAddress = (unsigned char)parseInt(str, 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result;
		if (!isMaster(srcAddress))
			return RESULT_ERR_INVALID_ADDR;
	}

	str = getDefault(*it++, defaults, defaultPos++).c_str();
	if (it == end)
		 return RESULT_ERR_EOF;
	vector<unsigned char> dstAddresses;
	bool isBroadcastOrMasterDestination = false;
	if (*str == 0) {
		dstAddresses.push_back(SYN); // no specific destination
	} else {
		istringstream stream(str);
		string token;
		bool first = true;
		while (getline(stream, token, VALUE_SEPARATOR) != 0) {
			unsigned char dstAddress = (unsigned char)parseInt(token.c_str(), 16, 0, 0xff, result);
			if (result != RESULT_OK)
				return result;
			if (!isValidAddress(dstAddress))
				return RESULT_ERR_INVALID_ADDR;
			bool broadcastOrMaster = (dstAddress == BROADCAST) || isMaster(dstAddress);
			if (first) {
				isBroadcastOrMasterDestination = broadcastOrMaster;
				first = false;
			} else if (isBroadcastOrMasterDestination != broadcastOrMaster)
				return RESULT_ERR_INVALID_ADDR;
			dstAddresses.push_back(dstAddress);
		}
	}

	vector<unsigned char> id;
	bool useDefaults = true;
	for (int pos = 0; pos < 2 && it != end; pos++) { // message id (PBSB, optional master data)
		string token = *it++;
		if (useDefaults) {
			if (pos == 0 && token.size() > 0)
				useDefaults = false;
			else
				token = getDefault("", defaults, defaultPos).append(token);
		}
		istringstream input(token);
		while (!input.eof()) {
			while (input.peek() == ' ')
				input.get();
			if (input.eof()) // no more digits
				break;
			token.clear();
			token.push_back((char)input.get());
			if (input.eof()) {
				return RESULT_ERR_INVALID_ARG; // too short hex
			}
			token.push_back((char)input.get());

			unsigned char value = (unsigned char)parseInt(token.c_str(), 16, 0, 0xff, result);
			if (result != RESULT_OK) {
				return result; // invalid hex value
			}
			id.push_back(value);
		}
		if (pos == 0 && id.size() != 2)
			return RESULT_ERR_INVALID_ARG; // missing/too short/too PBSB

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
	if (it==realEnd) {
		vector<SingleDataField*> fields;
		data = new DataFieldSet("", "", fields);
	} else {
		result = DataField::create(it, realEnd, templates, data, isWrite, false, isBroadcastOrMasterDestination);
		if (result != RESULT_OK) {
			return result;
		}
	}
	if (id.size() + data->getLength(pt_masterData) > 2 + MAX_POS || data->getLength(pt_slaveData) > MAX_POS) {
		// max NN exceeded
		delete data;
		return RESULT_ERR_INVALID_POS;
	}
	unsigned int index = 0;
	bool multiple = dstAddresses.size()>1;
	char num[10];
	for (vector<unsigned char>::iterator it = dstAddresses.begin(); it != dstAddresses.end(); it++, index++) {
		unsigned char dstAddress = *it;
		string useCircuit = circuit;
		if (multiple) {
			sprintf(num, ".%d", index);
			useCircuit = useCircuit + num;
		}
		messages.push_back(new Message(useCircuit, name, isWrite, isPassive, comment, srcAddress, dstAddress, id, data, index==0, pollPriority));
	}
	return RESULT_OK;
}

bool Message::setPollPriority(unsigned char priority)
{
	if (priority == m_pollPriority || m_isPassive)
		return false;

	m_pollPriority = priority;
	return true;
}

result_t Message::prepareMaster(const unsigned char srcAddress, SymbolString& masterData, istringstream& input, char separator, const unsigned char dstAddress)
{
	if (m_isPassive)
		return RESULT_ERR_INVALID_ARG; // prepare not possible

	SymbolString master(false);
	result_t result = master.push_back(srcAddress, false, false);
	if (result != RESULT_OK)
		return result;
	if (dstAddress == SYN) {
		if (m_dstAddress == SYN)
			return RESULT_ERR_INVALID_ADDR;
		result = master.push_back(m_dstAddress, false, false);
	}
	else
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
	result = master.push_back((unsigned char)(m_id.size() - 2 + addData), false, false);
	if (result != RESULT_OK)
		return result;
	for (size_t i = 2; i < m_id.size(); i++) {
		result = master.push_back(m_id[i], false, false);
		if (result != RESULT_OK)
			return result;
	}
	result = m_data->write(input, pt_masterData, master, (unsigned char)(m_id.size() - 2), separator);
	if (result != RESULT_OK)
		return result;
	time(&m_lastUpdateTime);
	switch (master.compareMaster(m_lastMasterData)) {
	case 1: // completely different
		m_lastChangeTime = m_lastUpdateTime;
		m_lastMasterData = masterData;
		break;
	case 2: // only master address is different
		m_lastMasterData = masterData;
		break;
	}
	masterData.addAll(master);
	return result;
}

result_t Message::prepareSlave(SymbolString& slaveData)
{
	if (!m_isPassive || m_isWrite)
			return RESULT_ERR_INVALID_ARG; // prepare not possible

	SymbolString slave(false);
	unsigned char addData = m_data->getLength(pt_slaveData);
	result_t result = slave.push_back(addData, false, false);
	if (result != RESULT_OK)
		return result;
	istringstream input; // TODO create input from database of internal variables
	result = m_data->write(input, pt_slaveData, slave, 0);
	if (result != RESULT_OK)
		return result;
	time(&m_lastUpdateTime);
	if (slave != m_lastSlaveData) {
		m_lastChangeTime = m_lastUpdateTime;
		m_lastSlaveData = slave;
	}
	slaveData.addAll(slave);
	return result;
}

result_t Message::decode(const PartType partType, SymbolString& data,
		ostringstream& output, OutputFormat outputFormat,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	unsigned char offset;
	if (partType == pt_masterData)
		offset = (unsigned char)(m_id.size() - 2);
	else
		offset = 0;
	result_t result = m_data->read(partType, data, offset, output, outputFormat, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY && fieldName != NULL)
		return RESULT_ERR_NOTFOUND;

	time(&m_lastUpdateTime);
	if (partType == pt_masterData) {
		switch (data.compareMaster(m_lastMasterData)) {
		case 1: // completely different
			m_lastChangeTime = m_lastUpdateTime;
			m_lastMasterData = data;
			break;
		case 2: // only master address is different
			m_lastMasterData = data;
			break;
		}
	} else if (partType == pt_slaveData) {
		if (data != m_lastSlaveData) {
			m_lastChangeTime = m_lastUpdateTime;
			m_lastSlaveData = data;
		}
	}
	return result;
}

result_t Message::decode(SymbolString& masterData, SymbolString& slaveData,
		ostringstream& output, OutputFormat outputFormat,
		bool leadingSeparator)
{
	unsigned char offset = (unsigned char)(m_id.size() - 2);
	size_t startPos = output.str().length();
	result_t result = m_data->read(pt_masterData, masterData, offset, output, outputFormat, leadingSeparator, NULL, -1);
	if (result < RESULT_OK)
		return result;
	bool empty = result == RESULT_EMPTY;
	offset = 0;
	leadingSeparator = output.str().length() > startPos;
	result = m_data->read(pt_slaveData, slaveData, offset, output, outputFormat, leadingSeparator, NULL, -1);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY && !empty)
		result = RESULT_OK; // OK if at least one part was non-empty
	time(&m_lastUpdateTime);
	switch (masterData.compareMaster(m_lastMasterData)) {
	case 1: // completely different
		m_lastChangeTime = m_lastUpdateTime;
		m_lastMasterData = masterData;
		break;
	case 2: // only master address is different
		m_lastMasterData = masterData;
		break;
	}
	if (slaveData != m_lastSlaveData) {
		m_lastChangeTime = m_lastUpdateTime;
		m_lastSlaveData = slaveData;
	}
	return result;
}

result_t Message::decodeLastData(ostringstream& output, OutputFormat outputFormat,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	unsigned char offset = (unsigned char)(m_id.size() - 2);
	size_t startPos = output.str().length();
	result_t result = m_data->read(pt_masterData, m_lastMasterData, offset, output, outputFormat, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	bool empty = result == RESULT_EMPTY;
	offset = 0;
	leadingSeparator = output.str().length() > startPos;
	result = m_data->read(pt_slaveData, m_lastSlaveData, offset, output, outputFormat, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY && !empty)
		result = RESULT_OK; // OK if at least one part was non-empty
	else if (result == RESULT_EMPTY && fieldName != NULL)
		return RESULT_ERR_NOTFOUND;
	return result;
}

bool Message::isLessPollWeight(const Message* other)
{
	unsigned int tw = m_pollPriority * m_pollCount;
	unsigned int ow = other->m_pollPriority * other->m_pollCount;
	if (tw > ow)
		return true;
	if (tw < ow)
		return false;
	if (m_pollPriority > other->m_pollPriority)
		return true;
	if (m_pollPriority < other->m_pollPriority)
		return false;
	if (m_lastPollTime > other->m_lastPollTime)
		return true;

	return false;
}

void Message::dump(ostream& output)
{
	if (m_isPassive) {
		output << "u";
		if (m_isWrite)
			output << "w";
	} else if (m_isWrite)
		output << "w";
	else {
		output << "r";
		if (m_pollPriority>0)
			output << static_cast<unsigned>(m_pollPriority);
	}
	DataField::dumpString(output, m_circuit);
	DataField::dumpString(output, m_name);
	DataField::dumpString(output, m_comment);
	output << FIELD_SEPARATOR;
	if (m_srcAddress != SYN)
		output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_srcAddress);
	output << FIELD_SEPARATOR;
	if (m_dstAddress != SYN)
		output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_dstAddress);
	output << FIELD_SEPARATOR;
	unsigned int cnt = 0;
	for (vector<unsigned char>::const_iterator it = m_id.begin(); it < m_id.end(); it++) {
		if (cnt++ == 2)
			output << FIELD_SEPARATOR;
		output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
	}
	if (cnt <= 2)
		output << FIELD_SEPARATOR; // no further ID bytes besides PBSB
	output << FIELD_SEPARATOR;
	m_data->dump(output);
}


string strtolower(const string& str)
{
	string ret(str);
	transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
	return ret;
}

result_t MessageMap::add(Message* message)
{
	unsigned long long key = message->getKey();
	map<unsigned long long, Message*>::iterator keyIt = m_messagesByKey.find(key);
	if (keyIt != m_messagesByKey.end())
		return RESULT_ERR_DUPLICATE; // duplicate key

	bool isPassive = message->isPassive();
	bool isWrite = message->isWrite();
	string circuit = strtolower(message->getCircuit());
	string name = strtolower(message->getName());
	string nameKey = string(isPassive ? "P" : (isWrite ? "W" : "R")) + circuit + FIELD_SEPARATOR + name;
	map<string, Message*>::iterator nameIt = m_messagesByName.find(nameKey);
	if (nameIt != m_messagesByName.end())
		return RESULT_ERR_DUPLICATE; // duplicate key

	m_messagesByName[nameKey] = message;
	m_messageCount++;
	if (isPassive)
		m_passiveMessageCount++;

	nameKey = string(isPassive ? "-P" : (isWrite ? "-W" : "-R")) + name; // also store without circuit
	nameIt = m_messagesByName.find(nameKey);
	if (nameIt == m_messagesByName.end())
		m_messagesByName[nameKey] = message; // only store first key without circuit

	unsigned char idLength = (unsigned char)(message->getId().size() - 2);
	if (idLength < m_minIdLength)
		m_minIdLength = idLength;
	if (idLength > m_maxIdLength)
		m_maxIdLength = idLength;
	m_messagesByKey[key] = message;

	addPollMessage(message);

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
	vector<Message*> messages;
	while (getline(stream, type, VALUE_SEPARATOR) != 0) {
		*restart = type;
		begin = restart;
		messages.clear();
		result = Message::create(begin, end, defaults, arg, messages);
		for (vector<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
			Message* message = *it;
			if (result == RESULT_OK)
				result = add(message);
			if (result != RESULT_OK)
				delete message; // delete all remaining messages on error
		}
		if (result != RESULT_OK)
			return result;
		begin = restart;
	}
	return result;
}

Message* MessageMap::find(const string& circuit, const string& name, const bool isWrite, const bool isPassive)
{
	string lcircuit = strtolower(circuit);
	string lname = strtolower(name);
	for (int i = 0; i < 2; i++) {
		string key;
		if (i == 0)
			key = string(isPassive ? "P" : (isWrite ? "W" : "R")) + lcircuit + FIELD_SEPARATOR + lname;
		else if (lcircuit.length() == 0)
			key = string(isPassive ? "-P" : (isWrite ? "-W" : "-R")) + lname; // second try: without circuit
		else
			continue; // not allowed without circuit
		map<string, Message*>::iterator it = m_messagesByName.find(key);
		if (it != m_messagesByName.end())
			return it->second;
	}

	return NULL;
}

deque<Message*> MessageMap::findAll(const string& circuit, const string& name, const short pb, const bool completeMatch,
	const bool withRead, const bool withWrite, const bool withPassive)
{
	deque<Message*> ret;

	string lcircuit = strtolower(circuit);
	string lname = strtolower(name);
	bool checkCircuit = lcircuit.length() > 0;
	bool checkName = name.length() > 0;
	bool checkPb = pb >= 0;
	for (map<string, Message*>::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // avoid duplicates: instances stored multiple times have a key starting with "-"
			continue;
		Message* message = it->second;
		if (checkCircuit) {
			string check = strtolower(message->getCircuit());
			if (completeMatch ? (check != lcircuit) : (check.find(lcircuit) == check.npos))
				continue;
		}
		if (checkName) {
			string check = strtolower(message->getName());
			if (completeMatch ? (check != lname) : (check.find(lname) == check.npos))
				continue;
		}
		if (checkPb && message->getId()[0] != pb)
			continue;
		if (message->isPassive()) {
			if (!withPassive)
				continue;
		}
		else if (message->isWrite()) {
			if (!withWrite)
				continue;
		}
		else {
			if (!withRead)
				continue;
		}
		ret.push_back(message);
	}

	return ret;
}

Message* MessageMap::find(SymbolString& master)
{
	deque<Message*> ret = findAll(master);

	return ret.size()>0 ? ret.front() : NULL;
}

deque<Message*> MessageMap::findAll(SymbolString& master)
{
	deque<Message*> ret;

	if (master.size() < 5)
		return ret;
	unsigned char maxIdLength = master[4];
	if (maxIdLength < m_minIdLength)
		return ret;
	if (maxIdLength > m_maxIdLength)
		maxIdLength = m_maxIdLength;
	if (master.size() < 5+maxIdLength)
		return ret;

	for (int idLength = maxIdLength; ret.size()==0 && idLength >= m_minIdLength; idLength--) {
		int exp = 7;
		unsigned long long key = (unsigned long long)idLength << (8 * exp + 5);
		key |= (unsigned long long)getMasterNumber(master[0]) << (8 * exp--);
		key |= (unsigned long long)master[1] << (8 * exp--);
		key |= (unsigned long long)master[2] << (8 * exp--);
		key |= (unsigned long long)master[3] << (8 * exp--);
		for (unsigned char i = 0; i < idLength; i++)
			key |= (unsigned long long)master[5 + i] << (8 * exp--);

		map<unsigned long long , Message*>::iterator it = m_messagesByKey.find(key);
		if (it != m_messagesByKey.end()) {
			ret.push_back(it->second);
		}
		if ((key & ID_SOURCE_MASK) != 0) {
			it = m_messagesByKey.find(key & ~ID_SOURCE_MASK); // try again without specific source master
			if (it != m_messagesByKey.end())
				ret.push_back(it->second);
		}
		it = m_messagesByKey.find(key | ID_SOURCE_MASK); // try again with special value for active
		if (it != m_messagesByKey.end())
			ret.push_back(it->second);
	}

	return ret;
}

void MessageMap::invalidateCache(Message* message)
{
	message->m_lastUpdateTime = 0;
	string circuit = message->getCircuit();
	size_t pos = circuit.find('#');
	if (pos!=string::npos)
		circuit.resize(pos);
	string name = message->getName();
	deque<Message*> messages = findAll(circuit, name, -1, false, true, true, true);
	for (deque<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
		if (*it==message)
			continue;
		message = *it;
		if (name!=message->getName())
			continue; // check exact name
		string check = message->getCircuit();
		if (check!=circuit) {
			size_t pos = check.find('#');
			if (pos!=string::npos)
				check.resize(pos);
			if (check!=circuit)
				continue;
		}
		message->m_lastUpdateTime = 0;
	}
}

void MessageMap::addPollMessage(Message* message)
{
	if (message != NULL && message->getPollPriority() > 0) {
		message->m_lastPollTime = m_pollMessages.size();
		m_pollMessages.push(message);
	}
}

void MessageMap::clear()
{
	// clear poll messages
	while (!m_pollMessages.empty()) {
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
	m_passiveMessageCount = 0;
	m_messagesByName.clear();
	// clear messages by key
	m_messagesByKey.clear();
	m_minIdLength = 4;
	m_maxIdLength = 0;
}

Message* MessageMap::getNextPoll()
{
	if (m_pollMessages.empty())
		return NULL;
	Message* ret = m_pollMessages.top();
	m_pollMessages.pop();
	ret->m_pollCount++;
	time(&(ret->m_lastPollTime));
	m_pollMessages.push(ret); // re-insert at new position
	return ret;
}

void MessageMap::dump(ostream& output)
{
	bool first = true;
	for (map<string, Message*>::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // skip instances stored multiple times (key starting with "-")
			continue;
		Message* message = it->second;
		if (first)
			first = false;
		else
			cout << endl;
		message->dump(cout);
	}
	if (!first)
		cout << endl;
}

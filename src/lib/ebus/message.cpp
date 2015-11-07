/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2015 John Baier <ebusd@ebusd.eu>
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
#include <climits>

using namespace std;

/** the bit mask of the source master number in the message key. */
#define ID_SOURCE_MASK (0x1fLL << (8 * 7))

/** the maximum poll priority for a @a Message referred to by a @a Condition. */
#define POLL_PRIORITY_CONDITION 5

Message::Message(const string circuit, const string name, const bool isWrite,
		const bool isPassive, const string comment,
		const unsigned char srcAddress, const unsigned char dstAddress,
		const vector<unsigned char> id,
		DataField* data, const bool deleteData,
		const unsigned char pollPriority,
		Condition* condition)
		: m_circuit(circuit), m_name(name), m_isWrite(isWrite),
		  m_isPassive(isPassive), m_comment(comment),
		  m_srcAddress(srcAddress), m_dstAddress(dstAddress),
		  m_id(id), m_data(data), m_deleteData(deleteData),
		  m_pollPriority(pollPriority),
		  m_usedByCondition(false), m_condition(condition),
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
		DataField* data, const bool deleteData)
		: m_circuit(), m_name(), m_isWrite(isWrite),
		  m_isPassive(isPassive), m_comment(),
		  m_srcAddress(SYN), m_dstAddress(SYN),
		  m_data(data), m_deleteData(true),
		  m_pollPriority(0),
		  m_usedByCondition(false), m_condition(NULL),
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
	if (value.length() > 0 || defaults == NULL || pos >= defaults->size()) {
		return value;
	}

	return defaults->at(pos);
}

result_t Message::create(vector<string>::iterator& it, const vector<string>::iterator end,
		vector< vector<string> >* defaultsRows, Condition* condition, const string& filename,
		DataFieldTemplates* templates, vector<Message*>& messages)
{
	// [type],[circuit],name,[comment],[QQ[;QQ]*],[ZZ],[PBSB],[ID],fields...
	result_t result;
	bool isWrite = false, isPassive = false;
	string defaultName;
	unsigned char pollPriority = 0;
	size_t defaultPos = 1;
	if (it == end)
		return RESULT_ERR_EOF;

	string typeStr = *it++;
	const char* str = typeStr.c_str(); // [type]
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

	string circuit = getDefault(*it++, defaults, defaultPos++); // [circuit]
	if (it == end)
		return RESULT_ERR_EOF;

	string name = *it++; // name
	if (it == end)
		return RESULT_ERR_EOF;
	if (name.length() == 0)
		return RESULT_ERR_INVALID_ARG; // empty name
	defaultPos++;

	string comment = getDefault(*it++, defaults, defaultPos++); // [comment]
	if (it == end)
		return RESULT_ERR_EOF;

	str = getDefault(*it++, defaults, defaultPos++).c_str(); // [QQ[;QQ]*]
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

	str = getDefault(*it++, defaults, defaultPos++).c_str(); // [ZZ]
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
			DataFieldTemplates::trim(token);
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
	for (int pos = 0; pos < 2 && it != end; pos++) { // [PBSB],[ID] (optional master data)
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
				return RESULT_ERR_INVALID_ARG; // to short hex
			}
			token.push_back((char)input.get());

			unsigned char value = (unsigned char)parseInt(token.c_str(), 16, 0, 0xff, result);
			if (result != RESULT_OK) {
				return result; // invalid hex value
			}
			id.push_back(value);
		}
		if (pos == 0 && id.size() != 2)
			return RESULT_ERR_INVALID_ARG; // missing/to short/to long PBSB

		defaultPos++;
	}
	if (id.size() < 2 || id.size() > 6) {
		return RESULT_ERR_INVALID_ARG; // missing/to short/to long ID
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
		messages.push_back(new Message(useCircuit, name, isWrite, isPassive, comment, srcAddress, dstAddress, id, data, index==0, pollPriority, condition));
	}
	return RESULT_OK;
}

Message* Message::derive(const unsigned char dstAddress)
{
	return new Message(m_circuit, m_name, m_isWrite,
		m_isPassive, m_comment,
		m_srcAddress, dstAddress,
		m_id, m_data, false,
		m_pollPriority, m_condition);
}

unsigned long long Message::getDerivedKey(const unsigned char dstAddress)
{
	return (m_key & ~(0xffLL << (8*6))) | (unsigned long long)dstAddress << (8*6);
}

bool Message::setPollPriority(unsigned char priority)
{
	if (priority == m_pollPriority || m_isPassive)
		return false;

	if (m_usedByCondition && (priority==0 || priority>POLL_PRIORITY_CONDITION))
		priority = POLL_PRIORITY_CONDITION;

	bool ret = m_pollPriority==0 && priority>0;
	m_pollPriority = priority;
	return ret;
}

void Message::setUsedByCondition() {
	if (m_usedByCondition)
		return;
	m_usedByCondition = true;
	if (m_pollPriority==0 || m_pollPriority>POLL_PRIORITY_CONDITION)
		setPollPriority(POLL_PRIORITY_CONDITION);
}

bool Message::isAvailable()
{
	return (m_condition==NULL) || m_condition->isTrue();
}

bool Message::hasField(const char* fieldName, bool numeric)
{
	return m_data->hasField(fieldName, numeric);
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
		m_lastMasterData = master;
		break;
	case 2: // only master address is different
		m_lastMasterData = master;
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

result_t Message::decodeLastDataField(unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	unsigned char offset = (unsigned char)(m_id.size() - 2);
	result_t result = m_data->read(pt_masterData, m_lastMasterData, offset, output, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY)
		result = m_data->read(pt_slaveData, m_lastSlaveData, 0, output, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY)
		return RESULT_ERR_NOTFOUND;
	return result;
}

bool Message::isLessPollWeight(const Message* other)
{
	unsigned char tprio = m_pollPriority;
	unsigned char oprio = other->m_pollPriority;
	unsigned int tw = tprio * m_pollCount;
	unsigned int ow = oprio * other->m_pollCount;
	if (tw > ow)
		return true;
	if (tw < ow)
		return false;
	if (tprio > oprio)
		return true;
	if (tprio < oprio)
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

void Message::dump(ostream& output, vector<size_t>& columns)
{
	bool first = true;
	unsigned int cnt = 0;
	for (vector<size_t>::const_iterator it = columns.begin(); it < columns.end(); it++) {
		if (first) {
			first = false;
		} else {
			output << FIELD_SEPARATOR;
		}
		size_t column = *it;
		switch (column) {
		case 0: // type
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
			break;
		case 1: // circuit
			DataField::dumpString(output, m_circuit, false);
			break;
		case 2: // name
			DataField::dumpString(output, m_name, false);
			break;
		case 3: // comment
			DataField::dumpString(output, m_comment, false);
			break;
		case 4: // QQ
			if (m_srcAddress != SYN)
				output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_srcAddress);
			break;
		case 5: // ZZ
			if (m_dstAddress != SYN)
				output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_dstAddress);
			break;
		case 6: // PBSB
		case 7: // ID
			for (vector<unsigned char>::const_iterator it = m_id.begin(); it < m_id.end(); it++) {
				cnt++;
				if (column == 6) {
					if (cnt == 2)
						break;
				} else if (cnt < 2) {
					continue;
				}
				output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
			}
			break;
		case 8: // fields
			m_data->dump(output);
			break;
		}
	}
}

string strtolower(const string& str)
{
	string ret(str);
	transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
	return ret;
}

Message* getFirstAvailable(vector<Message*> &messages) {
	for (vector<Message*>::iterator msgIt = messages.begin(); msgIt != messages.end(); msgIt++)
		if ((*msgIt)->isAvailable())
			return *msgIt;
	return NULL;
}


result_t Condition::create(vector<string>::iterator& it, const vector<string>::iterator end, SimpleCondition*& returnValue)
{
	// name,circuit,messagename,[comment],[fieldname],[ZZ],values   (name already skipped by caller)
	if (it+2>end) { // at least everything including messagename
		it = end; // for error reporting
		return RESULT_ERR_EOF;
	}
	string circuit = *(it++); // circuit
	if (circuit.length()==0)
		return RESULT_ERR_INVALID_ARG;
	string name = it==end ? "" : *(it++); // messagename
	if (name.length()==0)
		return RESULT_ERR_INVALID_ARG;
	it++; // comment
	string field = it==end ? "" : *(it++); // fieldname
	string zz = it==end ? "" : *(it++); // ZZ
	unsigned char dstAddress = SYN;
	result_t result = RESULT_OK;
	if (zz.length()>0) {
		dstAddress = (unsigned char)parseInt(zz.c_str(), 16, 0, 0xff, result);
		if (result != RESULT_OK)
			return result;
		if (dstAddress!=SYN && !isValidAddress(dstAddress, false))
			return RESULT_ERR_INVALID_ADDR;
	}
	istringstream stream(it==end ? "" : *(it++));
	string str;
	vector<unsigned int> valueRanges;
	while (getline(stream, str, VALUE_SEPARATOR) != 0) {
		DataFieldTemplates::trim(str);
		if (str.length()==0)
			return RESULT_ERR_INVALID_ARG;
		bool upto = str[0]=='<';
		if (upto || str[0]=='>') {
			if (str.length()==1)
				return RESULT_ERR_INVALID_ARG;
			if (upto)
				valueRanges.push_back(0);
			bool inclusive = str[1]=='=';
			unsigned int val = parseInt(str.substr(inclusive?2:1).c_str(), 10, inclusive?0:1, inclusive?UINT_MAX:(UINT_MAX-1), result);
			if (result!=RESULT_OK)
				return result;
			valueRanges.push_back(inclusive ? val : (val+(upto?-1:1)));
			if (!upto)
				valueRanges.push_back(UINT_MAX);
		} else {
			size_t pos = str.find('-');
			if (pos>0) { // range
				unsigned int val = parseInt(str.substr(0, pos).c_str(), 10, 0, UINT_MAX, result);
				if (result!=RESULT_OK)
					return result;
				valueRanges.push_back(val);
				pos++;
			} else { // single value
				pos = 0;
			}
			unsigned int val = parseInt(str.substr(pos).c_str(), 10, 0, UINT_MAX, result);
			if (result!=RESULT_OK)
				return result;
			valueRanges.push_back(val);
			if (pos==0)
				valueRanges.push_back(val); // single value
		}
	}

	returnValue = new SimpleCondition(circuit, name, dstAddress, field, valueRanges);
	return RESULT_OK;
}


CombinedCondition* SimpleCondition::combineAnd(Condition* other)
{
	CombinedCondition* ret = new CombinedCondition();
	return ret->combineAnd(this)->combineAnd(other);
}

result_t SimpleCondition::resolve(MessageMap* messages, ostringstream& errorMessage)
{
	if (m_message!=NULL)
		return RESULT_OK; // already resolved
	Message* message = messages->find(m_circuit, m_name, false);
	if (!message)
		message = messages->find(m_circuit, m_name, false, true);
	if (!message) {
		errorMessage << "condition " << m_circuit << " " << m_name << ": message not found";
		return RESULT_ERR_NOTFOUND;
	}
	if (message->getDstAddress()==SYN) {
		if (message->isPassive()) {
			errorMessage << "condition " << m_circuit << " " << m_name << ": invalid passive message";
			return RESULT_ERR_INVALID_ARG;
		}
		if (m_dstAddress==SYN) {
			errorMessage << "condition " << m_circuit << " " << m_name << ": destination address missing";
			return RESULT_ERR_INVALID_ADDR;
		}
		// clone the message with dedicated dstAddress if necessary
		unsigned long long key = message->getDerivedKey(m_dstAddress);
		vector<Message*>* derived = messages->getByKey(key);
		if (derived==NULL) {
			message = message->derive(m_dstAddress);
			messages->add(message);
		} else {
			message = getFirstAvailable(*derived);
			if (message==NULL) {
				errorMessage << "condition " << m_circuit << " " << m_name << ": conditional derived message";
				return RESULT_ERR_INVALID_ARG;
			}
		}
	}

	if (!m_valueRanges.empty()) {
		if (!message->hasField(m_field.length()>0 ? m_field.c_str() : NULL, true)) {
			errorMessage << "condition " << m_circuit << " " << m_name << ": numeric field " << m_field << " not found";
			return RESULT_ERR_NOTFOUND;
		}
	}
	m_message = message;
	message->setUsedByCondition();
	messages->addPollMessage(message, true);
	return RESULT_OK;
}

bool SimpleCondition::isTrue()
{
	if (!m_message)
		return false;
	if (m_message->getLastChangeTime()>m_lastCheckTime) {
		bool isTrue = m_valueRanges.empty(); // for message seen check
		if (!isTrue) {
			unsigned int value = 0;
			result_t result = m_message->decodeLastDataField(value, m_field.length()==0 ? NULL : m_field.c_str());
			if (result==RESULT_OK) {
				for (size_t i=0; i+1<m_valueRanges.size(); i+=2) {
					if (m_valueRanges[i]<=value && value<=m_valueRanges[i+1]) {
						isTrue = true;
						break;
					}
				}
			}
		}
		m_isTrue = isTrue;
		m_lastCheckTime = m_message->getLastChangeTime();
	}
	return m_isTrue;
}


result_t CombinedCondition::resolve(MessageMap* messages, ostringstream& errorMessage)
{
	ostringstream dummy;
	for (vector<Condition*>::iterator it = m_conditions.begin(); it!=m_conditions.end(); it++) {
		Condition* condition = *it;
		result_t ret = condition->resolve(messages, dummy);
		if (ret!=RESULT_OK)
			return ret;
	}
	return RESULT_OK;
}

bool CombinedCondition::isTrue()
{
	for (vector<Condition*>::iterator it = m_conditions.begin(); it!=m_conditions.end(); it++) {
		if (!(*it)->isTrue())
			return false;
	}
	return true;
}


result_t MessageMap::add(Message* message)
{
	unsigned long long key = message->getKey();
	bool conditional = message->isConditional();
	if (!m_addAll) {
		map<unsigned long long, vector<Message*> >::iterator keyIt = m_messagesByKey.find(key);
		if (keyIt != m_messagesByKey.end()) {
			if (!conditional)
				return RESULT_ERR_DUPLICATE; // duplicate key
			vector<Message*>* messages = &keyIt->second;
			if (!messages->front()->isConditional())
				return RESULT_ERR_DUPLICATE; // duplicate key
		}
	}
	bool isPassive = message->isPassive();
	bool isWrite = message->isWrite();
	string circuit = strtolower(message->getCircuit());
	string name = strtolower(message->getName());
	string nameKey = string(isPassive ? "P" : (isWrite ? "W" : "R")) + circuit + FIELD_SEPARATOR + name;
	if (!m_addAll) {
		map<string, vector<Message*> >::iterator nameIt = m_messagesByName.find(nameKey);
		if (nameIt != m_messagesByName.end()) {
			vector<Message*>* messages = &nameIt->second;
			if (!message->isConditional() || !messages->front()->isConditional())
				return RESULT_ERR_DUPLICATE_NAME; // duplicate key
		}
	}
	m_messagesByName[nameKey].push_back(message);
	m_messageCount++;
	if (conditional)
		m_conditionalMessageCount++;
	if (isPassive)
		m_passiveMessageCount++;

	nameKey = string(isPassive ? "-P" : (isWrite ? "-W" : "-R")) + name; // also store without circuit
	map<string, vector<Message*> >::iterator nameIt = m_messagesByName.find(nameKey);
	if (nameIt == m_messagesByName.end())
		m_messagesByName[nameKey].push_back(message); // always store first message without circuit (in order of circuit name)
	else {
		vector<Message*>* messages = &nameIt->second;
		Message* first = messages->front();
		if (circuit < first->getCircuit())
			m_messagesByName[nameKey].at(0) = message; // always store first message without circuit (in order of circuit name)
		else if (m_addAll || (conditional && first->isConditional()))
			m_messagesByName[nameKey].push_back(message); // store further messages only if both are conditional or if storing everything
	}
	unsigned char idLength = (unsigned char)(message->getId().size() - 2);
	if (idLength < m_minIdLength)
		m_minIdLength = idLength;
	if (idLength > m_maxIdLength)
		m_maxIdLength = idLength;
	m_messagesByKey[key].push_back(message);

	addPollMessage(message);

	return RESULT_OK;
}

result_t MessageMap::addDefaultFromFile(vector< vector<string> >& defaults, vector<string>& row,
	vector<string>::iterator& begin, string defaultDest, string defaultCircuit,
	const string& filename, unsigned int lineNo)
{
	// convert conditions in defaults
	string type = row[0];
	if (type.length()>0 && type[0]=='[' && type[type.length()-1]==']') {
		// condition
		type = type.substr(1, type.length()-2);
		string key = filename+":"+type;
		map<string, Condition*>::iterator it = m_conditions.find(key);
		if (it != m_conditions.end()) {
			m_lastError = "condition "+type+" already defined";
			return RESULT_ERR_DUPLICATE_NAME;
		}
		if (row.size()>1 && defaultCircuit.length()>0 && row[1].length()==0)
			row[1] = defaultCircuit; // set default circuit
		SimpleCondition* condition = NULL;
		result_t result = Condition::create(++begin, row.end(), condition);
		if (result!=RESULT_OK) {
			if (condition)
				delete condition;
			m_lastError = "invalid condition";
			return result;
		}
		if (!condition)
			return RESULT_ERR_INVALID_ARG;
		m_conditions[key] = condition;
		return RESULT_OK;
	}
	return FileReader<DataFieldTemplates*>::addDefaultFromFile(defaults, row, begin, defaultDest, defaultCircuit, filename, lineNo);
}

result_t MessageMap::readConditions(string& types, const string& filename, Condition*& condition)
{
	size_t pos;
	if (types.length()>0 && types[0]=='[' && (pos=types.find_last_of(']'))!=string::npos) {
		// check if combined or simple condition is already known
		const string combinedkey = filename+":"+types.substr(1, pos-1);
		map<string, Condition*>::iterator it = m_conditions.find(combinedkey);
		if (it!=m_conditions.end()) {
			condition = it->second;
			types = types.substr(pos+1);
		} else {
			bool store = false;
			while ((pos=types.find(']'))!=string::npos) {
				// simple condition
				string key = filename+":"+types.substr(1, pos-1);
				map<string, Condition*>::iterator it = m_conditions.find(key);
				if (it==m_conditions.end()) {
					m_lastError = "condition "+types.substr(1, pos-1)+" not defined";
					return RESULT_ERR_NOTFOUND;
				}
				if (condition) {
					condition = condition->combineAnd(it->second);
					store = true;
				} else
					condition = it->second;
				types = types.substr(pos+1);
				if (types.length()==0 || types[0]!='[')
					break;
			}
			if (store) {
				m_conditions[combinedkey] = condition; // store combined condition
			}
		}
	}
	return RESULT_OK;
}

result_t MessageMap::addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
	DataFieldTemplates* arg, vector< vector<string> >* defaults,
	const string& filename, unsigned int lineNo)
{
	vector<string>::iterator restart = begin;
	string types = *restart;
	Condition* condition = NULL;
	result_t result = readConditions(types, filename, condition);
	if (result!=RESULT_OK)
		return result;

	if (types.length() == 0)
		types.append("r");
	else if (types.find(']')!=string::npos)
		return RESULT_ERR_INVALID_ARG;

	result = RESULT_ERR_EOF;
	istringstream stream(types);
	string type;
	vector<Message*> messages;
	while (getline(stream, type, VALUE_SEPARATOR) != 0) {
		DataFieldTemplates::trim(type);
		*restart = type;
		begin = restart;
		messages.clear();
		result = Message::create(begin, end, defaults, condition, filename, arg, messages);
		for (vector<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
			Message* message = *it;
			if (result == RESULT_OK) {
				result = add(message);
				if (result==RESULT_ERR_DUPLICATE_NAME)
					begin = restart+3; // mark name as invalid
				else if (result==RESULT_ERR_DUPLICATE)
					begin = restart+8; // mark ID as invalid
			}
			if (result != RESULT_OK)
				delete message; // delete all remaining messages on error
		}
		if (result != RESULT_OK)
			return result;
	}
	return result;
}

result_t MessageMap::resolveConditions(bool verbose) {
	result_t overallResult = RESULT_OK;
	for (map<string, Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
		Condition* condition = it->second;
		ostringstream error;
		result_t result = condition->resolve(this, error);
		if (result!=RESULT_OK) {
			string errorMessage = error.str();
			if (errorMessage.length()>0) {
				if (m_lastError.length()>0)
					m_lastError += ", ";
				m_lastError += errorMessage;
			}
			if (verbose)
				overallResult = result;
			else {
				return result;
			}
		}
	}
	return overallResult;
}

vector<Message*>* MessageMap::getByKey(const unsigned long long key) {
	map<unsigned long long, vector<Message*> >::iterator it = m_messagesByKey.find(key);
	if (it != m_messagesByKey.end())
		return &it->second;
	return NULL;
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
		map<string, vector<Message*> >::iterator it = m_messagesByName.find(key);
		if (it != m_messagesByName.end()) {
			Message* message = getFirstAvailable(it->second);
			if (message)
				return message;
		}
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
	for (map<string, vector<Message*> >::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // avoid duplicates: instances stored multiple times have a key starting with "-"
			continue;
		Message* message = getFirstAvailable(it->second);
		if (!message)
			continue;
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

		map<unsigned long long , vector<Message*> >::iterator it = m_messagesByKey.find(key);
		if (it != m_messagesByKey.end()) {
			Message* message = getFirstAvailable(it->second);
			if (message)
				ret.push_back(message);
		}
		if ((key & ID_SOURCE_MASK) != 0) {
			it = m_messagesByKey.find(key & ~ID_SOURCE_MASK); // try again without specific source master
			if (it != m_messagesByKey.end()) {
				Message* message = getFirstAvailable(it->second);
				if (message)
					ret.push_back(message);
			}
		}
		it = m_messagesByKey.find(key | ID_SOURCE_MASK); // try again with special value for active
		if (it != m_messagesByKey.end()) {
			Message* message = getFirstAvailable(it->second);
			if (message)
				ret.push_back(message);
		}
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

void MessageMap::addPollMessage(Message* message, bool toFront)
{
	if (message != NULL && message->getPollPriority() > 0) {
		message->m_lastPollTime = toFront ? 0 : m_pollMessages.size();
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
	for (map<string, vector<Message*> >::iterator mit = m_messagesByName.begin(); mit != m_messagesByName.end(); mit++) {
		if (mit->first[0] != '-') { // avoid double free: instances stored multiple times have a key starting with "-"
			vector<Message*> messages = mit->second;
			for (vector<Message*>::iterator it = messages.begin(); it != messages.end(); it++)
				delete *it;
			messages.clear();
		}
	}
	// free condition instances
	for (map<string, Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
		delete it->second;
	}
	// clear messages by name
	m_messageCount = 0;
	m_conditionalMessageCount = 0;
	m_passiveMessageCount = 0;
	m_messagesByName.clear();
	// clear messages by key
	m_messagesByKey.clear();
	m_conditions.clear();
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
	for (map<string, vector<Message*> >::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // skip instances stored multiple times (key starting with "-")
			continue;
		Message* message = getFirstAvailable(it->second);
		if (!message)
			continue;
		if (first)
			first = false;
		else
			output << endl;
		message->dump(output);
	}
	if (!first)
		output << endl;
}

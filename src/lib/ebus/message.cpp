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

#include "message.h"
#include "data.h"
#include "result.h"
#include "symbol.h"
#include <string>
#include <vector>
#include <cstring>
#include <locale>
#include <iomanip>
#include <climits>

using namespace std;

/** the maximum length of the command ID bytes (in addition to PB/SB) for which the key is distinct. */
#define MAX_ID_KEYLEN 4

/** the bit mask of the source master number in the message key. */
#define ID_SOURCE_MASK (0x1fLL << (8 * 7))

/** the bits in the @a ID_SOURCE_MASK for arbitrary source and active read message. */
#define ID_SOURCE_ACTIVE_WRITE (0x1fLL << (8 * 7))

/** the bits in the @a ID_SOURCE_MASK for arbitrary source and active write message. */
#define ID_SOURCE_ACTIVE_READ (0x1eLL << (8 * 7))

/** the maximum poll priority for a @a Message referred to by a @a Condition. */
#define POLL_PRIORITY_CONDITION 5

extern DataFieldTemplates* getTemplates(const string filename);

Message::Message(const string circuit, const string name,
		const bool isWrite, const bool isPassive, const string comment,
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
		  m_usedByCondition(false), m_isScanMessage(false), m_condition(condition),
		  m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0)
{
	unsigned long long key = (unsigned long long)(id.size()-2) << (8 * 7 + 5);
	if (isPassive)
		key |= (unsigned long long)getMasterNumber(srcAddress) << (8 * 7); // 0..25
	else
		key |= (isWrite ? 0x1fLL : 0x1eLL) << (8 * 7); // special values for active
	key |= (unsigned long long)dstAddress << (8 * 6);
	int exp = 5;
	for (vector<unsigned char>::const_iterator it = id.begin(); it < id.end(); it++) {
		key ^= (unsigned long long)*it << (8 * exp--);
		if (exp == 0)
			exp = 3;
	}
	m_key = key;
	if (circuit=="scan") {
		setScanMessage();
		m_pollPriority = 0;
	}
}

Message::Message(const string circuit, const string name,
		const bool isWrite, const bool isPassive,
		const unsigned char pb, const unsigned char sb,
		DataField* data, const bool deleteData)
		: m_circuit(circuit), m_name(name), m_isWrite(isWrite),
		  m_isPassive(isPassive), m_comment(),
		  m_srcAddress(SYN), m_dstAddress(SYN),
		  m_data(data), m_deleteData(true),
		  m_pollPriority(0),
		  m_usedByCondition(false), m_isScanMessage(false), m_condition(NULL),
		  m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0)
{
	m_id.push_back(pb);
	m_id.push_back(sb);
	unsigned long long key = 0;
	if (!isPassive)
		key |= (isWrite ? 0x1fLL : 0x1eLL) << (8 * 7); // special values for active
	key |= (unsigned long long)SYN << (8 * 6);
	key |= (unsigned long long)pb << (8 * 5);
	key |= (unsigned long long)sb << (8 * 4);
	m_key = key;
	setScanMessage();
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

result_t Message::parseId(string input, vector<unsigned char>& id)
{
	istringstream in(input);
	while (!in.eof()) {
		while (in.peek() == ' ')
			in.get();
		if (in.eof()) // no more digits
			break;
		input.clear();
		input.push_back((char)in.get());
		if (in.eof()) {
			return RESULT_ERR_INVALID_ARG; // too short hex
		}
		input.push_back((char)in.get());

		result_t result;
		unsigned char value = (unsigned char)parseInt(input.c_str(), 16, 0, 0xff, result);
		if (result != RESULT_OK) {
			return result; // invalid hex value
		}
		id.push_back(value);
	}
	return RESULT_OK;
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
	string defStr = getDefault("", defaults, defaultPos++);
	if (defStr.length() > 0) {
		name = defStr+name; // prefix default name
	}

	string comment = *it++; // [comment]
	if (it == end)
		return RESULT_ERR_EOF;
	defStr = getDefault("", defaults, defaultPos++);
	if (defStr.length() > 0) {
		name = defStr+name; // prefix default comment
	}

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
		while (getline(stream, token, VALUE_SEPARATOR)) {
			FileReader::trim(token);
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
	string token = *it++; // [PBSB]
	bool useDefaults = token.empty();
	if (useDefaults)
		token = getDefault(token, defaults, defaultPos);
	defaultPos++;
	result = parseId(token, id);
	if (result!=RESULT_OK)
		return result;
	if (id.size() != 2)
		return RESULT_ERR_INVALID_ARG; // missing/to short/to long PBSB

	if (it == end)
		token = "";
	else
		token = *it++;// [ID] (optional master data)
	string defaultIdPrefix;
	if (useDefaults)
		defaultIdPrefix = getDefault("", defaults, defaultPos);
	defaultPos++;
	vector< vector<unsigned char> > chainIds;
	vector<unsigned char> chainLengths;
	istringstream stream(token);
	size_t maxLength = MAX_POS;
	size_t chainLength = 16;
	size_t chainPrefixLength = id.size();
	bool first = true, lastChainLengthSpecified = false;
	while (getline(stream, token, VALUE_SEPARATOR) || first) {
		FileReader::trim(token);
		token = defaultIdPrefix+token;
		size_t lengthPos = token.find(LENGTH_SEPARATOR);
		lastChainLengthSpecified = lengthPos!=string::npos;
		if (lastChainLengthSpecified) {
			chainLength = parseInt(token.substr(lengthPos+1).c_str(), 10, 0, MAX_POS, result);
			if (result != RESULT_OK)
				return result;
			token.resize(lengthPos);
		}
		vector<unsigned char> chainId = id;
		result = parseId(token, chainId);
		if (result!=RESULT_OK)
			return result;
		if (!chainIds.empty() && chainId.size()!=chainIds.front().size())
			return RESULT_ERR_INVALID_LIST;
		chainIds.push_back(chainId);
		chainLengths.push_back((unsigned char)chainLength);
		if (first) {
			chainPrefixLength = chainId.size();
			maxLength = 0;
		} else if (chainPrefixLength>2) {
			vector<unsigned char>& front = chainIds.front();
			for (size_t pos=2; pos<chainPrefixLength; pos++) {
				if (chainId[pos]!=front[pos]) {
					chainPrefixLength = pos;
					break;
				}
			}
		}
		if (maxLength+chainLength>255)
			return RESULT_ERR_INVALID_POS;
		maxLength += chainLength;
		first = false;
	}
	id = chainIds.front();
	if (chainIds.size()>1) {
		if (isPassive)
			return RESULT_ERR_INVALID_LIST;
		if (id.size()>chainPrefixLength)
			id.resize(chainPrefixLength);
		if (!lastChainLengthSpecified && chainLength<MAX_POS)
			maxLength += MAX_POS-chainLength;
	} else if (!lastChainLengthSpecified) {
		maxLength = MAX_POS;
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
		result = DataField::create(it, realEnd, templates, data, isWrite, false, isBroadcastOrMasterDestination, (unsigned char)maxLength);
		if (result != RESULT_OK) {
			return result;
		}
	}
	if (id.size() + data->getLength(pt_masterData, (unsigned char)maxLength) > 2 + maxLength || data->getLength(pt_slaveData, (unsigned char)maxLength) > maxLength) {
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
		Message* message;
		if (chainIds.size()>1) {
			message = new ChainedMessage(useCircuit, name, isWrite, comment, srcAddress, dstAddress, id, chainIds, chainLengths, data, index==0, pollPriority, condition);
		} else {
			message = new Message(useCircuit, name, isWrite, isPassive, comment, srcAddress, dstAddress, id, data, index==0, pollPriority, condition);
		}
		messages.push_back(message);
	}
	return RESULT_OK;
}

Message* Message::createScanMessage()
{
	return new Message("scan", "", false, false, 0x07, 0x04, DataFieldSet::getIdentFields(), true);
}

Message* Message::derive(const unsigned char dstAddress, const unsigned char srcAddress, const string circuit)
{
	Message* result = new Message(circuit.length()==0 ? m_circuit : circuit, m_name,
		m_isWrite, m_isPassive, m_comment,
		srcAddress==SYN ? m_srcAddress : srcAddress, dstAddress,
		m_id, m_data, false,
		m_pollPriority, m_condition);
	if (m_isScanMessage) {
		result->setScanMessage();
	}
	return result;
}

Message* Message::derive(const unsigned char dstAddress, const bool extendCircuit)
{
	if (extendCircuit) {
		ostringstream out;
		out << m_circuit << '.' << hex << setw(2) << setfill('0') << static_cast<unsigned>(dstAddress);
		return derive(dstAddress, SYN, out.str());
	}
	return derive(dstAddress, SYN, m_circuit);
}

bool Message::checkIdPrefix(vector<unsigned char>& id)
{
	if (id.size() > m_id.size()) {
		return false;
	}
	for (size_t pos = 0; pos < id.size(); pos++) {
		if (id[pos] != m_id[pos]) {
			return false;
		}
	}
	return true;
}

bool Message::checkId(SymbolString& master, unsigned char* index)
{
	unsigned char idLen = getIdLength();
	if (master.size() < 5+idLen) { // QQ, ZZ, PB, SB, NN
		return false;
	}
	for (unsigned char pos = 0; pos < idLen; pos++) {
		if (m_id[2+pos] != master[5+pos])
			return false;
	}
	if (index) {
		*index = 0;
	}
	return true;
}

bool Message::checkId(Message& other)
{
	unsigned char idLen = getIdLength();
	if (idLen != other.getIdLength() || getCount() > 1) { // only equal for non-chained messages
		return false;
	}
	return other.checkIdPrefix(m_id);
}

unsigned long long Message::getDerivedKey(const unsigned char dstAddress)
{
	return (m_key & ~(0xffLL << (8*6))) | (unsigned long long)dstAddress << (8*6);
}

bool Message::setPollPriority(unsigned char priority)
{
	if (priority == m_pollPriority || m_isPassive || isScanMessage() || m_dstAddress==SYN)
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

result_t Message::prepareMaster(const unsigned char srcAddress, SymbolString& masterData,
		istringstream& input, char separator,
		const unsigned char dstAddress, unsigned char index)
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
	result = prepareMasterPart(master, input, separator, index);
	if (result != RESULT_OK)
		return result;
	masterData.clear();
	masterData.addAll(master);
	result = storeLastData(pt_masterData, masterData, index);
	if (result < RESULT_OK)
		return result;
	return RESULT_OK;
}

result_t Message::prepareMasterPart(SymbolString& master, istringstream& input, char separator, unsigned char index)
{
	if (index!=0)
		return RESULT_ERR_NOTFOUND;
	unsigned char pos = master.size();
	result_t result = master.push_back(0, false, false); // length, will be set later
	if (result != RESULT_OK)
		return result;
	for (size_t i = 2; i < m_id.size(); i++) {
		result = master.push_back(m_id[i], false, false);
		if (result != RESULT_OK)
			return result;
	}
	result = m_data->write(input, pt_masterData, master, getIdLength(), separator);
	if (result != RESULT_OK)
		return result;
	master[pos] = (unsigned char)(master.size()-pos-1);
	return result;
}

result_t Message::prepareSlave(istringstream& input, SymbolString& slaveData)
{
	if (m_isWrite)
		return RESULT_ERR_INVALID_ARG; // prepare not possible

	SymbolString slave(false);
	result_t result = slave.push_back(0, false, false); // length, will be set later
	if (result != RESULT_OK)
		return result;
	result = m_data->write(input, pt_slaveData, slave, 0);
	if (result != RESULT_OK)
		return result;
	slave[0] = (unsigned char)(slave.size()-1);
	time(&m_lastUpdateTime);
	if (slave != m_lastSlaveData) {
		m_lastChangeTime = m_lastUpdateTime;
		m_lastSlaveData = slave;
	}
	slaveData.clear();
	slaveData.addAll(slave);
	return result;
}

result_t Message::storeLastData(SymbolString& master, SymbolString& slave)
{
	result_t result = storeLastData(pt_masterData, master, 0);
	if (result>=RESULT_OK)
		result = storeLastData(pt_slaveData, slave, 0);
	return result;
}

result_t Message::storeLastData(const PartType partType, SymbolString& data, unsigned char index)
{
	if (data.size() > 0 && (this->m_dstAddress == BROADCAST || partType == pt_slaveData)) {
		time(&m_lastUpdateTime);
	}
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
	return RESULT_OK;
}

result_t Message::decodeLastData(const PartType partType,
		ostringstream& output, OutputFormat outputFormat,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	unsigned char offset;
	if (partType == pt_masterData)
		offset = (unsigned char)(m_id.size() - 2);
	else
		offset = 0;
	result_t result = m_data->read(partType, partType==pt_masterData ? m_lastMasterData : m_lastSlaveData, offset, output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY && fieldName != NULL)
		return RESULT_ERR_NOTFOUND;
	return result;
}

result_t Message::decodeLastData(ostringstream& output, OutputFormat outputFormat,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	size_t startPos = output.str().length();
	result_t result = m_data->read(pt_masterData, m_lastMasterData, getIdLength(), output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	bool empty = result == RESULT_EMPTY;
	leadingSeparator |= output.str().length() > startPos;
	result = m_data->read(pt_slaveData, m_lastSlaveData, 0, output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
	if (result < RESULT_OK)
		return result;
	if (result == RESULT_EMPTY && !empty)
		result = RESULT_OK; // OK if at least one part was non-empty
	else if (result == RESULT_EMPTY && fieldName != NULL)
		return RESULT_ERR_NOTFOUND;
	return result;
}

result_t Message::decodeLastDataNumField(unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	result_t result = m_data->read(pt_masterData, m_lastMasterData, getIdLength(), output, fieldName, fieldIndex);
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

void Message::dump(ostream& output, vector<size_t>* columns, bool withConditions)
{
	bool first = true, all = columns==NULL;
	size_t end = all ? 9 : columns->size();
	for (size_t i=0; i<end; i++) {
		if (first) {
			first = false;
		} else {
			output << FIELD_SEPARATOR;
		}
		size_t column = all ? i : (*columns)[i];
		dumpColumn(output, column, withConditions);
	}
}

void Message::dumpColumn(ostream& output, size_t column, bool withConditions)
{
	switch (column) {
	case 0: // type
		if (withConditions && m_condition!=NULL) {
			m_condition->dump(output);
		}
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
		for (vector<unsigned char>::const_iterator it = m_id.begin(); it < m_id.begin()+2 && it < m_id.end(); it++) {
			output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
		}
		break;
	case 7: // ID
		for (vector<unsigned char>::const_iterator it = m_id.begin()+2; it < m_id.end(); it++) {
			output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
		}
		break;
	case 8: // fields
		m_data->dump(output);
		break;
	}
}


ChainedMessage::ChainedMessage(const string circuit, const string name,
		const bool isWrite, const string comment,
		const unsigned char srcAddress, const unsigned char dstAddress,
		const vector<unsigned char> id,
		vector< vector<unsigned char> > ids, vector<unsigned char> lengths,
		DataField* data, const bool deleteData,
		const unsigned char pollPriority,
		Condition* condition)
		: Message(circuit, name, isWrite, false, comment,
		  srcAddress, dstAddress, id,
		  data, deleteData, pollPriority, condition),
		  m_ids(ids), m_lengths(lengths),
		  m_maxTimeDiff(m_ids.size()*15) // 15 seconds per message
{
	size_t cnt = ids.size();
	m_lastMasterDatas = (SymbolString**)calloc(cnt, sizeof(SymbolString*));
	m_lastSlaveDatas = (SymbolString**)calloc(cnt, sizeof(SymbolString*));
	m_lastMasterUpdateTimes = (time_t*)calloc(cnt, sizeof(time_t));
	m_lastSlaveUpdateTimes = (time_t*)calloc(cnt, sizeof(time_t));
	for (size_t index=0; index<cnt; index++) {
		m_lastMasterDatas[index] = new SymbolString();
		m_lastSlaveDatas[index] = new SymbolString();
	}
}

ChainedMessage::~ChainedMessage()
{
	for (unsigned char index=0; index<m_ids.size(); index++) {
		delete m_lastMasterDatas[index];
		m_lastMasterDatas[index] = NULL;
		delete m_lastSlaveDatas[index];
		m_lastSlaveDatas[index] = NULL;
	}
	free(m_lastMasterDatas);
	free(m_lastSlaveDatas);
	free(m_lastMasterUpdateTimes);
	free(m_lastSlaveUpdateTimes);
}

Message* ChainedMessage::derive(const unsigned char dstAddress, const unsigned char srcAddress, const string circuit)
{
	ChainedMessage* result = new ChainedMessage(circuit.length()==0 ? m_circuit : circuit, m_name,
		m_isWrite, m_comment,
		srcAddress==SYN ? m_srcAddress : srcAddress, dstAddress,
		m_id, m_ids, m_lengths, m_data, false,
		m_pollPriority, m_condition);
	if (m_isScanMessage) {
		result->setScanMessage();
	}
	return result;
}

bool ChainedMessage::checkId(SymbolString& master, unsigned char* index)
{
	unsigned char idLen = getIdLength();
	if (master.size() < 5+idLen) { // QQ, ZZ, PB, SB, NN
		return false;
	}
	unsigned char chainPrefixLength = Message::getIdLength();
	for (unsigned char pos = 0; pos < chainPrefixLength; pos++) {
		if (m_id[2+pos] != master[5+pos]) {
			return false; // chain prefix mismatch
		}
	}
	for (unsigned char checkIndex=0; checkIndex<m_ids.size(); checkIndex++) { // check suffix for each part
		vector<unsigned char> id = m_ids[checkIndex];
		bool found = false;
		for (unsigned char pos=chainPrefixLength; pos<idLen; pos++) {
			if (id[2+pos] != master[5+pos]) {
				found = false;
				break;
			}
			found = true;
		}
		if (found) {
			if (index) {
				*index = checkIndex;
			}
			return true;
		}
	}
	return false;
}

bool ChainedMessage::checkId(Message& other)
{
	unsigned char idLen = getIdLength();
	if (idLen != other.getIdLength() || other.getCount() == 1) { // only equal for chained messages
		return false;
	}
	if (!other.checkIdPrefix(m_id)) {
		return false; // chain prefix mismatch
	}
	vector< vector<unsigned char> > otherIds = ((ChainedMessage&)other).m_ids;
	unsigned char chainPrefixLength = Message::getIdLength();
	for (unsigned char checkIndex=0; checkIndex<m_ids.size(); checkIndex++) { // check suffix for each part
		vector<unsigned char> id = m_ids[checkIndex];
		for (unsigned char otherIndex=0; otherIndex<otherIds.size(); otherIndex++) {
			vector<unsigned char> otherId = otherIds[otherIndex];
			bool found = false;
			for (unsigned char pos=chainPrefixLength; pos<idLen; pos++) {
				if (id[2+pos] != otherId[2+pos]) {
					found = false;
					break;
				}
				found = true;
			}
			if (found) {
				return true;
			}
		}
	}
	return false;
}

result_t ChainedMessage::prepareMasterPart(SymbolString& master, istringstream& input, char separator, unsigned char index)
{
	size_t cnt = getCount();
	if (index>=cnt)
		return RESULT_ERR_NOTFOUND;

	SymbolString allData(false);
	result_t result = m_data->write(input, pt_masterData, allData, 0, separator);
	if (result != RESULT_OK)
		return result;
	size_t pos = 0, addData = 0;
	if (m_isWrite) {
		addData = m_lengths[0];
		for (size_t i=0; i<index; i++) {
			pos += addData;
			addData = m_lengths[i+1];
		}
	}
	if (pos+addData>allData.size()) {
		return RESULT_ERR_INVALID_POS;
	}

	vector<unsigned char> id = m_ids[index];
	result = master.push_back((unsigned char)(id.size()-2+addData), false, false); // NN
	if (result != RESULT_OK)
		return result;
	for (size_t i = 2; i < id.size(); i++) {
		result = master.push_back(id[i], false, false);
		if (result != RESULT_OK)
			return result;
	}
	for (size_t i=0; i<addData; i++) {
		result = master.push_back(allData[pos+i], false, false);
		if (result != RESULT_OK)
			return result;
	}
	if (index==0) {
		for (size_t i=0; i<cnt; i++) {
			m_lastMasterUpdateTimes[index] = m_lastSlaveUpdateTimes[index] = 0;
		}
	}
	return result;
}

result_t ChainedMessage::storeLastData(SymbolString& master, SymbolString& slave)
{
	// determine index from master ID
	unsigned char index = 0;
	if (checkId(master, &index)) {
		result_t result = storeLastData(pt_masterData, master, index);
		if (result>=RESULT_OK)
			result = storeLastData(pt_slaveData, slave, index);
		return result;
	}
	return RESULT_ERR_INVALID_ARG;
}

result_t ChainedMessage::storeLastData(const PartType partType, SymbolString& data, unsigned char index)
{
	if (index>=m_ids.size())
		return RESULT_ERR_INVALID_ARG;
	if (partType == pt_masterData) {
		switch (data.compareMaster(*m_lastMasterDatas[index])) {
		case 1: // completely different
			*m_lastMasterDatas[index] = data;
			break;
		case 2: // only master address is different
			*m_lastMasterDatas[index] = data;
			break;
		}
		time(&m_lastMasterUpdateTimes[index]);
	} else if (partType == pt_slaveData) {
		if (data != *m_lastSlaveDatas[index]) {
			*m_lastSlaveDatas[index] = data;
		}
		time(&m_lastSlaveUpdateTimes[index]);
	}
	// check arrival time of all parts
	time_t minTime=0, maxTime=0;
	for (index=0; index<m_ids.size(); index++) {
		if (index==0) {
			minTime = maxTime = m_lastMasterUpdateTimes[index];
		} else {
			if (m_lastMasterUpdateTimes[index]<minTime) {
				minTime = m_lastMasterUpdateTimes[index];
			}
			if (m_lastMasterUpdateTimes[index]>maxTime) {
				maxTime = m_lastMasterUpdateTimes[index];
			}
		}
		if (m_lastSlaveUpdateTimes[index]<minTime) {
			minTime = m_lastSlaveUpdateTimes[index];
		}
		if (m_lastSlaveUpdateTimes[index]>maxTime) {
			maxTime = m_lastSlaveUpdateTimes[index];
		}
		if (minTime==0 || maxTime==0 || maxTime-minTime>m_maxTimeDiff) {
			return RESULT_CONTINUE;
		}
	}
	// everything was completely retrieved in short time
	SymbolString master(false);
	SymbolString slave(false);
	size_t offset = 5+(m_ids[0].size()-2); // skip QQ, ZZ, PB, SB, NN
	for (index=0; index<m_ids.size(); index++) {
		SymbolString* add = m_lastMasterDatas[index];
		size_t end = 5+(*add)[4];
		for (size_t pos=index==0?0:offset; pos<end; pos++) {
			master.push_back((*add)[pos], false, false);
		}
		add = m_lastSlaveDatas[index];
		end = 1+(*add)[0];
		for (size_t pos=index==0?0:1; pos<end; pos++) {
			slave.push_back((*add)[pos], false, false);
		}
	}
	// adjust NN
	if (master.size()-5>255 || slave.size()-1>255)
		return RESULT_ERR_INVALID_POS;
	master[4] = (unsigned char)(master.size()-5);
	slave[0] = (unsigned char)(slave.size()-1);
	result_t result = Message::storeLastData(pt_masterData, master, 0);
	if (result==RESULT_OK)
		result = Message::storeLastData(pt_slaveData, slave, 0);
	return result;
}

void ChainedMessage::dumpColumn(ostream& output, size_t column, bool withConditions)
{
	if (column!=7) {
		Message::dumpColumn(output, column, withConditions);
		return;
	}
	bool first = true;
	for (size_t index = 0; index<m_ids.size(); index++) {
		vector<unsigned char> id = m_ids[index];
		for (vector<unsigned char>::const_iterator it = id.begin()+2; it < id.end(); it++) {
			if (first) {
				first = false;
			} else {
				output << VALUE_SEPARATOR;
			}
			output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
		}
		output << LENGTH_SEPARATOR << dec << setw(0) << static_cast<unsigned>(m_lengths[index]);
	}
}


/**
 * Get the first available @a Message from the list.
 * @param messages the list of @a Message instances to check.
 * @param sameIdExtAs the optional @a Message to check for having the same ID.
 */
Message* getFirstAvailable(vector<Message*> &messages, SymbolString* sameIdExtAs) {
	for (vector<Message*>::iterator msgIt = messages.begin(); msgIt != messages.end(); msgIt++) {
		Message* message = *msgIt;
		if (sameIdExtAs && !message->checkId(*sameIdExtAs)) {
			continue;
		}
		if (message->isAvailable()) {
			return message;
		}
	}
	return NULL;
}

/**
 * Get the first available @a Message from the list.
 * @param messages the list of @a Message instances to check.
 * @param sameIdExtAs the optional @a Message to check for having the same ID.
 */
Message* getFirstAvailable(vector<Message*> &messages, Message* sameIdExtAs=NULL) {
	for (vector<Message*>::iterator msgIt = messages.begin(); msgIt != messages.end(); msgIt++) {
		Message* message = *msgIt;
		if (sameIdExtAs && !message->checkId(*sameIdExtAs)) {
			continue;
		}
		if (message->isAvailable()) {
			return message;
		}
	}
	return NULL;
}

/**
 * Split up a list of string values separated by @a VALUE_SEPARATOR.
 * @param valueList the input string to split.
 * @param values the output value list to append to.
 */
result_t splitValues(string valueList, vector<string>& values)
{
	istringstream stream(valueList);
	string str;
	while (getline(stream, str, VALUE_SEPARATOR)) {
		if (str.length()>0 && str[0]=='\'' && str[str.length()-1]=='\'') {
			str = str.substr(1, str.length()-2);
		}
		values.push_back(str);
	}
	return RESULT_OK;
}

/**
 * Split up a list of numeric value ranges separated by @a VALUE_SEPARATOR.
 * @param valueList the input string to split.
 * @param valueRanges the output list of value ranges to append to (pairs of inclusive from-to values).
 */
result_t splitValues(string valueList, vector<unsigned int>& valueRanges)
{
	istringstream stream(valueList);
	string str;
	result_t result;
	while (getline(stream, str, VALUE_SEPARATOR)) {
		FileReader::trim(str);
		if (str.length()==0) {
			return RESULT_ERR_INVALID_ARG;
		}
		bool upto = str[0]=='<';
		if (upto || str[0]=='>') {
			if (str.length()==1) {
				return RESULT_ERR_INVALID_ARG;
			}
			if (upto) {
				valueRanges.push_back(0);
			}
			bool inclusive = str[1]=='=';
			unsigned int val = parseInt(str.substr(inclusive?2:1).c_str(), 10, inclusive?0:1, inclusive?UINT_MAX:(UINT_MAX-1), result);
			if (result!=RESULT_OK) {
				return result;
			}
			valueRanges.push_back(inclusive ? val : (val+(upto?-1:1)));
			if (!upto) {
				valueRanges.push_back(UINT_MAX);
			}
		} else {
			size_t pos = str.find('-');
			if (pos!=string::npos && pos>0) { // range
				unsigned int val = parseInt(str.substr(0, pos).c_str(), 10, 0, UINT_MAX, result);
				if (result!=RESULT_OK) {
					return result;
				}
				valueRanges.push_back(val);
				pos++;
			} else { // single value
				pos = 0;
			}
			unsigned int val = parseInt(str.substr(pos).c_str(), 10, 0, UINT_MAX, result);
			if (result!=RESULT_OK)
				return result;
			valueRanges.push_back(val);
			if (pos==0) {
				valueRanges.push_back(val); // single value
			}
		}
	}
	return RESULT_OK;
}

result_t Condition::create(const string condName, vector<string>::iterator& it, const vector<string>::iterator end, string defaultDest, string defaultCircuit, SimpleCondition*& returnValue)
{
	// name,circuit,messagename,[comment],[fieldname],[ZZ],values   (name already skipped by caller)
	string circuit = it==end ? "" : *(it++); // circuit
	string name = it==end ? "" : *(it++); // messagename
	if (it<end) {
		it++; // comment
	}
	string field = it==end ? "" : *(it++); // fieldname
	string zz = it==end ? "" : *(it++); // ZZ
	unsigned char dstAddress = SYN;
	result_t result = RESULT_OK;
	if (zz.length()==0) {
		zz = defaultDest;
	}
	if (zz.length()>0) {
		dstAddress = (unsigned char)parseInt(zz.c_str(), 16, 0, 0xff, result);
		if (result != RESULT_OK) {
			return result;
		}
		if (dstAddress!=SYN && !isValidAddress(dstAddress, false)) {
			return RESULT_ERR_INVALID_ADDR;
		}
	}
	if (name.length()==0) {
		if (!isValidAddress(dstAddress, false) || isMaster(dstAddress)) {
			return RESULT_ERR_INVALID_ADDR;
		}
	} else if (circuit.length()==0) {
		circuit = defaultCircuit;
	}
	string valueList = it==end ? "" : *(it++);
	if (valueList.length()==0) {
		returnValue = new SimpleCondition(condName, circuit, name, dstAddress, field);
		return RESULT_OK;
	}
	if (valueList[0]=='\'') {
		// strings
		vector<string> values;
		result = splitValues(valueList, values);
		if (result!=RESULT_OK) {
			return result;
		}
		returnValue = new SimpleStringCondition(condName, circuit, name, dstAddress, field, values);
		return RESULT_OK;
	}
	// numbers
	vector<unsigned int> valueRanges;
	result = splitValues(valueList, valueRanges);
	if (result!=RESULT_OK) {
		return result;
	}
	returnValue = new SimpleNumericCondition(condName, circuit, name, dstAddress, field, valueRanges);
	return RESULT_OK;
}

SimpleCondition* SimpleCondition::derive(string valueList)
{
	if (valueList.empty()) {
		return NULL;
	}
	string name = m_condName+valueList;
	if (valueList[0]=='=') {
		valueList.erase(0, 1);
	}
	result_t result;
	if (valueList[0]=='\'') {
		// strings
		vector<string> values;
		result = splitValues(valueList, values);
		if (result!=RESULT_OK) {
			return NULL;
		}
		return new SimpleStringCondition(name, m_circuit, m_name, m_dstAddress, m_field, values);
	}
	// numbers
	if (!isNumeric()) {
		return NULL;
	}
	vector<unsigned int> valueRanges;
	result = splitValues(valueList, valueRanges);
	if (result!=RESULT_OK) {
		return NULL;
	}
	return new SimpleNumericCondition(name, m_circuit, m_name, m_dstAddress, m_field, valueRanges);
}

void SimpleCondition::dump(ostream& output)
{
	output << "[" << m_condName << "]";
	//output << "{name="<<m_name<<",field="<<m_field<<",dst="<<static_cast<unsigned>(m_dstAddress)<<",valuessize="<<static_cast<unsigned>(m_valueRanges.size())<<"}";
}

CombinedCondition* SimpleCondition::combineAnd(Condition* other)
{
	CombinedCondition* ret = new CombinedCondition();
	return ret->combineAnd(this)->combineAnd(other);
}

result_t SimpleCondition::resolve(MessageMap* messages, ostringstream& errorMessage, void (*readMessageFunc)(Message* message))
{
	if (m_message==NULL) {
		Message* message;
		if (m_name.length()==0) {
			message = messages->getScanMessage(m_dstAddress);
			errorMessage << "scan condition " << nouppercase << setw(2) << hex << setfill('0') << static_cast<unsigned>(m_dstAddress);
		} else {
			message = messages->find(m_circuit, m_name, false);
			if (!message) {
				message = messages->find(m_circuit, m_name, false, true);
			}
			errorMessage << "condition " << m_circuit << " " << m_name;
		}
		if (!message) {
			errorMessage << ": message not found";
			return RESULT_ERR_NOTFOUND;
		}
		if (message->getDstAddress()==SYN) {
			if (message->isPassive()) {
				errorMessage << ": invalid passive message";
				return RESULT_ERR_INVALID_ARG;
			}
			if (m_dstAddress==SYN) {
				errorMessage << ": destination address missing";
				return RESULT_ERR_INVALID_ADDR;
			}
			// clone the message with dedicated dstAddress if necessary
			unsigned long long key = message->getDerivedKey(m_dstAddress);
			vector<Message*>* derived = messages->getByKey(key);
			if (derived==NULL) {
				message = message->derive(m_dstAddress, true);
				messages->add(message);
			} else {
				Message* first = getFirstAvailable(*derived, message);
				if (first==NULL) {
					errorMessage << ": conditional derived message " << message->getCircuit() << "." << message->getName() << " for " << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_dstAddress) << " not found" ;
					return RESULT_ERR_INVALID_ARG;
				}
				message = first;
			}
		}

		if (m_hasValues) {
			if (!message->hasField(m_field.length()>0 ? m_field.c_str() : NULL, isNumeric())) {
				errorMessage << (isNumeric() ? ": numeric field " : ": string field ") << m_field << " not found";
				return RESULT_ERR_NOTFOUND;
			}
		}
		m_message = message;
		message->setUsedByCondition();
		if (m_name.length()>0 && !message->isScanMessage()) {
			messages->addPollMessage(message, true);
		}
	}
	if (m_message->getLastUpdateTime()==0 && readMessageFunc!=NULL) {
		(*readMessageFunc)(m_message);
	}
	return RESULT_OK;
}

bool SimpleCondition::isTrue()
{
	if (!m_message) {
		return false;
	}
	if (m_message->getLastChangeTime()>m_lastCheckTime) {
		bool isTrue = !m_hasValues; // for message seen check
		if (!isTrue) {
			isTrue = checkValue(m_message, m_field);
		}
		m_isTrue = isTrue;
		m_lastCheckTime = m_message->getLastChangeTime();
	}
	return m_isTrue;
}


bool SimpleNumericCondition::checkValue(Message* message, string field) {
	unsigned int value = 0;
	result_t result = message->decodeLastDataNumField(value, field.length()==0 ? NULL : field.c_str());
	if (result==RESULT_OK) {
		for (size_t i=0; i+1<m_valueRanges.size(); i+=2) {
			if (m_valueRanges[i]<=value && value<=m_valueRanges[i+1]) {
				return true;
			}
		}
	}
	return false;
}


bool SimpleStringCondition::checkValue(Message* message, string field) {
	ostringstream output;
	result_t result = message->decodeLastData(output, 0, false, field.length()==0 ? NULL : field.c_str());
	if (result==RESULT_OK) {
		string value = output.str();
		for (size_t i=0; i<m_values.size(); i++) {
			if (m_values[i]==value) {
				return true;
			}
		}
	}
	return false;
}


void CombinedCondition::dump(ostream& output)
{
	for (vector<Condition*>::iterator it = m_conditions.begin(); it!=m_conditions.end(); it++) {
		Condition* condition = *it;
		condition->dump(output);
	}
}

result_t CombinedCondition::resolve(MessageMap* messages, ostringstream& errorMessage, void (*readMessageFunc)(Message* message))
{
	for (vector<Condition*>::iterator it = m_conditions.begin(); it!=m_conditions.end(); it++) {
		Condition* condition = *it;
		ostringstream dummy;
		result_t ret = condition->resolve(messages, dummy, readMessageFunc);
		if (ret!=RESULT_OK) {
			errorMessage << dummy.str();
			return ret;
		}
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


result_t Instruction::create(const string contextPath, const string& defaultDest, const string& defaultCircuit, const string& defaultSuffix,
	Condition* condition, const string type, vector<string>::iterator& it, const vector<string>::iterator end, Instruction*& returnValue)
{
	// type[,argument]* (type already skipped by caller)
	bool singleton = false;
	if ((singleton=(type=="load")) || type=="include") {
		if (it==end || (*it).empty()) {
			return RESULT_ERR_INVALID_ARG;
		}
		size_t pos = contextPath.find_last_of('/');
		string path;
		if (pos==string::npos) {
			path = contextPath;
		} else {
			path = contextPath.substr(0, pos+1);
		}
		returnValue = new LoadInstruction(condition, singleton, defaultDest, defaultCircuit, defaultSuffix, path+(*it));
		return RESULT_OK;
	}
	// unknown instruction
	return RESULT_ERR_INVALID_ARG;
}

string Instruction::getDestination()
{
	// ZZ.circuit[.suffix]
	string ret;
	if (!m_defaultDest.empty())
		ret = m_defaultDest;
	if (!m_defaultCircuit.empty() || !m_defaultSuffix.empty()) {
		if (!ret.empty())
			ret += ".";
		if (m_defaultCircuit.empty())
			ret += "*";
		else
			ret += m_defaultCircuit;
		if (!m_defaultSuffix.empty())
			ret += m_defaultSuffix;
	}
	return ret;
}


result_t LoadInstruction::execute(MessageMap* messages, ostringstream& log) {
	result_t result = messages->readFromFile(m_filename, false, m_defaultDest, m_defaultCircuit, m_defaultSuffix);
	if (log.tellp()>0) {
		log << ", ";
	}
	if (result!=RESULT_OK) {
		log << "error " << (isSingleton() ? "loading " : "including ") << m_filename << " for \"" << getDestination() << "\": " << getResultCode(result);
		return result;
	}
	log << (isSingleton() ? "loaded " : "included ") << m_filename << " for \"" << getDestination() << "\"";
	if (isSingleton() && !m_defaultDest.empty()) {
		result_t temp;
		unsigned char address = (unsigned char)parseInt(m_defaultDest.c_str(), 16, 0, 0xff, temp);
		if (temp==RESULT_OK) {
			size_t pos = m_filename.find_last_of('/');
			string filename;
			if (pos==string::npos) {
				filename = m_filename;
			} else {
				filename = m_filename.substr(pos+1);
			}
			messages->addLoadedFile(address, filename);
		}
	}
	return result;
}


result_t MessageMap::add(Message* message, bool storeByName)
{
	unsigned long long key = message->getKey();
	bool conditional = message->isConditional();
	if (!m_addAll) {
		map<unsigned long long, vector<Message*> >::iterator keyIt = m_messagesByKey.find(key);
		if (keyIt != m_messagesByKey.end()) {
			Message* other = getFirstAvailable(keyIt->second, message);
			if (other != NULL) {
				if (!conditional) {
					return RESULT_ERR_DUPLICATE; // duplicate key
				}
				if (!other->isConditional())
					return RESULT_ERR_DUPLICATE; // duplicate key
			}
		}
	}
	bool isPassive = message->isPassive();
	if (storeByName) {
		bool isWrite = message->isWrite();
		string circuit = message->getCircuit();
		FileReader::tolower(circuit);
		string name = message->getName();
		FileReader::tolower(name);
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
		m_messageCount++;
		if (conditional)
			m_conditionalMessageCount++;
		if (isPassive)
			m_passiveMessageCount++;
		addPollMessage(message);
	}
	unsigned char idLength = message->getIdLength();
	if (idLength > m_maxIdLength)
		m_maxIdLength = idLength;
	m_messagesByKey[key].push_back(message);

	return RESULT_OK;
}

result_t MessageMap::addDefaultFromFile(vector< vector<string> >& defaults, vector<string>& row,
	vector<string>::iterator& begin, string defaultDest, string defaultCircuit, string defaultSuffix,
	const string& filename, unsigned int lineNo)
{
	// check for condition in defaults
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
		SimpleCondition* condition = NULL;
		result_t result = Condition::create(type, ++begin, row.end(), defaultDest, defaultCircuit+defaultSuffix, condition);
		if (condition==NULL || result!=RESULT_OK) {
			m_lastError = "invalid condition";
			return result;
		}
		m_conditions[key] = condition;
		return RESULT_OK;
	}
	if (row.size()>1 && defaultCircuit.length()>0) {
		if (row[1].length()==0)
			row[1] = defaultCircuit+defaultSuffix; // set default circuit and suffix: "circuit[.suffix]"
		else if (row[1][0]=='#')
			row[1] = defaultCircuit+defaultSuffix+row[1]; // move security suffix behind default circuit and suffix: "circuit[.suffix]#security"
		else if (defaultSuffix.length()>0 && row[1].find_last_of('.')==string::npos) { // circuit suffix not yet present
			size_t pos = row[1].find_first_of('#');
			if (pos==string::npos)
				row[1] += defaultSuffix; // append default suffix: "circuit.suffix"
			else
				row[1] = row[1].substr(0, pos)+defaultSuffix+row[1].substr(pos); // insert default suffix: "circuit.suffix#security"
		}
	}
	if (row.size()>5 && defaultDest.length()>0 && row[5].length()==0)
		row[5] = defaultDest; // set default destination
	return FileReader::addDefaultFromFile(defaults, row, begin, defaultDest, defaultCircuit, defaultSuffix, filename, lineNo);
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
			condition = NULL;
			while ((pos=types.find(']'))!=string::npos) {
				// simple condition
				string key = filename+":"+types.substr(1, pos-1);
				map<string, Condition*>::iterator it = m_conditions.find(key);
				Condition* add = NULL;
				if (it==m_conditions.end()) {
					// check for on-the-fly condition
					size_t pos = key.find_first_of("=<>", filename.length()+1);
					if (pos!=string::npos) {
						it = m_conditions.find(key.substr(0, pos));
						if (it!=m_conditions.end()) {
							// derive from another condition
							add = it->second->derive(key.substr(pos));
							if (add==NULL) {
								m_lastError = "derive condition with values "+key.substr(pos)+" failed";
								return RESULT_ERR_INVALID_ARG;
							}
							m_conditions[key] = add; // store derived condition
						}
					}
					if (add==NULL) {
						// shared condition not available
						m_lastError = "condition "+types.substr(1, pos-1)+" not defined";
						return RESULT_ERR_NOTFOUND;
					}
				} else {
					add = it->second;
				}
				if (condition) {
					condition = condition->combineAnd(add);
					store = true;
				} else {
					condition = add;
				}
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
	vector< vector<string> >* defaults, const string& defaultDest, const string& defaultCircuit, const string& defaultSuffix,
	const string& filename, unsigned int lineNo)
{
	vector<string>::iterator restart = begin;
	string types = *restart;
	Condition* condition = NULL;
	result_t result = readConditions(types, filename, condition);
	if (result!=RESULT_OK)
		return result;
	if (types.length()>0 && types[0]=='!') {
		// instruction
		types = types.substr(1);
		Instruction* instruction = NULL;
		result_t result = Instruction::create(filename, defaultDest, defaultCircuit, defaultSuffix, condition, types, ++begin, end, instruction);
		if (instruction==NULL || result!=RESULT_OK) {
			m_lastError = "invalid instruction";
			return result;
		}
		map<string, vector<Instruction*> >::iterator it = m_instructions.find(filename);
		if (it == m_instructions.end()) {
			vector<Instruction*> instructions;
			instructions.push_back(instruction);
			m_instructions[filename] = instructions;
		} else {
			it->second.push_back(instruction);
		}
		return RESULT_OK;
	}
	if (types.length() == 0)
		types.append("r");
	else if (types.find(']')!=string::npos)
		return RESULT_ERR_INVALID_ARG;

	result = RESULT_ERR_EOF;
	DataFieldTemplates* templates = getTemplates(filename);
	istringstream stream(types);
	string type;
	vector<Message*> messages;
	while (getline(stream, type, VALUE_SEPARATOR)) {
		FileReader::trim(type);
		*restart = type;
		begin = restart;
		messages.clear();
		result = Message::create(begin, end, defaults, condition, filename, templates, messages);
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

Message* MessageMap::getScanMessage(const unsigned char dstAddress)
{
	if (dstAddress==SYN)
		return m_scanMessage;
	if (!isValidAddress(dstAddress, false) || isMaster(dstAddress))
		return NULL;
	unsigned long long key = m_scanMessage->getDerivedKey(dstAddress);
	vector<Message*>* msgs = getByKey(key);
	if (msgs!=NULL)
		return msgs->front();
	Message* message = m_scanMessage->derive(dstAddress, true);
	add(message);
	return message;
}

result_t MessageMap::resolveConditions(bool verbose) {
	m_lastError = "";
	result_t overallResult = RESULT_OK;
	for (map<string, Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
		Condition* condition = it->second;
		result_t result = resolveCondition(condition);
		if (result!=RESULT_OK) {
			overallResult = result;
		}
	}
	return overallResult;
}

result_t MessageMap::resolveCondition(Condition* condition, void (*readMessageFunc)(Message* message))
{
	ostringstream error;
	result_t result = condition->resolve(this, error, readMessageFunc);
	if (result!=RESULT_OK) {
		string errorMessage = error.str();
		if (errorMessage.length()>0) {
			if (m_lastError.length()>0)
				m_lastError += ", ";
			m_lastError += errorMessage;
		}
	}
	return result;
}

result_t MessageMap::executeInstructions(ostringstream& log, void (*readMessageFunc)(Message* message))
{
	m_lastError = "";
	result_t overallResult = RESULT_OK;
	vector<string> remove;
	for (map<string, vector<Instruction*> >::iterator it = m_instructions.begin(); it != m_instructions.end(); it++) {
		vector<Instruction*> instructions = it->second;
		bool removeSingletons = false;
		vector<Instruction*> remain;
		for (vector<Instruction*>::iterator lit = instructions.begin(); lit != instructions.end(); lit++) {
			Instruction* instruction = *lit;
			if (removeSingletons && instruction->isSingleton()) {
				delete instruction;
				continue;
			}
			Condition* condition = instruction->getCondition();
			bool execute = condition==NULL;
			if (!execute) {
				result_t result = resolveCondition(condition, instruction->isSingleton()?readMessageFunc:NULL);
				if (result!=RESULT_OK) {
					overallResult = result;
				} else if (condition->isTrue()) {
					execute = true;
				}
			}
			if (execute) {
				if (instruction->isSingleton()) {
					removeSingletons = true;
				}
				result_t result = instruction->execute(this, log);
				if (result!=RESULT_OK) {
					overallResult = result;
				}
				delete instruction;
			} else {
				remain.push_back(instruction);
			}
		}
		if (removeSingletons && !remain.empty()) {
			instructions = remain;
			remain.clear();
			for (vector<Instruction*>::iterator lit = instructions.begin(); lit != instructions.end(); lit++) {
				Instruction* instruction = *lit;
				if (!instruction->isSingleton()) {
					remain.push_back(instruction);
					continue;
				}
				delete instruction;
			}
		}
		if (remain.empty()) {
			remove.push_back(it->first);
		} else {
			it->second = remain;
		}
	}
	for (vector<string>::iterator it = remove.begin(); it != remove.end(); it++) {
		m_instructions.erase(*it);
	}
	return overallResult;
}

void MessageMap::addLoadedFile(unsigned char address, string file)
{
	if (file.empty()) return;
	if (m_loadedFiles.find(address)==m_loadedFiles.end())
		m_loadedFiles[address] = "\""+file+"\"";
	else
		m_loadedFiles[address] += ", \""+file+"\"";
}

string MessageMap::getLoadedFiles(unsigned char address)
{
	if (m_loadedFiles.find(address)==m_loadedFiles.end())
		return "";
	return m_loadedFiles[address];
}

vector<Message*>* MessageMap::getByKey(const unsigned long long key) {
	map<unsigned long long, vector<Message*> >::iterator it = m_messagesByKey.find(key);
	if (it != m_messagesByKey.end())
		return &it->second;
	return NULL;
}

Message* MessageMap::find(const string& circuit, const string& name, const bool isWrite, const bool isPassive)
{
	string lcircuit = circuit;
	FileReader::tolower(lcircuit);
	string lname = name;
	FileReader::tolower(lname);
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

deque<Message*> MessageMap::findAll(const string& circuit, const string& name, const bool completeMatch,
	const bool withRead, const bool withWrite, const bool withPassive,
	const bool completeMatchIgnoreCircuitSuffix, const bool onlyAvailable)
{
	bool checkCircuitIgnoreSuffix = completeMatch && completeMatchIgnoreCircuitSuffix;
	deque<Message*> ret;
	string lcircuit = circuit;
	FileReader::tolower(lcircuit);
	string lname = name;
	FileReader::tolower(lname);
	bool checkCircuit = lcircuit.length() > 0;
	bool checkName = name.length() > 0;
	if (checkCircuit && checkCircuitIgnoreSuffix) {
		size_t pos = lcircuit.find('#');
		if (pos!=string::npos) {
			lcircuit.resize(pos);
		}
	}
	for (map<string, vector<Message*> >::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // avoid duplicates: instances stored multiple times have a key starting with "-"
			continue;
		for (vector<Message*>::iterator msgIt = it->second.begin(); msgIt != it->second.end(); msgIt++) {
			Message* message = *msgIt;
			if (checkCircuit) {
				string check = message->getCircuit();
				FileReader::tolower(check);
				if (checkCircuitIgnoreSuffix) {
					size_t pos = check.find('#');
					if (pos!=string::npos) {
						check.resize(pos);
					}
				}
				if (completeMatch ? (check != lcircuit) : (check.find(lcircuit) == check.npos))
					continue;
			}
			if (checkName) {
				string check = message->getName();
				FileReader::tolower(check);
				if (completeMatch ? (check != lname) : (check.find(lname) == check.npos))
					continue;
			}
			if (message->isPassive()) {
				if (!withPassive) {
					continue;
				}
			} else if (message->isWrite()) {
				if (!withWrite) {
					continue;
				}
			} else {
				if (!withRead) {
					continue;
				}
			}
			if (!onlyAvailable || message->isAvailable()) {
				ret.push_back(*msgIt);
			}
		}
	}

	return ret;
}

Message* MessageMap::find(SymbolString& master, bool anyDestination,
	const bool withRead, const bool withWrite, const bool withPassive)
{
	if (master.size() < 5) {
		return NULL;
	}
	unsigned char maxIdLength = master[4];
	if (maxIdLength > m_maxIdLength) {
		maxIdLength = m_maxIdLength;
	}
	if (master.size() < 5+maxIdLength) {
		return NULL;
	}
	if (maxIdLength == 0 && anyDestination && master[2] == 0x07 && master[3] == 0x04) {
		return m_scanMessage;
	}
	unsigned long long baseKey = (unsigned long long)getMasterNumber(master[0]) << (8 * 7); // QQ address for passive message
	baseKey |= (unsigned long long)(anyDestination ? SYN : master[1]) << (8 * 6); // ZZ address
	baseKey |= (unsigned long long)master[2] << (8 * 5); // PB
	baseKey |= (unsigned long long)master[3] << (8 * 4); // SB
	for (unsigned char idLength = maxIdLength; true; idLength--) {
		unsigned long long key = (unsigned long long)idLength << (8 * 7 + 5);
		key |= baseKey;
		int exp = 3;
		for (unsigned char i = 0; i < idLength; i++) {
			key |= (unsigned long long)master[5 + i] << (8 * exp--);
			if (exp == 0)
				exp = 3;
		}

		map<unsigned long long , vector<Message*> >::iterator it;
		if (withPassive) {
			it = m_messagesByKey.find(key);
			if (it != m_messagesByKey.end()) {
				Message* message = getFirstAvailable(it->second, &master);
				if (message)
					return message;
			}
			if ((key & ID_SOURCE_MASK) != 0) {
				key &= ~ID_SOURCE_MASK;
				it = m_messagesByKey.find(key & ~ID_SOURCE_MASK); // try again without specific source master
				if (it != m_messagesByKey.end()) {
					Message* message = getFirstAvailable(it->second, &master);
					if (message)
						return message;
				}
			}
		} else {
			key &= ~ID_SOURCE_MASK;
		}
		if (withRead) {
			it = m_messagesByKey.find(key | ID_SOURCE_ACTIVE_READ); // try again with special value for active read
			if (it != m_messagesByKey.end()) {
				Message* message = getFirstAvailable(it->second, &master);
				if (message)
					return message;
			}
		}
		if (withWrite) {
			it = m_messagesByKey.find(key | ID_SOURCE_ACTIVE_WRITE); // try again with special value for active write
			if (it != m_messagesByKey.end()) {
				Message* message = getFirstAvailable(it->second, &master);
				if (message)
					return message;
			}
		}
		if (idLength == 0)
			break;
	}

	return NULL;
}

void MessageMap::invalidateCache(Message* message)
{
	if (message->m_data==DataFieldSet::getIdentFields())
		return;
	message->m_lastUpdateTime = 0;
	string circuit = message->getCircuit();
	string name = message->getName();
	deque<Message*> messages = findAll(circuit, name, true, true, true, true, true);
	for (deque<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
		Message* checkMessage = *it;
		if (checkMessage!=message) {
			checkMessage->m_lastUpdateTime = 0;
		}
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
	m_loadedFiles.clear();
	// clear poll messages
	while (!m_pollMessages.empty()) {
		m_pollMessages.top();
		m_pollMessages.pop();
	}
	// free message instances by name
	for (map<string, vector<Message*> >::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		vector<Message*> nameMessages = it->second;
		if (it->first[0] != '-') { // avoid double free: instances stored multiple times have a key starting with "-"
			for (vector<Message*>::iterator nit = nameMessages.begin(); nit != nameMessages.end(); nit++) {
				Message* message = *nit;
				map<unsigned long long, vector<Message*> >::iterator keyIt = m_messagesByKey.find(message->getKey());
				if (keyIt != m_messagesByKey.end()) {
					vector<Message*>* keyMessages = &keyIt->second;
					if (!keyMessages->empty()) {
						for (vector<Message*>::iterator kit = keyMessages->begin(); kit != keyMessages->end(); kit++) {
							if (*kit==message) {
								keyMessages->erase(kit--);
							}
						}
					}
				}
				delete message;
			}
		}
		nameMessages.clear();
	}
	// free remaining message instances by key
	for (map<unsigned long long, vector<Message*> >::iterator it = m_messagesByKey.begin(); it != m_messagesByKey.end(); it++) {
		vector<Message*> keyMessages = it->second;
		for (vector<Message*>::iterator kit = keyMessages.begin(); kit != keyMessages.end(); kit++) {
			Message* message = *kit;
			delete message;
		}
		keyMessages.clear();
	}
	// free condition instances
	for (map<string, Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
		delete it->second;
	}
	// free instruction instances
	for (map<string, vector<Instruction*> >::iterator it = m_instructions.begin(); it != m_instructions.end(); it++) {
		vector<Instruction*> instructions = it->second;
		for (vector<Instruction*>::iterator lit = instructions.begin(); lit != instructions.end(); lit++) {
			Instruction* instruction = *lit;
			delete instruction;
		}
		instructions.clear();
	}
	// clear messages by name
	m_messageCount = 0;
	m_conditionalMessageCount = 0;
	m_passiveMessageCount = 0;
	m_messagesByName.clear();
	// clear messages by key
	m_messagesByKey.clear();
	m_conditions.clear();
	m_instructions.clear();
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

void MessageMap::dump(ostream& output, bool withConditions)
{
	bool first = true;
	for (map<string, vector<Message*> >::iterator it = m_messagesByName.begin(); it != m_messagesByName.end(); it++) {
		if (it->first[0] == '-') // skip instances stored multiple times (key starting with "-")
			continue;
		if (m_addAll) {
			for (vector<Message*>::iterator mit = it->second.begin(); mit != it->second.end(); mit++) {
				Message* message = *mit;
				if (!message)
					continue;
				if (first)
					first = false;
				else
					output << endl;
				message->dump(output, NULL, withConditions);
			}
		} else {
			Message* message = getFirstAvailable(it->second);
			if (!message)
				continue;
			if (first)
				first = false;
			else
				output << endl;
			message->dump(output, NULL, withConditions);
		}
	}
	if (!first)
		output << endl;
}

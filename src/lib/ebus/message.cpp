/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/message.h"
#include <string>
#include <vector>
#include <cstring>
#include <locale>
#include <iomanip>
#include <climits>
#include "lib/ebus/data.h"
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

using std::dec;
using std::hex;
using std::nouppercase;
using std::setfill;
using std::setw;
using std::endl;

/** the maximum length of the command ID bytes (in addition to PB/SB) for which the key is distinct. */
#define MAX_ID_KEYLEN 4

/** the bit mask of the source master number in the message key. */
#define ID_SOURCE_MASK (0x1fLL << (8 * 7))

/** the bit mask for the ID length and combined ID bytes in the message key. */
#define ID_LENGTH_AND_IDS_MASK ((7LL << (8 * 7 + 5)) | 0xffffffffLL)

/** the bits in the @a ID_SOURCE_MASK for arbitrary source and active read message. */
#define ID_SOURCE_ACTIVE_WRITE (0x1fLL << (8 * 7))

/** the bits in the @a ID_SOURCE_MASK for arbitrary source and active write message. */
#define ID_SOURCE_ACTIVE_READ (0x1eLL << (8 * 7))

/** special value for invalid message key. */
#define INVALID_KEY 0xffffffffffffffffLL

/** the maximum poll priority for a @a Message referred to by a @a Condition. */
#define POLL_PRIORITY_CONDITION 5

/** the field name constant for the message level. */
static const char* FIELNAME_LEVEL = "level";

/** the known full length field names. */
static const char* knownFieldNamesFull[] = {
    "type", "circuit", FIELNAME_LEVEL, "name", "comment", "qq", "zz", "pbsb", "id", "fields",
};

/** the known full length field names. */
static const char* knownFieldNamesShort[] = {
    "t", "c", "l", "n", "co", "q", "z", "p", "i", "f",
};

/** the number of known field names. */
static const size_t knownFieldCount = sizeof(knownFieldNamesFull) / sizeof(char*);

/** the default field map for messages. */
static const char* defaultMessageFieldMap[] = {  // access level not included in default map
    "type", "circuit", "name", "comment", "qq", "zz", "pbsb", "id",
    "*name", "part", "type", "divisor/values", "unit", "comment",
};

extern DataFieldTemplates* getTemplates(const string filename);

/**
 * Get the normalized message field name for the given name.
 * @param name the input field name.
 * @param supportsLanguage set to true when the field supports multiple language.
 * @return the normalized message field name, or empty if unknown.
 */
string getMessageFieldName(const string name, bool& supportsLanguage) {
  if (name.find("type") != string::npos) {
    return "type";
  }
  if (name == "circuit" || name == "level" || name == "qq" || name == "zz" || name == "pbsb" || name == "id") {
    return name;
  }
  if (name.find("name") != string::npos) {
    return "name";
  }
  supportsLanguage = true;
  if (name.find("comment") == 0) {
    return "comment";
  }
  return "";
}

/*  case MESSAGEFIELD_DATAFIELDS:
    return withDataFields ? "fields" : "";
  */

Message::Message(const string circuit, const string level, const string name,
    const bool isWrite, const bool isPassive, const map<string, string>& attributes,
    const symbol_t srcAddress, const symbol_t dstAddress,
    const vector<symbol_t> id,
    const DataField* data, const bool deleteData,
    const size_t pollPriority,
    Condition* condition)
    : AttributedItem(name, attributes), m_circuit(circuit), m_level(level), m_isWrite(isWrite),
      m_isPassive(isPassive),
      m_srcAddress(srcAddress), m_dstAddress(dstAddress),
      m_id(id), m_key(createKey(id, isWrite, isPassive, srcAddress, dstAddress)),
      m_data(data), m_deleteData(deleteData),
      m_pollPriority(pollPriority),
      m_usedByCondition(false), m_isScanMessage(false), m_condition(condition),
      m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0) {
  if (circuit == "scan") {
    setScanMessage();
    m_pollPriority = 0;
  }
}

Message::Message(const string circuit, const string level, const string name,
    const symbol_t pb, const symbol_t sb,
    const bool broadcast, const DataField* data, const bool deleteData)
    : AttributedItem(name), m_circuit(circuit), m_level(level), m_isWrite(broadcast),
      m_isPassive(false),
      m_srcAddress(SYN), m_dstAddress(broadcast ? BROADCAST : SYN),
      m_id({pb, sb}), m_key(createKey(pb, sb, broadcast)),
      m_data(data), m_deleteData(deleteData),
      m_pollPriority(0),
      m_usedByCondition(false), m_isScanMessage(true), m_condition(NULL),
      m_lastUpdateTime(0), m_lastChangeTime(0), m_pollCount(0), m_lastPollTime(0) {
}


/**
 * Helper method for getting a default if the value is empty.
 * @param value the value to check.
 * @param defaults a @a vector of defaults, or NULL.
 * @param pos the position in defaults.
 * @param replaceStar whether to replace a star in the default with the value.
 * If there is no star in the default and the value is empty, use the complete
 * default instead.
 * @param required don't combine the value with the default when the value is
 * empty and @p replaceStar is @p true.
 * @return the default if available and value is empty, or the value.
 */
string getDefault(const string value, const map<string, string>& defaults, const string fieldName,
    bool replaceStar = false, bool required = false) {
  if (defaults.empty()) {
    return value;
  }
  if (value.length() == 0 && replaceStar && required) {
    return value;
  }
  auto it = defaults.find(fieldName);
  const string defaultStr = it == defaults.end() ? "" : it->second;
  if (!replaceStar || defaultStr.empty()) {
    return value.length() > 0 ? value : defaultStr;
  }
  size_t insertPos = defaultStr.find('*');
  if (insertPos == string::npos) {
    return value.length() == 0 ? defaultStr : value;
  }
  return defaultStr.substr(0, insertPos)+value+defaultStr.substr(insertPos+1);
}

uint64_t Message::createKey(const vector<symbol_t> id,
    const bool isWrite, const bool isPassive,
    const symbol_t srcAddress, const symbol_t dstAddress) {
  uint64_t key = (uint64_t)(id.size()-2) << (8 * 7 + 5);
  if (isPassive) {
    key |= (uint64_t)getMasterNumber(srcAddress) << (8 * 7);  // 0..25
  } else {
    key |= (isWrite ? 0x1fLL : 0x1eLL) << (8 * 7);  // special values for active
  }
  key |= (uint64_t)dstAddress << (8 * 6);
  int exp = 5;
  for (vector<symbol_t>::const_iterator it = id.begin(); it < id.end(); it++) {
    key ^= (uint64_t)*it << (8 * exp--);
    if (exp == 0) {
      exp = 3;
    }
  }
  return key;
}

uint64_t Message::createKey(MasterSymbolString& master, size_t maxIdLength, bool anyDestination) {
  if (master.size() < 5) {
    return INVALID_KEY;
  }
  size_t idLength = master.getDataSize();
  if (maxIdLength < idLength) {
    idLength = maxIdLength;
  }
  if (master.getDataSize() < idLength) {
    return INVALID_KEY;
  }
  uint64_t key = (uint64_t)idLength << (8 * 7 + 5);
  key |= (uint64_t)getMasterNumber(master[0]) << (8 * 7);  // QQ address for passive message
  key |= (uint64_t)(anyDestination ? SYN : master[1]) << (8 * 6);  // ZZ address
  key |= (uint64_t)master[2] << (8 * 5);  // PB
  key |= (uint64_t)master[3] << (8 * 4);  // SB
  int exp = 3;
  for (size_t i = 0; i < idLength; i++) {
    key ^= (uint64_t)master.dataAt(i) << (8 * exp--);
    if (exp == 0) {
      exp = 3;
    }
  }
  return key;
}

uint64_t Message::createKey(const symbol_t pb, const symbol_t sb, const bool broadcast) {
  uint64_t key = 0;
  key |= (broadcast ? 0x1fLL : 0x1eLL) << (8 * 7);  // special values for active
  key |= (uint64_t)(broadcast ? BROADCAST : SYN) << (8 * 6);
  key |= (uint64_t)pb << (8 * 5);
  key |= (uint64_t)sb << (8 * 4);
  return key;
}

result_t Message::parseId(string input, vector<symbol_t>& id) {
  istringstream in(input);
  while (!in.eof()) {
    while (in.peek() == ' ') {
      in.get();
    }
    if (in.eof()) {  // no more digits
      break;
    }
    input.clear();
    input.push_back(static_cast<char>(in.get()));
    if (in.eof()) {
      return RESULT_ERR_INVALID_ARG;  // too short hex
    }
    input.push_back(static_cast<char>(in.get()));

    result_t result;
    symbol_t value = (symbol_t)parseInt(input.c_str(), 16, 0, 0xff, result);
    if (result != RESULT_OK) {
      return result;  // invalid hex value
    }
    id.push_back(value);
  }
  return RESULT_OK;
}

result_t Message::create(map<string, string> row, vector< map<string, string> > subRows,
    map<string, map<string, string> >& rowDefaults, map<string, vector< map<string, string> > >& subRowDefaults,
    string& errorDescription, Condition* condition, const string filename, DataFieldTemplates* templates,
    vector<Message*>& messages) {
  // [type],[circuit],name,[comment],[QQ[;QQ]*],[ZZ],[PBSB],[ID],fields...
  result_t result;
  bool isWrite = false, isPassive = false;
  string defaultName;
  size_t pollPriority = 0;
  string typeStr = pluck(row, "type");
  if (typeStr.empty()) {
    errorDescription = "empty type";
    return RESULT_ERR_EOF;
  }
  if (typeStr.empty()) {  // default: active read
    defaultName = "r";
  } else {
    defaultName = typeStr;
    string lower = typeStr;
    FileReader::tolower(lower);
    char type = lower[0];
    if (type == 'r') {  // active read
      char poll = lower[1];
      if (poll >= '0' && poll <= '9') {  // poll priority (=active read)
        pollPriority = poll - '0';
        defaultName.erase(1, 1);  // cut off priority digit
      }
    } else if (type == 'w') {  // active write
      isWrite = true;
    } else {  // any other: passive read/write
      isPassive = true;
      type = lower[1];
      isWrite = type == 'w';  // if type continues with "w" it is treated as passive write
    }
  }

  map<string, string>& defaults = rowDefaults[defaultName];
  string circuit = getDefault(pluck(row, "circuit"), defaults, "circuit", true);  // [circuit[#level]]
  string level = getDefault(pluck(row, "level"), defaults, "level", true);
  size_t pos = circuit.find('#');  // TODO remove some day
  if (pos != string::npos) {
    level = circuit.substr(pos+1);
    circuit.resize(pos);
  }
  if (circuit.empty()) {
    errorDescription = "circuit";
    return RESULT_ERR_MISSING_ARG;  // empty circuit
  }
  string name = getDefault(pluck(row, "name"), defaults, "name", true, true);  // name
  if (name.empty()) {
    errorDescription = "name";
    return RESULT_ERR_MISSING_ARG;  // empty name
  }
  string comment = getDefault(pluck(row, "comment"), defaults, "comment", true);  // [comment]
  if (!comment.empty()) {
    row["comment"] = comment;
  }
  string str = getDefault(pluck(row, "qq"), defaults, "qq");  // [QQ[;QQ]*]
  symbol_t srcAddress;
  if (str.empty()) {
    srcAddress = SYN;  // no specific source
  } else {
    srcAddress = (symbol_t)parseInt(str.c_str(), 16, 0, 0xff, result);
    if (result != RESULT_OK) {
      errorDescription = "qq "+str;
      return result;
    }
    if (!isMaster(srcAddress)) {
      errorDescription = "qq "+str;
      return RESULT_ERR_INVALID_ADDR;
    }
  }

  str = getDefault(pluck(row, "zz"), defaults, "zz");  // [ZZ]
  vector<symbol_t> dstAddresses;
  bool isBroadcastOrMasterDestination = false;
  if (str.empty()) {
    dstAddresses.push_back(SYN);  // no specific destination
  } else {
    istringstream stream(str);
    string token;
    bool first = true;
    while (getline(stream, token, VALUE_SEPARATOR)) {
      FileReader::trim(token);
      symbol_t dstAddress = (symbol_t)parseInt(token.c_str(), 16, 0, 0xff, result);
      if (result != RESULT_OK) {
        errorDescription = "zz "+token;
        return result;
      }
      if (!isValidAddress(dstAddress)) {
        errorDescription = "zz "+token;
        return RESULT_ERR_INVALID_ADDR;
      }
      bool broadcastOrMaster = (dstAddress == BROADCAST) || isMaster(dstAddress);
      if (first) {
        isBroadcastOrMasterDestination = broadcastOrMaster;
        first = false;
      } else if (isBroadcastOrMasterDestination != broadcastOrMaster) {
        errorDescription = "zz "+token;
        return RESULT_ERR_INVALID_ADDR;
      }
      dstAddresses.push_back(dstAddress);
    }
  }

  vector<symbol_t> id;
  str = pluck(row, "pbsb");  // [PBSB]
  bool useDefaults = str.empty();
  if (useDefaults) {
    str = getDefault(str, defaults, "pbsb");
  }
  result = parseId(str, id);
  if (result != RESULT_OK) {
    errorDescription = "pbsb "+str;
    return result;
  }
  if (id.size() != 2) {
    errorDescription = "pbsb "+str;
    return RESULT_ERR_INVALID_ARG;  // missing/to short/to long PBSB
  }
  str = pluck(row, "id");  // [ID] (optional master data)
  string defaultIdPrefix;
  if (useDefaults) {
    defaultIdPrefix = getDefault("", defaults, "id");
  }
  vector< vector<symbol_t> > chainIds;
  vector<size_t> chainLengths;
  istringstream stream(str);
  size_t maxLength = MAX_POS;
  size_t chainLength = 16;
  size_t chainPrefixLength = id.size();
  bool first = true, lastChainLengthSpecified = false;
  while (getline(stream, str, VALUE_SEPARATOR) || first) {
    FileReader::trim(str);
    str = defaultIdPrefix+str;
    size_t lengthPos = str.find(LENGTH_SEPARATOR);
    lastChainLengthSpecified = lengthPos != string::npos;
    if (lastChainLengthSpecified) {
      chainLength = parseInt(str.substr(lengthPos+1).c_str(), 10, 0, MAX_POS, result);
      if (result != RESULT_OK) {
        errorDescription = "id "+str;
        return result;
      }
      str.resize(lengthPos);
    }
    vector<symbol_t> chainId = id;
    result = parseId(str, chainId);
    if (result != RESULT_OK) {
      errorDescription = "id "+str;
      return result;
    }
    if (!chainIds.empty() && chainId.size() != chainIds.front().size()) {
      errorDescription = "id length "+str;
      return RESULT_ERR_INVALID_LIST;
    }
    chainIds.push_back(chainId);
    chainLengths.push_back((symbol_t)chainLength);
    if (first) {
      chainPrefixLength = chainId.size();
      maxLength = 0;
    } else if (chainPrefixLength > 2) {
      vector<symbol_t>& front = chainIds.front();
      for (size_t pos = 2; pos < chainPrefixLength; pos++) {
        if (chainId[pos] != front[pos]) {
          chainPrefixLength = pos;
          break;
        }
      }
    }
    if (maxLength+chainLength > 255) {
      errorDescription = "id length "+str;
      return RESULT_ERR_INVALID_POS;
    }
    maxLength += chainLength;
    first = false;
  }
  id = chainIds.front();
  if (chainIds.size() > 1) {
    if (isPassive) {
      errorDescription = "id (passive)";
      return RESULT_ERR_INVALID_LIST;
    }
    if (id.size() > chainPrefixLength) {
      id.resize(chainPrefixLength);
    }
    if (!lastChainLengthSpecified && chainLength < MAX_POS) {
      maxLength += MAX_POS-chainLength;
    }
  } else if (!lastChainLengthSpecified) {
    maxLength = MAX_POS;
  }
  vector<string> newTypes;
  vector< map<string, string> >& subDefaults = subRowDefaults[defaultName];
  if (!subDefaults.empty()) {
    subRows.insert(subRows.begin(), subDefaults.begin(), subDefaults.end());
  }
  const DataField* data = NULL;
  if (subRows.empty()) {
    vector<const SingleDataField*> fields;
    data = new DataFieldSet("", fields);
  } else {
    result = DataField::create(subRows, errorDescription, templates, data, isWrite, false,
        isBroadcastOrMasterDestination, maxLength);
    if (result != RESULT_OK) {
      return result;
    }
  }
  if (id.size() + data->getLength(pt_masterData, maxLength) > 2 + maxLength
      || data->getLength(pt_slaveData, maxLength) > maxLength) {
    // max NN exceeded
    delete data;
    errorDescription = "data length";
    return RESULT_ERR_INVALID_POS;
  }
  unsigned int index = 0;
  bool multiple = dstAddresses.size() > 1;
  char num[10];
  for (vector<symbol_t>::iterator it = dstAddresses.begin(); it != dstAddresses.end(); it++, index++) {
    symbol_t dstAddress = *it;
    string useCircuit = circuit;
    if (multiple) {
      snprintf(num, sizeof(num), ".%d", index);
      useCircuit = useCircuit + num;
    }
    Message* message;
    if (chainIds.size() > 1) {
      message = new ChainedMessage(useCircuit, level, name, isWrite, row, srcAddress, dstAddress, id, chainIds,
          chainLengths, data, index == 0, pollPriority, condition);
    } else {
      message = new Message(useCircuit, level, name, isWrite, isPassive, row, srcAddress, dstAddress, id, data,
          index == 0, pollPriority, condition);
    }
    messages.push_back(message);
  }
  return RESULT_OK;
}

Message* Message::createScanMessage(bool broadcast) {
  return new Message("scan", "", "", 0x07, 0x04, broadcast, DataFieldSet::getIdentFields(), !broadcast);
}

bool Message::extractFieldNames(string str, vector<string>& fields, bool checkAbbreviated) {
  istringstream input(str);
  vector<string> row;
  string column;
  while (getline(input, column, FIELD_SEPARATOR)) {
    size_t idx = knownFieldCount;
    for (size_t i = 0; i < knownFieldCount; i++) {
      if (column == knownFieldNamesFull[i]) {
        idx = i;
        break;
      }
      if (checkAbbreviated && column == knownFieldNamesShort[i]) {
        idx = i;
        break;
      }
    }
    if (idx != knownFieldCount) {
      column = knownFieldNamesFull[idx];
    }  // else: custom attribute
    fields.push_back(column);
  }
  return !fields.empty();
}

Message* Message::derive(const symbol_t dstAddress, const symbol_t srcAddress, const string circuit) const {
  Message* result = new Message(circuit.length() == 0 ? m_circuit : circuit, m_level, m_name,
    m_isWrite, m_isPassive, m_attributes,
    srcAddress == SYN ? m_srcAddress : srcAddress, dstAddress,
    m_id, m_data, false,
    m_pollPriority, m_condition);
  if (m_isScanMessage) {
    result->setScanMessage();
  }
  return result;
}

Message* Message::derive(const symbol_t dstAddress, const bool extendCircuit) const {
  if (extendCircuit) {
    ostringstream out;
    out << m_circuit << '.' << hex << setw(2) << setfill('0') << static_cast<unsigned>(dstAddress);
    return derive(dstAddress, SYN, out.str());
  }
  return derive(dstAddress, SYN, m_circuit);
}

bool Message::checkLevel(const string level, const string checkLevels) {
  if (level.empty()) {
    return true;
  }
  if (checkLevels.empty()) {
    return false;
  }
  if (checkLevels == "*") {
    return true;
  }
  size_t len = level.length();
  size_t maxLen = checkLevels.length();
  for (size_t pos = checkLevels.find(level); pos != string::npos && pos + len <= maxLen;
      pos = checkLevels.find(level, pos)) {
    if ((pos == 0 || checkLevels[pos - 1] == VALUE_SEPARATOR)
        && (pos + len == maxLen || checkLevels[pos + len] == VALUE_SEPARATOR)) {
      return true;
    }
    pos += len;
  }
  return false;
}

bool Message::checkIdPrefix(const vector<symbol_t>& id) const {
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

bool Message::checkId(const MasterSymbolString& master, size_t* index) const {
  size_t idLen = getIdLength();
  if (master.getDataSize() < idLen) {
    return false;
  }
  for (size_t pos = 0; pos < idLen; pos++) {
    if (m_id[2+pos] != master.dataAt(pos)) {
      return false;
    }
  }
  if (index) {
    *index = 0;
  }
  return true;
}

bool Message::checkId(Message& other) const {
  size_t idLen = getIdLength();
  if (idLen != other.getIdLength() || getCount() > 1) {  // only equal for non-chained messages
    return false;
  }
  return other.checkIdPrefix(m_id);
}

uint64_t Message::getDerivedKey(const symbol_t dstAddress) const {
  return (m_key & ~(0xffLL << (8*6))) | (uint64_t)dstAddress << (8*6);
}

bool Message::setPollPriority(size_t priority) {
  if (priority == m_pollPriority || m_isPassive || isScanMessage() || m_dstAddress == SYN) {
    return false;
  }
  if (m_usedByCondition && (priority == 0 || priority > POLL_PRIORITY_CONDITION)) {
    priority = POLL_PRIORITY_CONDITION;
  }
  bool ret = m_pollPriority == 0 && priority > 0;
  m_pollPriority = priority;
  return ret;
}

void Message::setUsedByCondition() {
  if (m_usedByCondition) {
    return;
  }
  m_usedByCondition = true;
  if (m_pollPriority == 0 || m_pollPriority > POLL_PRIORITY_CONDITION) {
    setPollPriority(POLL_PRIORITY_CONDITION);
  }
}

bool Message::isAvailable() {
  return (m_condition == NULL) || m_condition->isTrue();
}

bool Message::hasField(const char* fieldName, bool numeric) const {
  return m_data->hasField(fieldName, numeric);
}

result_t Message::prepareMaster(const symbol_t srcAddress, MasterSymbolString& master,
    istringstream& input, char separator,
    const symbol_t dstAddress, size_t index) {
  if (m_isPassive) {
    return RESULT_ERR_INVALID_ARG;  // prepare not possible
  }
  master.clear();
  master.push_back(srcAddress);
  if (dstAddress == SYN) {
    if (m_dstAddress == SYN) {
      return RESULT_ERR_INVALID_ADDR;
    }
    master.push_back(m_dstAddress);
  } else {
    master.push_back(dstAddress);
  }
  master.push_back(m_id[0]);
  master.push_back(m_id[1]);
  result_t result = prepareMasterPart(master, input, separator, index);
  if (result != RESULT_OK) {
    return result;
  }
  result = storeLastData(master, index);
  if (result < RESULT_OK) {
    return result;
  }
  return RESULT_OK;
}

result_t Message::prepareMasterPart(MasterSymbolString& master, istringstream& input, char separator,
    size_t index) {
  if (index != 0) {
    return RESULT_ERR_NOTFOUND;
  }
  master.push_back(0);  // length, will be set later
  for (size_t i = 2; i < m_id.size(); i++) {
    master.push_back(m_id[i]);
  }
  result_t result = m_data->write(input, master, getIdLength(), separator);
  if (result != RESULT_OK) {
    return result;
  }
  master.adjustHeader();
  return result;
}

result_t Message::prepareSlave(istringstream& input, SlaveSymbolString& slave) {
  if (m_isWrite) {
    return RESULT_ERR_INVALID_ARG;  // prepare not possible
  }
  slave.clear();
  slave.push_back(0);  // length, will be set later
  result_t result = m_data->write(input, slave, 0);
  if (result != RESULT_OK) {
    return result;
  }
  slave.adjustHeader();
  time(&m_lastUpdateTime);
  if (slave != m_lastSlaveData) {
    m_lastChangeTime = m_lastUpdateTime;
    m_lastSlaveData = slave;
  }
  return result;
}

result_t Message::storeLastData(MasterSymbolString& master, SlaveSymbolString& slave) {
  result_t result = storeLastData(master, 0);
  if (result >= RESULT_OK) {
    result = storeLastData(slave, 0);
  }
  return result;
}

result_t Message::storeLastData(MasterSymbolString& data, size_t index) {
  if (data.size() > 0 && (m_isWrite || this->m_dstAddress == BROADCAST || isMaster(this->m_dstAddress)
      || data.getDataSize() + 2 > m_id.size())) {
    time(&m_lastUpdateTime);
  }
  switch (data.compareTo(m_lastMasterData)) {
  case 1:  // completely different
    m_lastChangeTime = m_lastUpdateTime;
    m_lastMasterData = data;
    break;
  case 2:  // only master address is different
    m_lastMasterData = data;
    break;
  // else: identical
  }
  return RESULT_OK;
}

result_t Message::storeLastData(SlaveSymbolString& data, size_t index) {
  if (data.size() > 0) {
    time(&m_lastUpdateTime);
  }
  if (data != m_lastSlaveData) {
    m_lastChangeTime = m_lastUpdateTime;
    m_lastSlaveData = data;
  }
  return RESULT_OK;
}

result_t Message::decodeLastMasterData(ostringstream& output, OutputFormat outputFormat,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) const {
  size_t offset = m_id.size() - 2;
  result_t result = m_data->read(m_lastMasterData, offset,
      output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
  if (result < RESULT_OK) {
    return result;
  }
  if (result == RESULT_EMPTY && fieldName != NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  return result;
}

result_t Message::decodeLastSlaveData(ostringstream& output, OutputFormat outputFormat,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) const {
  result_t result = m_data->read(m_lastSlaveData, 0,
      output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
  if (result < RESULT_OK) {
    return result;
  }
  if (result == RESULT_EMPTY && fieldName != NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  return result;
}

result_t Message::decodeLastData(ostringstream& output, OutputFormat outputFormat,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) const {
  size_t startPos = output.str().length();
  result_t result = m_data->read(m_lastMasterData, getIdLength(), output, outputFormat, -1,
      leadingSeparator, fieldName, fieldIndex);
  if (result < RESULT_OK) {
    return result;
  }
  bool empty = result == RESULT_EMPTY;
  leadingSeparator |= output.str().length() > startPos;
  result = m_data->read(m_lastSlaveData, 0, output, outputFormat, -1, leadingSeparator, fieldName, fieldIndex);
  if (result < RESULT_OK) {
    return result;
  }
  if (result == RESULT_EMPTY && !empty) {
    result = RESULT_OK;  // OK if at least one part was non-empty
  } else if (result == RESULT_EMPTY && fieldName != NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  return result;
}

result_t Message::decodeLastDataNumField(unsigned int& output, const char* fieldName, ssize_t fieldIndex) const {
  result_t result = m_data->read(m_lastMasterData, getIdLength(), output, fieldName, fieldIndex);
  if (result < RESULT_OK) {
    return result;
  }
  if (result == RESULT_EMPTY) {
    result = m_data->read(m_lastSlaveData, 0, output, fieldName, fieldIndex);
  }
  if (result < RESULT_OK) {
    return result;
  }
  if (result == RESULT_EMPTY) {
    return RESULT_ERR_NOTFOUND;
  }
  return result;
}

bool Message::isLessPollWeight(const Message* other) const {
  size_t tprio = m_pollPriority;
  size_t oprio = other->m_pollPriority;
  size_t tw = tprio * m_pollCount;
  size_t ow = oprio * other->m_pollCount;
  if (tw > ow) {
    return true;
  }
  if (tw < ow) {
    return false;
  }
  if (tprio > oprio) {
    return true;
  }
  if (tprio < oprio) {
    return false;
  }
  if (m_lastPollTime > other->m_lastPollTime) {
    return true;
  }
  return false;
}

void Message::dumpHeader(ostream& output, vector<string>* fieldNames) {
  bool first = true;
  if (fieldNames == NULL) {
    for (auto fieldName : defaultMessageFieldMap) {
      if (first) {
        first = false;
      } else {
        output << FIELD_SEPARATOR;
      }
      output << fieldName;
    }
    return;
  }
  for (auto fieldName : *fieldNames) {
    if (first) {
      first = false;
    } else {
      output << FIELD_SEPARATOR;
    }
    output << fieldName;
  }
}

void Message::dump(ostream& output, vector<string>* fieldNames, bool withConditions) const {
  bool first = true;
  if (fieldNames == NULL) {
    for (auto fieldName : knownFieldNamesFull) {
      if (fieldName == FIELNAME_LEVEL) {
        continue;  // access level not included in default dump format
      }
      if (first) {
        first = false;
      } else {
        output << FIELD_SEPARATOR;
      }
      dumpField(output, fieldName, withConditions);
    }
    return;
  }
  for (auto fieldName : *fieldNames) {
    if (first) {
      first = false;
    } else {
      output << FIELD_SEPARATOR;
    }
    dumpField(output, fieldName, withConditions);
  }
}

void Message::dumpField(ostream& output, string fieldName, bool withConditions) const {
  if (fieldName == "type") {
    if (withConditions && m_condition != NULL) {
      m_condition->dump(output);
    }
    if (m_isPassive) {
      output << "u";
      if (m_isWrite) {
        output << "w";
      }
    } else if (m_isWrite) {
      output << "w";
    } else {
      output << "r";
      if (m_pollPriority > 0) {
        output << static_cast<unsigned>(m_pollPriority);
      }
    }
    return;
  }
  if (fieldName == "circuit") {
    dumpString(output, m_circuit, false);
    return;
  }
  if (fieldName == "level") {
    dumpString(output, m_level, false);
    return;
  }
  if (fieldName == "name") {
    dumpString(output, m_name, false);
    return;
  }
  if (fieldName == "qq") {
    if (m_srcAddress != SYN) {
      output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_srcAddress);
    }
    return;
  }
  if (fieldName == "zz") {
    if (m_dstAddress != SYN) {
      output << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_dstAddress);
    }
    return;
  }
  if (fieldName == "pbsb") {
    for (vector<symbol_t>::const_iterator it = m_id.begin(); it < m_id.begin()+2 && it < m_id.end(); it++) {
      output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
    }
    return;
  }
  if (fieldName == "id") {
    for (vector<symbol_t>::const_iterator it = m_id.begin()+2; it < m_id.end(); it++) {
      output << hex << setw(2) << setfill('0') << static_cast<unsigned>(*it);
    }
    return;
  }
  if (fieldName == "fields") {
    m_data->dump(output);
    return;
  }
  dumpAttribute(output, fieldName, false);
}

void Message::decode(ostringstream& output, OutputFormat outputFormat, bool leadingSeparator,
    vector<string>* fields) const {
  if (leadingSeparator) {
    output << ",";
  }
  output << "\n  \"" << getName() << "\": {";
  output << "\n   \"lastup\": " << setw(0) << dec << static_cast<unsigned>(getLastUpdateTime());
  if (getLastUpdateTime() != 0) {
    output << ",\n   \"zz\": \"" << setfill('0') << setw(2) << hex << static_cast<unsigned>(getDstAddress()) << "\"";
    appendAttributes(output, OF_JSON | outputFormat);
    size_t pos = (size_t) output.tellp();
    output << ",\n   \"fields\": {";
    result_t dret = decodeLastData(output, outputFormat);
    if (dret == RESULT_OK) {
      output << "\n   }";
    } else {
      string prefix = output.str().substr(0, pos);
      output.str("");
      output.clear();  // remove written fields
      output << prefix << ",\n   \"decodeerror\": \"" << getResultCode(dret) << "\"";
    }
  }
  output << ",\n   \"passive\": " << (isPassive() ? "true" : "false");
  output << ",\n   \"write\": " << (isWrite() ? "true" : "false");
  output << "\n  }";
}


ChainedMessage::ChainedMessage(const string circuit, const string level, const string name,
    const bool isWrite, const map<string, string>& attributes,
    const symbol_t srcAddress, const symbol_t dstAddress,
    const vector<symbol_t> id,
    vector< vector<symbol_t> > ids, vector<size_t> lengths,
    const DataField* data, const bool deleteData,
    const size_t pollPriority,
    Condition* condition)
    : Message(circuit, level, name, isWrite, false, attributes,
      srcAddress, dstAddress, id,
      data, deleteData, pollPriority, condition),
      m_ids(ids), m_lengths(lengths),
      m_maxTimeDiff(m_ids.size()*15) {  // 15 seconds per message
  size_t cnt = ids.size();
  m_lastMasterDatas = reinterpret_cast<MasterSymbolString**>(calloc(cnt, sizeof(MasterSymbolString*)));
  m_lastSlaveDatas = reinterpret_cast<SlaveSymbolString**>(calloc(cnt, sizeof(SlaveSymbolString*)));
  m_lastMasterUpdateTimes = reinterpret_cast<time_t*>(calloc(cnt, sizeof(time_t)));
  m_lastSlaveUpdateTimes = reinterpret_cast<time_t*>(calloc(cnt, sizeof(time_t)));
  for (size_t index = 0; index < cnt; index++) {
    m_lastMasterDatas[index] = new MasterSymbolString;
    m_lastSlaveDatas[index] = new SlaveSymbolString();
  }
}

ChainedMessage::~ChainedMessage() {
  for (size_t index = 0; index < m_ids.size(); index++) {
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

Message* ChainedMessage::derive(const symbol_t dstAddress, const symbol_t srcAddress, const string circuit) const {
  ChainedMessage* result = new ChainedMessage(circuit.length() == 0 ? m_circuit : circuit, m_level, m_name,
    m_isWrite, m_attributes,
    srcAddress == SYN ? m_srcAddress : srcAddress, dstAddress,
    m_id, m_ids, m_lengths, m_data, false,
    m_pollPriority, m_condition);
  if (m_isScanMessage) {
    result->setScanMessage();
  }
  return result;
}

bool ChainedMessage::checkId(const MasterSymbolString& master, size_t* index) const {
  size_t idLen = getIdLength();
  if (master.getDataSize() < idLen) {
    return false;
  }
  size_t chainPrefixLength = Message::getIdLength();
  for (size_t pos = 0; pos < chainPrefixLength; pos++) {
    if (m_id[2+pos] != master.dataAt(pos)) {
      return false;  // chain prefix mismatch
    }
  }
  for (size_t checkIndex = 0; checkIndex < m_ids.size(); checkIndex++) {  // check suffix for each part
    vector<symbol_t> id = m_ids[checkIndex];
    bool found = false;
    for (size_t pos = chainPrefixLength; pos < idLen; pos++) {
      if (id[2+pos] != master.dataAt(pos)) {
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

bool ChainedMessage::checkId(Message& other) const {
  size_t idLen = getIdLength();
  if (idLen != other.getIdLength() || other.getCount() == 1) {  // only equal for chained messages
    return false;
  }
  if (!other.checkIdPrefix(m_id)) {
    return false;  // chain prefix mismatch
  }
  vector< vector<symbol_t> > otherIds = ((ChainedMessage&)other).m_ids;
  size_t chainPrefixLength = Message::getIdLength();
  for (size_t checkIndex = 0; checkIndex < m_ids.size(); checkIndex++) {  // check suffix for each part
    vector<symbol_t> id = m_ids[checkIndex];
    for (size_t otherIndex = 0; otherIndex < otherIds.size(); otherIndex++) {
      vector<symbol_t> otherId = otherIds[otherIndex];
      bool found = false;
      for (size_t pos = chainPrefixLength; pos < idLen; pos++) {
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

result_t ChainedMessage::prepareMasterPart(MasterSymbolString& master, istringstream& input, char separator,
    size_t index) {
  size_t cnt = getCount();
  if (index >= cnt) {
    return RESULT_ERR_NOTFOUND;
  }
  MasterSymbolString allData;
  result_t result = m_data->write(input, allData, 0, separator);
  if (result != RESULT_OK) {
    return result;
  }
  size_t pos = 0, addData = 0;
  if (m_isWrite) {
    addData = m_lengths[0];
    for (size_t i = 0; i < index; i++) {
      pos += addData;
      addData = m_lengths[i+1];
    }
  }
  if (pos+addData > allData.getDataSize()) {
    return RESULT_ERR_INVALID_POS;
  }
  vector<symbol_t> id = m_ids[index];
  master.push_back((symbol_t)(id.size()-2+addData));  // NN
  for (size_t i = 2; i < id.size(); i++) {
    master.push_back(id[i]);
  }
  for (size_t i = 0; i < addData; i++) {
    master.push_back(allData.dataAt(pos+i));
  }
  if (index == 0) {
    for (size_t i = 0; i < cnt; i++) {
      m_lastMasterUpdateTimes[index] = m_lastSlaveUpdateTimes[index] = 0;
    }
  }
  return result;
}

result_t ChainedMessage::storeLastData(MasterSymbolString& master, SlaveSymbolString& slave) {
  // determine index from master ID
  size_t index = 0;
  if (checkId(master, &index)) {
    result_t result = storeLastData(master, index);
    if (result >= RESULT_OK) {
      result = storeLastData(slave, index);
    }
    return result;
  }
  return RESULT_ERR_INVALID_ARG;
}

result_t ChainedMessage::storeLastData(MasterSymbolString& data, size_t index) {
  if (index >= m_ids.size()) {
    return RESULT_ERR_INVALID_ARG;
  }
  switch (data.compareTo(*m_lastMasterDatas[index])) {
  case 1:  // completely different
    *m_lastMasterDatas[index] = data;
    break;
  case 2:  // only master address is different
    *m_lastMasterDatas[index] = data;
    break;
  }
  time(&m_lastMasterUpdateTimes[index]);
  return combineLastParts();
}

result_t ChainedMessage::storeLastData(SlaveSymbolString& data, size_t index) {
  if (index >= m_ids.size()) {
    return RESULT_ERR_INVALID_ARG;
  }
  if (data != *m_lastSlaveDatas[index]) {
    *m_lastSlaveDatas[index] = data;
  }
  time(&m_lastSlaveUpdateTimes[index]);
  return combineLastParts();
}

result_t ChainedMessage::combineLastParts() {
  // check arrival time of all parts
  time_t minTime = 0, maxTime = 0;
  for (size_t index = 0; index < m_ids.size(); index++) {
    if (index == 0) {
      minTime = maxTime = m_lastMasterUpdateTimes[index];
    } else {
      if (m_lastMasterUpdateTimes[index] < minTime) {
        minTime = m_lastMasterUpdateTimes[index];
      }
      if (m_lastMasterUpdateTimes[index] > maxTime) {
        maxTime = m_lastMasterUpdateTimes[index];
      }
    }
    if (m_lastSlaveUpdateTimes[index] < minTime) {
      minTime = m_lastSlaveUpdateTimes[index];
    }
    if (m_lastSlaveUpdateTimes[index] > maxTime) {
      maxTime = m_lastSlaveUpdateTimes[index];
    }
    if (minTime == 0 || maxTime == 0 || maxTime-minTime > m_maxTimeDiff) {
      return RESULT_CONTINUE;
    }
  }
  // everything was completely retrieved in short time
  MasterSymbolString master;
  SlaveSymbolString slave;
  size_t offset = m_ids[0].size()-2;
  SymbolString* add = m_lastMasterDatas[0];
  for (size_t pos = 0; pos < 5+offset; pos++) {
    master.push_back((*add)[pos]);  // copy header
  }
  slave.push_back(0);  // NN, set later
  for (size_t index = 0; index < m_ids.size(); index++) {
    add = m_lastMasterDatas[index];
    size_t end = add->getDataSize();
    for (size_t pos = offset; pos < end; pos++) {
      master.push_back(add->dataAt(pos));
    }
    add = m_lastSlaveDatas[index];
    end = add->getDataSize();
    for (size_t pos = 0; pos < end; pos++) {
      slave.push_back(add->dataAt(pos));
    }
  }
  // adjust NN
  if (!master.adjustHeader() || !slave.adjustHeader()) {
    return RESULT_ERR_INVALID_POS;
  }
  result_t result = Message::storeLastData(master, 0);
  if (result == RESULT_OK) {
    result = Message::storeLastData(slave, 0);
  }
  return result;
}

void ChainedMessage::dumpField(ostream& output, string fieldName, bool withConditions) const {
  if (fieldName != "id") {
    Message::dumpField(output, fieldName, withConditions);
    return;
  }
  bool first = true;
  for (size_t index = 0; index < m_ids.size(); index++) {
    vector<symbol_t> id = m_ids[index];
    for (vector<symbol_t>::const_iterator it = id.begin()+2; it < id.end(); it++) {
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
 * @param sameIdExtAs the optional @a MasterSymbolString to check for having the same ID.
 * @param onlyAvailable true to include only available messages (default true), false to also include messages that
 * are currently not available (e.g. due to unresolved or false conditions).
 */
Message* getFirstAvailable(const vector<Message*> &messages, const MasterSymbolString* sameIdExtAs,
    const bool onlyAvailable = true) {
  for (auto message : messages) {
    if (sameIdExtAs && !message->checkId(*sameIdExtAs)) {
      continue;
    }
    if (!onlyAvailable || message->isAvailable()) {
      return message;
    }
  }
  return NULL;
}

/**
 * Get the first available @a Message from the list.
 * @param messages the list of @a Message instances to check.
 * @param sameIdExtAs the optional @a Message to check for having the same ID.
 * @param onlyAvailable true to include only available messages (default true), false to also include messages that
 * are currently not available (e.g. due to unresolved or false conditions).
 */
Message* getFirstAvailable(const vector<Message*> &messages, Message* sameIdExtAs = NULL,
    const bool onlyAvailable = true) {
  for (auto message : messages) {
    if (sameIdExtAs && !message->checkId(*sameIdExtAs)) {
      continue;
    }
    if (!onlyAvailable || message->isAvailable()) {
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
result_t splitValues(string valueList, vector<string>& values) {
  istringstream stream(valueList);
  string str;
  while (getline(stream, str, VALUE_SEPARATOR)) {
    if (str.length() > 0 && str[0] == '\'' && str[str.length()-1] == '\'') {
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
result_t splitValues(string valueList, vector<unsigned int>& valueRanges) {
  istringstream stream(valueList);
  string str;
  result_t result;
  while (getline(stream, str, VALUE_SEPARATOR)) {
    FileReader::trim(str);
    if (str.length() == 0) {
      return RESULT_ERR_INVALID_ARG;
    }
    bool upto = str[0] == '<';
    if (upto || str[0] == '>') {
      if (str.length() == 1) {
        return RESULT_ERR_INVALID_ARG;
      }
      if (upto) {
        valueRanges.push_back(0);
      }
      bool inclusive = str[1] == '=';
      unsigned int val = parseInt(str.substr(inclusive?2:1).c_str(), 10, inclusive?0:1,
          inclusive?UINT_MAX:(UINT_MAX-1), result);
      if (result != RESULT_OK) {
        return result;
      }
      valueRanges.push_back(inclusive ? val : (val+(upto?-1:1)));
      if (!upto) {
        valueRanges.push_back(UINT_MAX);
      }
    } else {
      size_t pos = str.find('-');
      if (pos != string::npos && pos > 0) {  // range
        unsigned int val = parseInt(str.substr(0, pos).c_str(), 10, 0, UINT_MAX, result);
        if (result != RESULT_OK) {
          return result;
        }
        valueRanges.push_back(val);
        pos++;
      } else {  // single value
        pos = 0;
      }
      unsigned int val = parseInt(str.substr(pos).c_str(), 10, 0, UINT_MAX, result);
      if (result != RESULT_OK) {
        return result;
      }
      valueRanges.push_back(val);
      if (pos == 0) {
        valueRanges.push_back(val);  // single value
      }
    }
  }
  return RESULT_OK;
}

result_t Condition::create(const string condName, map<string, string> row, map<string, string> rowDefaults,
    SimpleCondition*& returnValue) {
  // type=name,circuit,name=messagename,[comment],qq=[fieldname],[ZZ],pbsb=values
  string circuit = row["circuit"];  // circuit[#level]
  string level;
  size_t pos = circuit.find('#');
  if (pos != string::npos) {
    level = circuit.substr(pos+1);
    circuit.resize(pos);
  }
  string name = row["name"];  // messagename
  string field = row["qq"];  // fieldname
  string zz = row["zz"];  // ZZ
  symbol_t dstAddress = SYN;
  result_t result = RESULT_OK;
  if (zz.empty()) {
    zz = rowDefaults["zz"];
  }
  if (zz.length() > 0) {
    dstAddress = (symbol_t)parseInt(zz.c_str(), 16, 0, 0xff, result);
    if (result != RESULT_OK) {
      return result;
    }
    if (dstAddress != SYN && !isValidAddress(dstAddress, false)) {
      return RESULT_ERR_INVALID_ADDR;
    }
  }
  if (name.length() == 0) {
    if (!isValidAddress(dstAddress, false) || isMaster(dstAddress)) {
      return RESULT_ERR_INVALID_ADDR;
    }
  } else if (circuit.empty()) {
    circuit = rowDefaults["circuit"];
  }
  string valueList = row["pbsb"];
  if (valueList.empty()) {
    valueList = row["id"];
  }
  if (valueList.empty()) {
    returnValue = new SimpleCondition(condName, condName, circuit, level, name, dstAddress, field);
    return RESULT_OK;
  }
  if (valueList[0] == '\'') {
    // strings
    vector<string> values;
    result = splitValues(valueList, values);
    if (result != RESULT_OK) {
      return result;
    }
    returnValue = new SimpleStringCondition(condName, condName, circuit, level, name, dstAddress, field, values);
    return RESULT_OK;
  }
  // numbers
  vector<unsigned int> valueRanges;
  result = splitValues(valueList, valueRanges);
  if (result != RESULT_OK) {
    return result;
  }
  returnValue = new SimpleNumericCondition(condName, condName, circuit, level, name, dstAddress, field, valueRanges);
  return RESULT_OK;
}

SimpleCondition* SimpleCondition::derive(string valueList) const {
  if (valueList.empty()) {
    return NULL;
  }
  string name = m_condName+valueList;
  if (valueList[0] == '=') {
    valueList.erase(0, 1);
  }
  result_t result;
  if (valueList[0] == '\'') {
    // strings
    vector<string> values;
    result = splitValues(valueList, values);
    if (result != RESULT_OK) {
      return NULL;
    }
    return new SimpleStringCondition(name, m_refName, m_circuit, m_level, m_name, m_dstAddress, m_field, values);
  }
  // numbers
  if (!isNumeric()) {
    return NULL;
  }
  vector<unsigned int> valueRanges;
  result = splitValues(valueList, valueRanges);
  if (result != RESULT_OK) {
    return NULL;
  }
  return new SimpleNumericCondition(name, m_refName, m_circuit, m_level, m_name, m_dstAddress, m_field, valueRanges);
}

void SimpleCondition::dump(ostream& output, bool matched) const {
  if (matched) {
    if (!m_isTrue) {
      return;
    }
    output << "[" << m_refName;
    if (m_hasValues) {
      output << "=" << m_matchedValue;
    }
    output << "]";
  } else {
    output << "[" << m_condName << "]";
  }
}

CombinedCondition* SimpleCondition::combineAnd(Condition* other) {
  CombinedCondition* ret = new CombinedCondition();
  return ret->combineAnd(this)->combineAnd(other);
}

result_t SimpleCondition::resolve(MessageMap* messages, ostringstream& errorMessage,
    void (*readMessageFunc)(Message* message)) {
  if (m_message == NULL) {
    Message* message;
    if (m_name.length() == 0) {
      message = messages->getScanMessage(m_dstAddress);
      errorMessage << "scan condition " << nouppercase << setw(2) << hex << setfill('0')
          << static_cast<unsigned>(m_dstAddress);
    } else {
      message = messages->find(m_circuit, m_name, m_level, false);
      if (!message) {
        message = messages->find(m_circuit, m_name, m_level, false, true);
      }
      errorMessage << "condition " << m_circuit << " " << m_name;
    }
    if (!message) {
      errorMessage << ": message not found";
      return RESULT_ERR_NOTFOUND;
    }
    if (message->getDstAddress() == SYN) {
      if (message->isPassive()) {
        errorMessage << ": invalid passive message";
        return RESULT_ERR_INVALID_ARG;
      }
      if (m_dstAddress == SYN) {
        errorMessage << ": destination address missing";
        return RESULT_ERR_INVALID_ADDR;
      }
      // clone the message with dedicated dstAddress if necessary
      uint64_t key = message->getDerivedKey(m_dstAddress);
      const vector<Message*>* derived = messages->getByKey(key);
      if (derived == NULL) {
        message = message->derive(m_dstAddress, true);
        messages->add(message);
      } else {
        Message* first = getFirstAvailable(*derived, message);
        if (first == NULL) {
          errorMessage << ": conditional derived message " << message->getCircuit() << "." << message->getName()
              << " for " << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_dstAddress) << " not found";
          return RESULT_ERR_INVALID_ARG;
        }
        message = first;
      }
    }

    if (m_hasValues) {
      if (!message->hasField(m_field.length() > 0 ? m_field.c_str() : NULL, isNumeric())) {
        errorMessage << (isNumeric() ? ": numeric field " : ": string field ") << m_field << " not found";
        return RESULT_ERR_NOTFOUND;
      }
    }
    m_message = message;
    message->setUsedByCondition();
    if (m_name.length() > 0 && !message->isScanMessage()) {
      messages->addPollMessage(message, true);
    }
  }
  if (m_message->getLastUpdateTime() == 0 && readMessageFunc != NULL) {
    (*readMessageFunc)(m_message);
  }
  return RESULT_OK;
}

bool SimpleCondition::isTrue() {
  if (!m_message) {
    return false;
  }
  if (m_message->getLastChangeTime() > m_lastCheckTime) {
    bool isTrue = !m_hasValues;  // for message seen check
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
  result_t result = message->decodeLastDataNumField(value, field.length() == 0 ? NULL : field.c_str());
  if (result == RESULT_OK) {
    for (size_t i = 0; i+1 < m_valueRanges.size(); i+=2) {
      if (m_valueRanges[i] <= value && value <= m_valueRanges[i+1]) {
        ostringstream out;
        out << static_cast<unsigned>(value);
        m_matchedValue = out.str();
        return true;
      }
    }
  }
  return false;
}


bool SimpleStringCondition::checkValue(Message* message, string field) {
  ostringstream output;
  result_t result = message->decodeLastData(output, 0, false, field.length() == 0 ? NULL : field.c_str());
  if (result == RESULT_OK) {
    string value = output.str();
    for (size_t i = 0; i < m_values.size(); i++) {
      if (m_values[i] == value) {
        m_matchedValue = "'"+value+"'";
        return true;
      }
    }
  }
  return false;
}


void CombinedCondition::dump(ostream& output, bool matched) const {
  for (auto condition : m_conditions) {
    condition->dump(output, matched);
  }
}

result_t CombinedCondition::resolve(MessageMap* messages, ostringstream& errorMessage,
    void (*readMessageFunc)(Message* message)) {
  for (auto condition : m_conditions) {
    ostringstream dummy;
    result_t ret = condition->resolve(messages, dummy, readMessageFunc);
    if (ret != RESULT_OK) {
      errorMessage << dummy.str();
      return ret;
    }
  }
  return RESULT_OK;
}

bool CombinedCondition::isTrue() {
  for (vector<Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
    if (!(*it)->isTrue()) {
      return false;
    }
  }
  return true;
}


result_t Instruction::create(const string& contextPath, const string type,
    Condition* condition, map<string, string>& row, map<string, string>& defaults,
    Instruction*& returnValue) {
  // type[,argument]*
  bool singleton = type == "load";
  if (singleton || type == "include") {
    if (row.empty()) {
      return RESULT_ERR_INVALID_ARG;
    }
    size_t pos = contextPath.find_last_of('/');
    string path;
    if (pos == string::npos) {
      path = contextPath;
    } else {
      path = contextPath.substr(0, pos+1);
    }
    string arg = row["file"];
    row.erase("file");
    for (auto entry : row) {  // fallback to first field
      if (!entry.second.empty()) {
        arg = entry.second;
        break;
      }
    }
    returnValue = new LoadInstruction(condition, singleton, defaults, path+arg);
    return RESULT_OK;
  }
  // unknown instruction
  return RESULT_ERR_INVALID_ARG;
}

string Instruction::getDestination() const {
  // ZZ.circuit[.suffix]
  string ret;
  auto it = m_defaults.find("zz");
  if (it != m_defaults.end() && !it->second.empty()) {
    ret = it->second;
  }
  it = m_defaults.find("circuit");
  string circuit = it == m_defaults.end() ? "" : it->second;
  it = m_defaults.find("suffix");
  string suffix = it == m_defaults.end() ? "" : it->second;
  if (!circuit.empty() || !suffix.empty()) {
    if (!ret.empty()) {
      ret += ".";
    }
    if (circuit.empty()) {
      ret += "*";
    } else {
      ret += circuit;
    }
    if (!suffix.empty()) {
      ret += suffix;
    }
  }
  return ret;
}


result_t LoadInstruction::execute(MessageMap* messages, ostringstream& log, Condition* condition) {
  string errorDescription;
  result_t result = messages->readFromFile(m_filename, errorDescription, false, &m_defaults);
  if (log.tellp() > 0) {
    log << ", ";
  }
  if (result != RESULT_OK) {
    log << "error " << (isSingleton() ? "loading \"" : "including \"") << m_filename << "\" for \""
        << getDestination() << "\": " << getResultCode(result);
    if (!errorDescription.empty()) {
      log << " " << errorDescription;
    }
    return result;
  }
  log << (isSingleton() ? "loaded \"" : "included \"") << m_filename << "\" for \"" << getDestination() << "\"";
  if (isSingleton() && !m_defaults["zz"].empty()) {
    result_t temp;
    symbol_t address = (symbol_t)parseInt(m_defaults["zz"].c_str(), 16, 0, 0xff, temp);
    if (temp == RESULT_OK) {
      size_t pos = m_filename.find_last_of('/');
      string filename;
      if (pos == string::npos) {
        filename = m_filename;
      } else {
        filename = m_filename.substr(pos+1);
      }
      string comment;
      if (condition) {
        ostringstream out;
        condition->dump(out, true);
        comment = out.str();
        log << " ("+comment+")";
      }
      messages->addLoadedFile(address, filename, comment);
    }
  }
  return result;
}


vector<string> MessageMap::s_noFiles;

result_t MessageMap::add(Message* message, bool storeByName) {
  uint64_t key = message->getKey();
  bool conditional = message->isConditional();
  if (!m_addAll) {
    map<uint64_t, vector<Message*> >::iterator keyIt = m_messagesByKey.find(key);
    if (keyIt != m_messagesByKey.end()) {
      Message* other = getFirstAvailable(keyIt->second, message);
      if (other != NULL) {
        if (!conditional) {
          return RESULT_ERR_DUPLICATE;  // duplicate key
        }
        if (!other->isConditional()) {
          return RESULT_ERR_DUPLICATE;  // duplicate key
        }
      }
    }
  }
  bool isPassive = message->isPassive();
  if (storeByName) {
    bool isWrite = message->isWrite();
    string circuit = message->getCircuit();
    FileReader::tolower(circuit);
    if (circuit == "scan") {
      m_additionalScanMessages = true;
    }
    string name = message->getName();
    FileReader::tolower(name);
    string suffix = FIELD_SEPARATOR + name + (isPassive ? "P" : (isWrite ? "W" : "R"));
    string nameKey = circuit + suffix;
    if (!m_addAll) {
      map<string, vector<Message*> >::iterator nameIt = m_messagesByName.find(nameKey);
      if (nameIt != m_messagesByName.end()) {
        vector<Message*>* messages = &nameIt->second;
        if (!message->isConditional() || !messages->front()->isConditional()) {
          return RESULT_ERR_DUPLICATE_NAME;  // duplicate key
        }
      }
    }
    m_messagesByName[nameKey].push_back(message);
    nameKey = suffix;  // also store without circuit
    map<string, vector<Message*> >::iterator nameIt = m_messagesByName.find(nameKey);
    if (nameIt == m_messagesByName.end()) {
      // always store first message without circuit (in order of circuit name)
      m_messagesByName[nameKey].push_back(message);
    } else {
      vector<Message*>* messages = &nameIt->second;
      Message* first = messages->front();
      if (circuit < first->getCircuit()) {
        // always store first message without circuit (in order of circuit name)
        m_messagesByName[nameKey].at(0) = message;
      } else if (m_addAll || (conditional && first->isConditional())) {
        // store further messages only if both are conditional or if storing everything
        m_messagesByName[nameKey].push_back(message);
      }
    }
    m_messageCount++;
    if (conditional) {
      m_conditionalMessageCount++;
    }
    if (isPassive) {
      m_passiveMessageCount++;
    }
    addPollMessage(message);
  }
  size_t idLength = message->getIdLength();
  if (message->getDstAddress() == BROADCAST && idLength > m_maxBroadcastIdLength) {
    m_maxBroadcastIdLength = idLength;
  }
  if (idLength > m_maxIdLength) {
    m_maxIdLength = idLength;
  }
  m_messagesByKey[key].push_back(message);
  return RESULT_OK;
}

result_t MessageMap::getFieldMap(vector<string>& row, string& errorDescription, const string preferLanguage) const {
  // type (r[1-9];w;u),circuit,name,[comment],[QQ],ZZ,PBSB,[ID],field1,part (m/s),datatypes/templates,divider/values,
  //  unit,comment
  // minimum: type,name,PBSB,field,datatype
  if (row.empty()) {
    for (auto col : defaultMessageFieldMap) {
      row.push_back(col);
    }
    return RESULT_OK;
  }
  bool inDataFields = false;
  map<string, size_t> seen;
  for (size_t col = 0; col < row.size(); col++) {
    string &name = row[col];
    string lowerName = name;
    tolower(lowerName);
    trim(lowerName);
    if (lowerName.empty()) {
      errorDescription = "missing name in column " + AttributedItem::formatInt(col);
      return RESULT_ERR_INVALID_ARG;
    }
    bool supportsLang = false, toDataFields = false;
    string useName;
    if (inDataFields) {
      useName = getDataFieldName(lowerName, supportsLang);
    } else {
      useName = getMessageFieldName(lowerName, supportsLang);
      if (useName.empty()) {
        useName = getDataFieldName(lowerName, supportsLang);
        toDataFields = !useName.empty();
      }
    }
    bool unknown = useName.empty();
    size_t langPos = supportsLang ? lowerName.find_last_of('.') : string::npos;
    map<string, size_t>::iterator previous;
    if (langPos == lowerName.length()-3) {
      string lang = lowerName.substr(langPos+1);
      if (unknown) {
        useName = lowerName.substr(0, langPos);
      }
      previous = seen.find(useName);
      if (previous != seen.end()) {
        if (lang != preferLanguage) {
          // skip this column
          name = SKIP_COLUMN;
          continue;
        }
        // replace previous
        row[previous->second] = SKIP_COLUMN;
        seen.erase(useName);
        previous = seen.end();
      }
    } else {
      if (unknown) {
        useName = lowerName;
      }
      previous = seen.find(useName);
    }
    if (inDataFields) {
      if (!unknown && previous != seen.end()) {
        if (seen.find("name") == seen.end() || seen.find("type") == seen.end()) {
          errorDescription = "missing field name/type as of already seen "+useName;
          return RESULT_ERR_EOF;  // require at least name and type
        }
        seen.clear();
      }
    } else {
      /*if (!unknown && (useName != "name" || seen.find("name") == seen.end())) {
        // keep first name for message
      } else {*/
      if (toDataFields) {
        if (seen.find("type") == seen.end() || seen.find("name") == seen.end() || seen.find("pbsb") == seen.end()) {
          errorDescription = "missing message name/type/pbsb";
          return RESULT_ERR_EOF;  // require at least type, name, and pbsb
        }
        inDataFields = true;
        seen.clear();
      }
      if (!inDataFields && seen.find(useName) != seen.end()) {
        errorDescription = "duplicate message " + useName;
        return RESULT_ERR_INVALID_ARG;
      }
    }
    if (seen.empty() && inDataFields) {
      name = "*" + useName;  // data field repetition
    } else {
      name = useName;
    }
    seen[useName] = col;
  }
  if (inDataFields) {
    if (seen.find("name") == seen.end() || seen.find("type") == seen.end()) {
      errorDescription = "missing field name/type";
      return RESULT_ERR_EOF;  // require at least name and type
    }
  } else if (seen.find("type") == seen.end() || seen.find("name") == seen.end() || seen.find("pbsb") == seen.end()) {
    errorDescription = "missing message name/type/pbsb";
    return RESULT_ERR_EOF;  // require at least type, name, and pbsb
  }
  return RESULT_OK;
}

result_t MessageMap::addDefaultFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
    string& errorDescription, const string filename, unsigned int lineNo) {
  // check for condition in defaults
  string type = row["type"];
  row.erase("type");
  auto mainDefaults = getDefaults().find("");
  map<string, string> defaults;
  if (mainDefaults != getDefaults().end()) {
    defaults = mainDefaults->second;
  }
  if (!type.empty() && type[0] == '[' && type[type.length()-1] == ']') {
    // condition
    type = type.substr(1, type.length()-2);
    if (type.find('[') != string::npos || type.find(']') != string::npos) {
      errorDescription = "invalid condition name "+type;
      return RESULT_ERR_INVALID_ARG;
    }
    string key = filename+":"+type;
    map<string, Condition*>::iterator it = m_conditions.find(key);
    if (it != m_conditions.end()) {
      errorDescription = "condition "+type+" already defined";
      return RESULT_ERR_DUPLICATE_NAME;
    }
    SimpleCondition* condition = NULL;
    result_t result = Condition::create(type, row, defaults, condition);
    if (condition == NULL || result != RESULT_OK) {
      errorDescription = "invalid condition";
      return result;
    }
    m_conditions[key] = condition;
    return RESULT_OK;
  }
  string defaultCircuit = defaults["circuit"];
  if (type.empty()) {
    if (defaultCircuit.empty()) {
      errorDescription = "invalid default definition";
      return RESULT_ERR_INVALID_ARG;
    }
    // circuit level additional attributes
  }
  string defaultSuffix = defaults["suffix"];
  defaults.erase("suffix");
  for (auto entry : row) {
    string value = entry.second;
    if (entry.first == "circuit" && !defaultCircuit.empty()) {  // TODO remove some day
      if (value.empty()) {
        value = defaultCircuit+defaultSuffix;  // set default circuit and suffix: "circuit[.suffix]"
      } else if (value[0] == '#') {
        // move access level behind default circuit and suffix: "circuit[.suffix]#level"
        value = defaultCircuit+defaultSuffix+value;
      } else if (!defaultSuffix.empty() && value.find_last_of('.') == string::npos) {
        // circuit suffix not yet present
        size_t pos = value.find_first_of('#');
        if (pos == string::npos) {
          value += defaultSuffix;  // append default suffix: "circuit.suffix"
        } else {
          // insert default suffix: "circuit.suffix#level"
          value = value.substr(0, pos)+defaultSuffix+value.substr(pos);
        }
      }
    }
    if (!value.empty() || defaults[entry.first].empty()) {
      defaults[entry.first] = value;
    }
  }
  if (type.empty()) {
    string circuit = defaults["circuit"];
    defaults.erase("circuit");
    string name = defaults["name"];
    defaults.erase("name");
    if (!name.empty() || !defaults.empty()) {
      m_circuitData[circuit] = new AttributedItem(name, defaults);
    }
    return RESULT_OK;
  }
  getDefaults()[type] = defaults;
  vector< map<string, string> > subDefaults = subRows;  // ensure to have a copy
  getSubDefaults()[type] = subDefaults;
  return RESULT_OK;
}

result_t MessageMap::readConditions(string& types, const string filename, string& errorDescription,
    Condition*& condition) {
  size_t pos;
  if (types.length() > 0 && types[0] == '[' && (pos=types.find_last_of(']')) != string::npos) {
    // check if combined or simple condition is already known
    const string combinedkey = filename+":"+types.substr(1, pos-1);
    auto it = m_conditions.find(combinedkey);
    if (it != m_conditions.end()) {
      condition = it->second;
      types = types.substr(pos+1);
    } else {
      bool store = false;
      condition = NULL;
      while ((pos=types.find(']')) != string::npos) {
        // simple condition
        string key = filename+":"+types.substr(1, pos-1);
        map<string, Condition*>::iterator it = m_conditions.find(key);
        Condition* add = NULL;
        if (it == m_conditions.end()) {
          // check for on-the-fly condition
          size_t pos = key.find_first_of("=<>", filename.length()+1);
          if (pos != string::npos) {
            it = m_conditions.find(key.substr(0, pos));
            if (it != m_conditions.end()) {
              // derive from another condition
              add = it->second->derive(key.substr(pos));
              if (add == NULL) {
                errorDescription = "derive condition with values "+key.substr(pos)+" failed";
                return RESULT_ERR_INVALID_ARG;
              }
              m_conditions[key] = add;  // store derived condition
            }
          }
          if (add == NULL) {
            // shared condition not available
            errorDescription = "condition "+types.substr(1, pos-1)+" not defined";
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
        if (types.empty() || types[0] != '[') {
          break;
        }
      }
      if (store) {
        m_conditions[combinedkey] = condition;  // store combined condition
      }
    }
  }
  return RESULT_OK;
}

bool MessageMap::extractDefaultsFromFilename(string filename, map<string, string>& defaults,
    symbol_t* destAddress, unsigned int* software, unsigned int* hardware) const {
  string ident, circuit, suffix;
  unsigned int sw = UINT_MAX, hw = UINT_MAX;
  string remain = filename;
  if (remain.length() > 4 && remain.substr(remain.length()-4) == ".csv") {
    remain = remain.substr(0, remain.length()-3);  // including trailing "."
  }
  size_t pos = remain.find('.');
  if (pos != 2) {
    return false;  // missing "ZZ."
  }
  result_t result = RESULT_OK;
  string destStr = remain.substr(0, pos);
  symbol_t dest = (symbol_t)parseInt(destStr.c_str(), 16, 0, 0xff, result, NULL);
  if (result != RESULT_OK || !isValidAddress(dest)) {
    return false;  // invalid "ZZ"
  }
  if (destAddress) {
    *destAddress = dest;
  }
  remain.erase(0, pos);
  if (remain.length() > 1) {
    pos = remain.rfind(".SW");  // check for ".SWxxxx."
    if (pos != string::npos && remain.find(".", pos+1) == pos+7) {
      sw = parseInt(remain.substr(pos+3, 4).c_str(), 10, 0, 9999, result, NULL);
      if (result != RESULT_OK) {
        return false;  // invalid "SWxxxx"
      }
      remain.erase(pos, 7);
    }
  }
  if (software) {
    *software = sw;
  }
  if (remain.length() > 1) {
    pos = remain.rfind(".HW");  // check for ".HWxxxx."
    if (pos != string::npos && remain.find(".", pos+1) == pos+7) {
      hw = parseInt(remain.substr(pos+3, 4).c_str(), 10, 0, 9999, result, NULL);
      if (result != RESULT_OK) {
        return false;  // invalid "HWxxxx"
      }
      remain.erase(pos, 7);
    }
  }
  if (hardware) {
    *hardware = hw;
  }
  if (remain.length() > 1) {
    pos = remain.find('.', 1);  // check for ".IDENT."
    if (pos != string::npos && pos >= 1 && pos <= 6) {
      // up to 5 chars between two "."s, immediately after "ZZ.", or ".."
      ident = circuit = remain.substr(1, pos-1);
      remain.erase(0, pos);
      pos = remain.find('.', 1);  // check for ".CIRCUIT."
      if (pos != string::npos && (pos>2 || remain[1]<'0' || remain[1]>'9')) {
        circuit = remain.substr(1, pos-1);
        remain.erase(0, pos);
        pos = remain.find('.', 1);  // check for ".SUFFIX."
      }
      if (pos != string::npos && pos == 2 && remain[1] >= '0' && remain[1] <= '9') {
        suffix = remain.substr(0, 2);
        remain.erase(0, pos);
      }
    }
  }
  defaults["zz"] = destStr;
  defaults["circuit"] = circuit;
  defaults["suffix"] = suffix;
  defaults["name"] = ident;
  return true;
}

result_t MessageMap::readFromFile(const string filename, string& errorDescription, bool verbose,
    map<string, string>* defaults, size_t* hash, size_t* size, time_t* time) {
  size_t localHash, localSize;
  time_t localTime;
  if (!hash) {
    hash = &localHash;
  }
  if (!size) {
    size = &localSize;
  }
  if (!time) {
    time = &localTime;
  }
  result_t result = MappedFileReader::readFromFile(filename, errorDescription, verbose, defaults, hash, size, time);
  if (result == RESULT_OK) {
    m_loadedFileInfos[filename].m_hash = *hash;
    m_loadedFileInfos[filename].m_size = *size;
    m_loadedFileInfos[filename].m_time = *time;
  }
  return result;
}

result_t MessageMap::addFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
    string& errorDescription, const string filename, unsigned int lineNo) {
  Condition* condition = NULL;
  string types = row["type"];
  result_t result = readConditions(types, filename, errorDescription, condition);
  if (result != RESULT_OK) {
    return result;
  }
  if (!types.empty() && types[0] == '!') {
    // instruction
    if (!subRows.empty()) {
      errorDescription = "invalid instruction";
      return RESULT_ERR_INVALID_ARG;
    }
    types = types.substr(1);
    Instruction* instruction = NULL;
    row.erase("type");
    result_t result = Instruction::create(filename, types, condition, row, getDefaults()[""], instruction);
    if (instruction == NULL || result != RESULT_OK) {
      errorDescription = "invalid instruction";
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
  if (types.length() == 0) {
    types.append("r");
  } else if (types.find(']') != string::npos) {
    errorDescription = "invalid type "+types;
    return RESULT_ERR_INVALID_ARG;
  }
  result = RESULT_ERR_EOF;
  DataFieldTemplates* templates = getTemplates(filename);
  istringstream stream(types);
  string type;
  vector<Message*> messages;
  while (getline(stream, type, VALUE_SEPARATOR)) {
    FileReader::trim(type);
    messages.clear();
    row["type"] = type;
    result = Message::create(row, subRows, getDefaults(), getSubDefaults(), errorDescription, condition, filename,
        templates, messages);
    for (vector<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
      Message* message = *it;
      if (result == RESULT_OK) {
        result = add(message);
        if (result == RESULT_ERR_DUPLICATE_NAME) {
          errorDescription = "invalid name";
        } else if (result == RESULT_ERR_DUPLICATE) {
          errorDescription = "duplicate ID";
        }
      }
      if (result != RESULT_OK) {
        delete message;  // delete all remaining messages on error
      }
    }
    if (result != RESULT_OK) {
      return result;
    }
  }
  return result;
}

Message* MessageMap::getScanMessage(const symbol_t dstAddress) {
  if (dstAddress == SYN) {
    return m_scanMessage;
  }
  if (dstAddress == BROADCAST) {
    return m_broadcastScanMessage;
  }
  if (!isValidAddress(dstAddress, true) || isMaster(dstAddress)) {
    return NULL;
  }
  uint64_t key = m_scanMessage->getDerivedKey(dstAddress);
  const vector<Message*>* msgs = getByKey(key);
  if (msgs != NULL) {
    return msgs->front();
  }
  Message* message = m_scanMessage->derive(dstAddress, true);
  add(message);
  return message;
}

result_t MessageMap::resolveConditions(string& errorDescription, bool verbose) {
  result_t overallResult = RESULT_OK;
  for (map<string, Condition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); it++) {
    Condition* condition = it->second;
    result_t result = resolveCondition(condition, errorDescription);
    if (result != RESULT_OK) {
      overallResult = result;
    }
  }
  return overallResult;
}

result_t MessageMap::resolveCondition(Condition* condition, string& errorDescription,
    void (*readMessageFunc)(Message* message)) {
  ostringstream error;
  result_t result = condition->resolve(this, error, readMessageFunc);
  if (result != RESULT_OK) {
    string errorMessage = error.str();
    if (errorMessage.length() > 0) {
      if (!errorDescription.empty()) {
        errorDescription += ", ";
      }
      errorDescription += errorMessage;
    }
  }
  return result;
}

result_t MessageMap::executeInstructions(ostringstream& log, void (*readMessageFunc)(Message* message)) {
  result_t overallResult = RESULT_OK;
  vector<string> remove;
  for (auto& it : m_instructions) {
    auto& instructions = it.second;
    bool removeSingletons = false;
    vector<Instruction*> remain;
    for (auto instruction : instructions) {
      if (removeSingletons && instruction->isSingleton()) {
        delete instruction;
        continue;
      }
      Condition* condition = instruction->getCondition();
      bool execute = condition == NULL;
      if (!execute) {
        string errorDescription;
        result_t result = resolveCondition(condition, errorDescription,
            instruction->isSingleton()?readMessageFunc:NULL);
        if (result != RESULT_OK) {
          overallResult = result;
          log << "error resolving condition for \"" << instruction->getDestination() << "\": "
              << getResultCode(result);
          if (!errorDescription.empty()) {
            log << " " << errorDescription;
          }
        } else if (condition->isTrue()) {
          execute = true;
        }
      }
      if (execute) {
        if (instruction->isSingleton()) {
          removeSingletons = true;
        }
        result_t result = instruction->execute(this, log, condition);
        if (result != RESULT_OK) {
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
      remove.push_back(it.first);
    } else {
      it.second = remain;
    }
  }
  for (auto it : remove) {
    m_instructions.erase(it);
  }
  return overallResult;
}

void MessageMap::addLoadedFile(symbol_t address, string file, string comment) {
  if (!file.empty()) {
    vector<string>& files = m_loadedFiles[address];
    files.push_back(file);
    if (!comment.empty()) {
      m_loadedFileInfos[file].m_comment = comment;
    }
  }
}

const vector<string>& MessageMap::getLoadedFiles(symbol_t address) const {
  auto files = m_loadedFiles.find(address);
  if (files != m_loadedFiles.end()) {
    return files->second;
  }
  return s_noFiles;
}

vector<string> MessageMap::getLoadedFiles() const {
  vector<string> ret;
  for (auto& loadedFile : m_loadedFileInfos) {
    ret.push_back(loadedFile.first);
  }
  return ret;
}

bool MessageMap::getLoadedFileInfo(string filename, string& comment, size_t* hash, size_t* size, time_t* time) const {
  auto it = m_loadedFileInfos.find(filename);
  if (it == m_loadedFileInfos.end()) {
    comment = "";
    hash = size = 0;
    time = 0;
    return false;
  }
  comment = it->second.m_comment;
  if (hash) {
    *hash = it->second.m_hash;
  }
  if (size) {
    *size = it->second.m_size;
  }
  if (time) {
    *time = it->second.m_time;
  }
  return true;
}

const vector<Message*>* MessageMap::getByKey(const uint64_t key) const {
  auto it = m_messagesByKey.find(key);
  if (it != m_messagesByKey.end()) {
    return &it->second;
  }
  return NULL;
}

Message* MessageMap::find(const string& circuit, const string& name, const string& levels, const bool isWrite,
    const bool isPassive) const {
  string lcircuit = circuit;
  FileReader::tolower(lcircuit);
  string lname = name;
  FileReader::tolower(lname);
  string suffix = FIELD_SEPARATOR + lname + (isPassive ? "P" : (isWrite ? "W" : "R"));
  for (int i = 0; i < 2; i++) {
    string nameKey;
    if (i == 0) {
      nameKey = lcircuit + suffix;
    } else if (lcircuit.empty()) {
      nameKey = suffix;  // second try: without circuit
    } else {
      continue;  // not allowed without circuit
    }
    auto it = m_messagesByName.find(nameKey);
    if (it != m_messagesByName.end()) {
      Message* message = getFirstAvailable(it->second);
      if (message && message->hasLevel(levels)) {
        return message;
      }
    }
  }
  return NULL;
}

deque<Message*> MessageMap::findAll(const string& circuit, const string& name, const string& levels,
    const bool completeMatch, const bool withRead, const bool withWrite, const bool withPassive,
    const bool includeEmptyLevel, const bool onlyAvailable,
    const time_t since, const time_t until) const {
  deque<Message*> ret;
  string lcircuit = circuit;
  FileReader::tolower(lcircuit);
  string lname = name;
  FileReader::tolower(lname);
  bool checkCircuit = lcircuit.length() > 0;
  bool checkLevel = levels != "*";
  bool checkName = lname.length() > 0;
  for (auto it : m_messagesByName) {
    if (it.first[0] == FIELD_SEPARATOR) {  // avoid duplicates: instances stored multiple times have a special key
      continue;
    }
    for (auto message : it.second) {
      if (checkLevel && !message->hasLevel(levels, includeEmptyLevel)) {
        continue;
      }
      if (checkCircuit) {
        string check = message->getCircuit();
        FileReader::tolower(check);
        if (completeMatch ? (check != lcircuit) : (check.find(lcircuit) == check.npos)) {
          continue;
        }
      }
      if (checkName) {
        string check = message->getName();
        FileReader::tolower(check);
        if (completeMatch ? (check != lname) : (check.find(lname) == check.npos)) {
          continue;
        }
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
      if (since != 0 || until != 0) {
        if (message->getDstAddress() == SYN) {
          continue;
        }
        time_t lastchg = message->getLastChangeTime();
        if ((since != 0 && lastchg < since)
        || (until != 0 && lastchg >= until)) {
          continue;
        }
      }
      if (!onlyAvailable || message->isAvailable()) {
        ret.push_back(message);
      }
    }
  }

  return ret;
}

Message* MessageMap::find(MasterSymbolString& master, bool anyDestination,
  const bool withRead, const bool withWrite, const bool withPassive, const bool onlyAvailable) const {
  if (anyDestination && master.size() >= 5 && master[4] == 0 && master[2] == 0x07 && master[3] == 0x04) {
    return m_scanMessage;
  }
  uint64_t baseKey = Message::createKey(master,
      anyDestination || master[1] != BROADCAST ? m_maxIdLength : m_maxBroadcastIdLength, anyDestination);
  if (baseKey == INVALID_KEY) {
    return NULL;
  }
  size_t maxIdLength = Message::getKeyLength(baseKey);
  for (size_t idLength = maxIdLength; true; idLength--) {
    uint64_t key = baseKey;
    if (idLength == maxIdLength) {
      baseKey &= ~ID_LENGTH_AND_IDS_MASK;
    } else {
      key |= (uint64_t)idLength << (8 * 7 + 5);
      int exp = 3;
      for (size_t i = 0; i < idLength; i++) {
        key ^= (uint64_t)master.dataAt(i) << (8 * exp--);
        if (exp == 0) {
          exp = 3;
        }
      }
    }
    map<uint64_t, vector<Message*> >::const_iterator it;
    if (withPassive) {
      it = m_messagesByKey.find(key);
      if (it != m_messagesByKey.end()) {
        Message* message = getFirstAvailable(it->second, &master, onlyAvailable);
        if (message) {
          return message;
        }
      }
      if ((key & ID_SOURCE_MASK) != 0) {
        key &= ~ID_SOURCE_MASK;
        it = m_messagesByKey.find(key & ~ID_SOURCE_MASK);  // try again without specific source master
        if (it != m_messagesByKey.end()) {
          Message* message = getFirstAvailable(it->second, &master, onlyAvailable);
          if (message) {
            return message;
          }
        }
      }
    } else {
      key &= ~ID_SOURCE_MASK;
    }
    if (withRead) {
      it = m_messagesByKey.find(key | ID_SOURCE_ACTIVE_READ);  // try again with special value for active read
      if (it != m_messagesByKey.end()) {
        Message* message = getFirstAvailable(it->second, &master, onlyAvailable);
        if (message) {
          return message;
        }
      }
    }
    if (withWrite) {
      it = m_messagesByKey.find(key | ID_SOURCE_ACTIVE_WRITE);  // try again with special value for active write
      if (it != m_messagesByKey.end()) {
        Message* message = getFirstAvailable(it->second, &master, onlyAvailable);
        if (message) {
          return message;
        }
      }
    }
    if (idLength == 0) {
      break;
    }
  }

  return NULL;
}

void MessageMap::invalidateCache(Message* message) {
  if (message->m_data == DataFieldSet::getIdentFields()) {
    return;
  }
  message->m_lastUpdateTime = 0;
  string circuit = message->getCircuit();
  string name = message->getName();
  deque<Message*> messages = findAll(circuit, name, "*", true, true, true, true);
  for (deque<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
    Message* checkMessage = *it;
    if (checkMessage != message) {
      checkMessage->m_lastUpdateTime = 0;
    }
  }
}

void MessageMap::addPollMessage(Message* message, bool toFront) {
  if (message != NULL && message->getPollPriority() > 0) {
    message->m_lastPollTime = toFront ? 0 : m_pollMessages.size();
    m_pollMessages.push(message);
  }
}

bool MessageMap::decodeCircuit(const string circuit, ostringstream& output, OutputFormat outputFormat) const {
  auto it = m_circuitData.find(circuit);
  if (it == m_circuitData.end()) {
    return false;
  }
  if (outputFormat & OF_JSON) {
    output << "\"name\": \"" << it->second->getName() << "\"";
  } else {
    output << it->second->getName() << "=";
  }
  return it->second->appendAttributes(output, outputFormat);
}

void MessageMap::clear() {
  m_loadedFiles.clear();
  m_loadedFileInfos.clear();
  // clear poll messages
  while (!m_pollMessages.empty()) {
    m_pollMessages.top();
    m_pollMessages.pop();
  }
  // free message instances by name
  for (auto it : m_messagesByName) {
    vector<Message*> nameMessages = it.second;
    if (it.first[0] == FIELD_SEPARATOR) {  // avoid double free: instances stored multiple times have a special key
      continue;
    }
    for (Message* message : it.second) {
      map<uint64_t, vector<Message*> >::iterator keyIt = m_messagesByKey.find(message->getKey());
      if (keyIt != m_messagesByKey.end()) {
        vector<Message*>* keyMessages = &keyIt->second;
        if (!keyMessages->empty()) {
          for (vector<Message*>::iterator kit = keyMessages->begin(); kit != keyMessages->end(); kit++) {
            if (*kit == message) {
              keyMessages->erase(kit--);
            }
          }
        }
      }
      delete message;
    }
    it.second.clear();
  }
  // free remaining message instances by key
  for (map<uint64_t, vector<Message*> >::iterator it = m_messagesByKey.begin(); it != m_messagesByKey.end(); it++) {
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
  for (auto& it : m_circuitData) {
    delete it.second;
  }
  m_circuitData.clear();
  m_maxIdLength = m_maxBroadcastIdLength = 0;
  m_additionalScanMessages = false;
}

Message* MessageMap::getNextPoll() {
  if (m_pollMessages.empty()) {
    return NULL;
  }
  Message* ret = m_pollMessages.top();
  m_pollMessages.pop();
  ret->m_pollCount++;
  time(&(ret->m_lastPollTime));
  m_pollMessages.push(ret);  // re-insert at new position
  return ret;
}

void MessageMap::dump(ostream& output, bool withConditions) const {
  bool first = true;
  Message::dumpHeader(output, NULL);
  output << endl;
  for (auto it : m_messagesByName) {
    if (it.first[0] == '-') {  // skip instances stored multiple times (key starting with "-")
      continue;
    }
    if (m_addAll) {
      for (auto message : it.second) {
        if (!message) {
          continue;
        }
        if (first) {
          first = false;
        } else {
          output << endl;
        }
        message->dump(output, NULL, withConditions);
      }
    } else {
      Message* message = getFirstAvailable(it.second);
      if (!message) {
        continue;
      }
      if (first) {
        first = false;
      } else {
        output << endl;
      }
      message->dump(output, NULL, withConditions);
    }
  }
  if (!first) {
    output << endl;
  }
}

}  // namespace ebusd

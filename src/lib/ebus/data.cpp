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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/ebus/data.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <algorithm>

namespace ebusd {

using std::dec;
using std::hex;
using std::setw;

/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

size_t getDataFieldId(const string name) {
  if (name == "name" || name.find("field") != string::npos) {
    return DATAFIELD_NAME;
  }
  if (name.find("part") != string::npos) {
    return DATAFIELD_PART;
  }
  if (name.find("type") != string::npos) {
    return DATAFIELD_TYPE;
  }
  if (name.find("divisor") != string::npos || name.find("values") != string::npos) {
    return DATAFIELD_DIVISORVALUES;
  }
  if (name == "unit") {
    return DATAFIELD_UNIT;
  }
  if (name == "comment") {
    return DATAFIELD_COMMENT;
  }
  return UINT_MAX;
}

string getDataFieldName(const size_t fieldId) {
  switch (fieldId) {
  case DATAFIELD_NAME:
    return "name";
  case DATAFIELD_PART:
    return "part";
  case DATAFIELD_TYPE:
    return "type";
  case DATAFIELD_DIVISORVALUES:
    return "divisor/values";
  case DATAFIELD_UNIT:
    return "unit";
  case DATAFIELD_COMMENT:
    return "comment";
  default:
    return "";
  }
}


const string AttributedItem::pluck(map<string, string>& row, string key) {
  map<string, string>::iterator it = row.find(key);
  if (it == row.end()) {
    return "";
  }
  row.erase(it);
  return it->second;
}

void AttributedItem::dumpString(ostream& output, const string str, const bool prependFieldSeparator) {
  if (prependFieldSeparator) {
    output << FIELD_SEPARATOR;
  }
  if (str.find_first_of(FIELD_SEPARATOR) == string::npos) {
    output << str;
  } else {
    output << TEXT_SEPARATOR << str << TEXT_SEPARATOR;
  }
}

void AttributedItem::mergeAttributes(map<string, string>& attributes) const {
  for (auto& entry : m_attributes) {
    auto it = attributes.find(entry.first);
    if (it == attributes.end() || it->second.empty()) {
      attributes[entry.first] = entry.second;
    }
  }
}

void AttributedItem::dumpAttribute(ostream& output, const string name, const bool prependFieldSeparator) const {
  dumpString(output, getAttribute(name), prependFieldSeparator);
}

string AttributedItem::getAttribute(const string name) const {
  auto it = m_attributes.find(name);
  return it == m_attributes.end() ? "" : it->second;
}

void appendJson(ostringstream& output, const string name, const string value, bool asString = false) {
  bool plain;
  if (asString) {
    plain = false;
  } else {
    plain = value == "false" || value == "true";
    if (!plain) {
      const char* str = value.c_str();
      char* strEnd = NULL;
      double dvalue = strtod(str, &strEnd);
      plain = strEnd && *strEnd;
    }
  }
  output << ", \"" << name << "\": ";
  if (plain) {
    output << value;
  } else {
    output << '"' << value << '"';
  }
}

void AttributedItem::appendAttribute(ostringstream& output, OutputFormat outputFormat, const string name,
    const bool onlyIfNonEmpty, const string prefix, const string suffix) const {
  auto it = m_attributes.find(name);
  string value = it == m_attributes.end() ? "" : it->second;
  if (!onlyIfNonEmpty || !value.empty()) {
    if (outputFormat & OF_JSON) {
      appendJson(output, name, value, true);
    } else {
      output << " " << prefix << value << suffix;
    }
  }
}



string formatInt(size_t value) {
  ostringstream stream;
  stream << dec << static_cast<unsigned>(value);
  return stream.str();
}

result_t DataField::create(vector< map<string, string> >& rows, string& errorDescription,
    DataFieldTemplates* templates, const DataField*& returnField,
    const bool isWriteMessage,
    const bool isTemplate, const bool isBroadcastOrMasterDestination,
    const size_t maxFieldLength) {
  // template: name,[,part]basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  // std: name,part,basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  vector<const SingleDataField*> fields;
  string firstName;
  result_t result = RESULT_OK;
  if (rows.empty()) {
    errorDescription = "no fields";
    return RESULT_ERR_EOF;
  }
  size_t fieldIndex = -1;
  for (auto row : rows) {
    if (result != RESULT_OK) {
      break;
    }
    fieldIndex++;
    const string name = pluck(row, "name");
    PartType partType;
    bool hasPart = false;
    string part = pluck(row, "part");
    if (isTemplate) {
      partType = pt_any;
    } else {
      hasPart = !part.empty();
      if (hasPart) {
        FileReader::tolower(part);
      }
      if (isBroadcastOrMasterDestination
        || (isWriteMessage && !hasPart)
        || part == "m") {  // master data
        partType = pt_masterData;
      } else if ((!isWriteMessage && !hasPart)
        || part == "s") {  // slave data
        partType = pt_slaveData;
      } else {
        errorDescription = "part "+part+" in field "+formatInt(fieldIndex);
        result = hasPart ? RESULT_ERR_INVALID_ARG : RESULT_ERR_MISSING_ARG;
        break;
      }
    }
    if (fields.empty()) {
      firstName = name;
    }

    const string typeStr = pluck(row, "type");  // basetype[:len]|template[:name]
    if (typeStr.empty()) {
      errorDescription = "field type in field "+formatInt(fieldIndex);
      result = RESULT_ERR_MISSING_ARG;
      break;
    }

    string divisorStr = pluck(row, "divisor");
    string valuesStr = pluck(row, "values");
    if (divisorStr.empty() && valuesStr.empty()) {
      divisorStr = pluck(row, "divisor/values");  // [divisor|values]
      if (divisorStr.find('=') != string::npos) {
        valuesStr = divisorStr;
        divisorStr = "";
      }
    }
    int divisor = 0;
    if (!divisorStr.empty()) {
      divisor = parseSignedInt(divisorStr.c_str(), 10, -MAX_DIVISOR, MAX_DIVISOR, result);
      if (result != RESULT_OK) {
        errorDescription = "divisor "+divisorStr+" in field "+formatInt(fieldIndex);
      }
    }
    bool verifyValue = false;
    map<unsigned int, string> values;
    string constantValue;
    if (!valuesStr.empty()) {
      size_t equalPos = valuesStr.find('=');
      if (equalPos == string::npos) {
        errorDescription = "values "+valuesStr+" in field "+formatInt(fieldIndex);
        result = RESULT_ERR_INVALID_LIST;
      } else if (equalPos == 0 && valuesStr.length() > 1) {
        verifyValue = valuesStr[1] == '=';  // == forced verification of constant value
        if (verifyValue && valuesStr.length() == 1) {
          errorDescription = "values "+valuesStr+" in field "+formatInt(fieldIndex);
          result = RESULT_ERR_INVALID_LIST;
          break;
        }
        constantValue = valuesStr.substr(equalPos+(verifyValue?2:1));
      } else {
        string token;
        istringstream stream(valuesStr);
        while (getline(stream, token, VALUE_SEPARATOR)) {
          FileReader::trim(token);
          const char* str = token.c_str();
          char* strEnd = NULL;
          unsigned long id;
          if (strncasecmp(str, "0x", 2) == 0) {
            str += 2;
            id = strtoul(str, &strEnd, 16);  // hexadecimal
          } else {
            id = strtoul(str, &strEnd, 10);  // decimal
          }
          if (strEnd == NULL || strEnd == str || id > MAX_VALUE) {
            errorDescription = "value "+token+" in field "+formatInt(fieldIndex);
            result = RESULT_ERR_INVALID_LIST;
            break;
          }
          // remove blanks around '=' sign
          while (*strEnd == ' ') strEnd++;
          if (*strEnd != '=') {
            errorDescription = "value "+token+" in field "+formatInt(fieldIndex);
            result = RESULT_ERR_INVALID_LIST;
            break;
          }
          token = string(strEnd + 1);
          FileReader::trim(token);
          values[(unsigned int)id] = token;
        }
      }
      if (result != RESULT_OK) {
        break;
      }
    }

    bool firstType = true;
    string token;
    istringstream stream(typeStr);
    while (result == RESULT_OK && getline(stream, token, VALUE_SEPARATOR)) {
      bool lastType = stream.eof();
      FileReader::trim(token);
      const DataField* templ = templates->get(token);
      size_t pos = token.find(LENGTH_SEPARATOR);
      if (templ == NULL && pos != string::npos) {
        templ = templates->get(token.substr(0, pos));
      }
      if (templ == NULL) {  // basetype[:len]
        size_t length;
        string typeName;
        if (pos == string::npos) {
          length = 0;  // no length specified
          typeName = token;
        } else {
          if (pos+2 == token.length() && token[pos+1] == '*') {
            length = REMAIN_LEN;
          } else {
            length = (size_t)parseInt(token.substr(pos+1).c_str(), 10, 1, (unsigned int)maxFieldLength, result);
            if (result != RESULT_OK) {
              errorDescription = "field type "+token+" in field "+formatInt(fieldIndex);
              break;
            }
          }
          typeName = token.substr(0, pos);
        }
        transform(typeName.begin(), typeName.end(), typeName.begin(), ::toupper);
        const DataType* dataType = DataTypeList::getInstance()->get(typeName, length == REMAIN_LEN ? 0 : length);
        if (!dataType) {
          result = RESULT_ERR_NOTFOUND;
          errorDescription = "field type "+typeName+" in field "+formatInt(fieldIndex);
        } else {
          SingleDataField* add = NULL;
          result = SingleDataField::create(firstType ? name : "", row, dataType, partType, length, divisor, values,
            constantValue, verifyValue, add);
          if (add != NULL) {
            fields.push_back(add);
          } else {
            if (result == RESULT_OK) {
              errorDescription = "field type "+typeName+" in field "+formatInt(fieldIndex);
              result = RESULT_ERR_NOTFOUND;  // type not found
            } else {
              errorDescription = "create field in field "+formatInt(fieldIndex);
            }
          }
        }
      } else if (!constantValue.empty()) {
        errorDescription = "constant value "+constantValue+" in field "+formatInt(fieldIndex);
        result = RESULT_ERR_INVALID_ARG;  // invalid value list
      } else {  // template[:name]
        string fieldName;
        if (pos != string::npos) {  // replacement name specified
          fieldName = token.substr(pos+1);
        } else {
          fieldName = (firstType && lastType) ? name : "";
        }
        result = templ->derive(fieldName, row, partType, divisor, values, fields);
        if (result != RESULT_OK) {
          errorDescription = "derive field "+fieldName+" in field "+formatInt(fieldIndex);
        }
      }
      if (firstType && !lastType) {
        pluck(row, "comment");
        pluck(row, "unit");
      }
      firstType = false;
    }
  }

  if (result != RESULT_OK) {
    while (!fields.empty()) {  // cleanup already created fields
      delete fields.back();
      fields.pop_back();
    }
    return result;
  }

  if (fields.size() == 1) {
    returnField = fields[0];
  } else {
    returnField = new DataFieldSet(firstName, fields);
  }
  return RESULT_OK;
}

string DataField::getDayName(int day) {
  if (day < 0 || day > 6) {
    return "";
  }
  return dayNames[day];
}


result_t SingleDataField::create(const string name, const map<string, string>& attributes, const DataType* dataType,
    const PartType partType, const size_t length, int divisor, map<unsigned int, string> values,
    const string constantValue, const bool verifyValue, SingleDataField* &returnField) {
  size_t bitCount = dataType->getBitCount();
  size_t byteCount = (bitCount + 7) / 8;
  if (dataType->isAdjustableLength()) {
    // check length
    if ((bitCount % 8) != 0) {
      if (length == 0) {
        bitCount = 1;  // default bit count: 1 bit
      } else if (length <= bitCount) {
        bitCount = length;
      } else {
        return RESULT_ERR_OUT_OF_RANGE;  // invalid length
      }
      byteCount = (bitCount + 7) / 8;
    } else if (length == 0) {
      byteCount = 1;  // default byte count: 1 byte
    } else if (length <= byteCount || length == REMAIN_LEN) {
      byteCount = length;
    } else {
      return RESULT_ERR_OUT_OF_RANGE;  // invalid length
    }
  }
  if (!constantValue.empty()) {
    returnField = new ConstantDataField(name, attributes, dataType, partType, byteCount, constantValue, verifyValue);
    return RESULT_OK;
  }
  if (dataType->isNumeric()) {
    const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(dataType);
    if (values.empty() && numType->hasFlag(DAY)) {
      for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++) {
        values[numType->getMinValue() + i] = dayNames[i];
      }
    }
    result_t result = numType->derive(divisor, bitCount, numType);
    if (result != RESULT_OK) {
      return result;
    }
    if (values.empty()) {
      returnField = new SingleDataField(name, attributes, numType, partType, byteCount);
      return RESULT_OK;
    }
    if (values.begin()->first < numType->getMinValue() || values.rbegin()->first > numType->getMaxValue()) {
      return RESULT_ERR_OUT_OF_RANGE;
    }
    returnField = new ValueListDataField(name, attributes, numType, partType, byteCount, values);
    return RESULT_OK;
  }
  if (divisor != 0 || !values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot set divisor or values for string field
  }
  returnField = new SingleDataField(name, attributes, dataType, partType, byteCount);
  return RESULT_OK;
}

void SingleDataField::dump(ostream& output) const {
  output << setw(0) << dec;  // initialize formatting
  dumpString(output, m_name, false);
  output << FIELD_SEPARATOR;
  if (m_partType == pt_masterData) {
    output << "m";
  } else if (m_partType == pt_slaveData) {
    output << "s";
  }
  output << FIELD_SEPARATOR;
  m_dataType->dump(output, m_length);
  dumpAttribute(output, "unit");
  dumpAttribute(output, "comment");
}


result_t SingleDataField::read(const SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName, ssize_t fieldIndex) const {
  if (m_partType == pt_any) {
    return RESULT_ERR_INVALID_PART;
  }
  if ((data.isMaster() ? pt_masterData : pt_slaveData) != m_partType) {
    return RESULT_EMPTY;
  }
  bool remainder = m_length == REMAIN_LEN && m_dataType->isAdjustableLength();
  if (offset + (remainder?1:m_length) > data.getDataSize()) {
    return RESULT_ERR_INVALID_POS;
  }
  if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
    return RESULT_EMPTY;
  }
  result_t res = m_dataType->readRawValue(data, offset, m_length, output);
  return res;
}

result_t SingleDataField::read(const SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) const {
  if (m_partType == pt_any) {
    return RESULT_ERR_INVALID_PART;
  }
  if ((data.isMaster() ? pt_masterData : pt_slaveData) != m_partType) {
    return RESULT_OK;
  }
  bool remainder = m_length == REMAIN_LEN && m_dataType->isAdjustableLength();
  if (offset + (remainder?1:m_length) > data.getDataSize()) {
    return RESULT_ERR_INVALID_POS;
  }
  if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
    return RESULT_EMPTY;
  }
  bool shortFormat = outputFormat & OF_SHORT;
  if (outputFormat & OF_JSON) {
    if (leadingSeparator) {
      output << ",";
    }
    if (!shortFormat) {
      output << "\n    ";
    }
    if (outputIndex >= 0 || m_name.empty() || !(outputFormat & OF_NAMES)) {
      output << "\"" << static_cast<signed int>(outputIndex < 0 ? 0 : outputIndex) << "\":";
      if (!shortFormat) {
        output << " {\"name\": \"" << m_name << "\"" << ", \"value\": ";
      }
    } else {
      output << "\"" << m_name << "\":";
      if (!shortFormat) {
        output << " {\"value\": ";
      }
    }
  } else {
    if (leadingSeparator) {
      output << UI_FIELD_SEPARATOR;
    }
    if (outputFormat & OF_NAMES) {
      output << m_name << "=";
    }
  }

  result_t result = readSymbols(data, offset, output, outputFormat);
  if (result != RESULT_OK) {
    return result;
  }
  if (!shortFormat) {
    if ((outputFormat & OF_UNITS)) {
      appendAttribute(output, outputFormat, "unit");
    }
    if ((outputFormat & OF_COMMENTS)) {
      appendAttribute(output, outputFormat, "comment", true, "[", "]");
    }
    if (outputFormat & OF_ALL_ATTRS) {
      for (auto& entry : m_attributes) {
        if (!entry.second.empty() && entry.first != "unit" && entry.first != "comment") {
          if (outputFormat & OF_JSON) {
            appendJson(output, entry.first, entry.second);
          } else {
            output << " " << entry.first << "=" << entry.second;
          }
        }
      }
    }
  }
  if (!shortFormat && (outputFormat & OF_JSON)) {
    output << "}";
  }
  return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input, SymbolString& data,
    size_t offset, char separator, size_t* length) const {
  if (m_partType == pt_any) {
    return RESULT_ERR_INVALID_PART;
  }
  if ((data.isMaster() ? pt_masterData : pt_slaveData) != m_partType) {
    return RESULT_OK;
  }
  return writeSymbols(input, (const size_t)offset, data, length);
}

result_t SingleDataField::readSymbols(const SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const {
  return m_dataType->readSymbols(input, offset, m_length, output, outputFormat);
}

result_t SingleDataField::writeSymbols(istringstream& input,
    const size_t offset,
    SymbolString& output, size_t* usedLength) const {
  return m_dataType->writeSymbols(input, offset, m_length, output, usedLength);
}

const SingleDataField* SingleDataField::clone() const {
  return new SingleDataField(*this);
}

result_t SingleDataField::derive(string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  bool numeric = m_dataType->isNumeric();
  if (!numeric && (divisor != 0 || !values.empty())) {
    return RESULT_ERR_INVALID_ARG;  // cannot set divisor or values for non-numeric field
  }
  if (name.empty()) {
    name = m_name;
  }
  mergeAttributes(attributes);
  const DataType* dataType = m_dataType;
  if (numeric) {
    const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(dataType);
    result_t result = numType->derive(divisor, 0, numType);
    if (result != RESULT_OK) {
      return result;
    }
    dataType = numType;
  }
  if (values.empty()) {
    fields.push_back(new SingleDataField(name, attributes, dataType, partType, m_length));
  } else if (numeric) {
    fields.push_back(new ValueListDataField(name, attributes, reinterpret_cast<const NumberDataType*>(dataType),
      partType, m_length, values));
  } else {
    return RESULT_ERR_INVALID_ARG;
  }
  return RESULT_OK;
}

bool SingleDataField::hasField(const char* fieldName, bool numeric) const {
  bool numericType = m_dataType->isNumeric();
  return numeric == numericType && (fieldName == NULL || fieldName == m_name);
}

size_t SingleDataField::getLength(PartType partType, size_t maxLength) const {
  if (partType != m_partType) {
    return 0;
  }
  bool remainder = m_length == REMAIN_LEN && m_dataType->isAdjustableLength();
  return remainder ? maxLength : m_length;
}

bool SingleDataField::hasFullByteOffset(bool after) const {
  if (m_length > 1) {
    return true;
  }
  int16_t firstBit;
  if (m_dataType->isNumeric()) {
    const NumberDataType* num = reinterpret_cast<const NumberDataType*>(m_dataType);
    firstBit = num->getFirstBit();
  } else {
    firstBit = 0;
  }
  return (m_dataType->getBitCount() % 8) == 0
  || (after && firstBit + (m_dataType->getBitCount() % 8) >= 8);
}


const ValueListDataField* ValueListDataField::clone() const {
  return new ValueListDataField(*this);
}

result_t ValueListDataField::derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  string useName = name.empty() ? m_name : name;
  mergeAttributes(attributes);
  if (divisor != 0 && divisor != 1) {
    return RESULT_ERR_INVALID_ARG;  // cannot use divisor != 1 for value list field
  }
  if (!m_dataType->isNumeric()) {
    return RESULT_ERR_INVALID_ARG;
  }
  if (!values.empty()) {
    const NumberDataType* num = reinterpret_cast<const NumberDataType*>(m_dataType);
    if (values.begin()->first < num->getMinValue() || values.rbegin()->first > num->getMaxValue()) {
      return RESULT_ERR_INVALID_ARG;  // cannot use divisor != 1 for value list field
    }
  } else {
    values = m_values;
  }
  fields.push_back(new ValueListDataField(useName, attributes, reinterpret_cast<const NumberDataType*>(m_dataType),
    partType, m_length, values));
  return RESULT_OK;
}

void ValueListDataField::dump(ostream& output) const {
  output << setw(0) << dec;  // initialize formatting
  dumpString(output, m_name, false);
  output << FIELD_SEPARATOR;
  if (m_partType == pt_masterData) {
    output << "m";
  } else if (m_partType == pt_slaveData) {
    output << "s";
  }
  output << FIELD_SEPARATOR;
  if (!m_dataType->dump(output, m_length)) {  // no divisor appended
    bool first = true;
    for (auto it : m_values) {
      if (first) {
        first = false;
      } else {
        output << VALUE_SEPARATOR;
      }
      output << static_cast<unsigned>(it.first) << "=" << it.second;
    }
  }  // else: impossible since divisor is not allowed for ValueListDataField
  dumpAttribute(output, "unit");
  dumpAttribute(output, "comment");
}

result_t ValueListDataField::readSymbols(const SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const {
  unsigned int value = 0;

  result_t result = m_dataType->readRawValue(input, offset, m_length, value);
  if (result != RESULT_OK) {
    return result;
  }
  auto it = m_values.find(value);
  if (it == m_values.end() && value != m_dataType->getReplacement()) {
    // fall back to raw value in input
    output << setw(0) << dec << static_cast<unsigned>(value);
    return RESULT_OK;
  }
  if (it == m_values.end()) {
    if (outputFormat & OF_JSON) {
      output << "null";
    } else if (value == m_dataType->getReplacement()) {
      output << NULL_VALUE;
    }
  } else if (outputFormat & OF_NUMERIC) {
    output << setw(0) << dec << static_cast<unsigned>(value);
  } else if (outputFormat & OF_JSON) {
    output << '"' << it->second << '"';
  } else {
    output << it->second;
  }
  return RESULT_OK;
}

result_t ValueListDataField::writeSymbols(istringstream& input,
    const size_t offset,
    SymbolString& output, size_t* usedLength) const {
  const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(m_dataType);
  if (isIgnored()) {
    // replacement value
    return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength);
  }
  const char* str = input.str().c_str();

  for (auto it : m_values) {
    if (it.second.compare(str) == 0) {
      return numType->writeRawValue(it.first, offset, m_length, output, usedLength);
    }
  }
  if (strcasecmp(str, NULL_VALUE) == 0) {
    // replacement value
    return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength);
  }
  char* strEnd = NULL;  // fall back to raw value in input
  unsigned int value;
  value = (unsigned int)strtoul(str, &strEnd, 10);
  if (strEnd == NULL || strEnd == str || (*strEnd != 0 && *strEnd != '.')) {
    return RESULT_ERR_INVALID_NUM;  // invalid value
  }
  if (m_values.find(value) != m_values.end()) {
    return numType->writeRawValue(value, offset, m_length, output, usedLength);
  }
  return RESULT_ERR_NOTFOUND;  // value assignment not found
}


const ConstantDataField* ConstantDataField::clone() const {
  return new ConstantDataField(*this);
}

result_t ConstantDataField::derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  string useName = name.empty() ? m_name : name;
  for (auto entry : m_attributes) {  // merge with this attributes
    if (attributes[entry.first].empty()) {
      attributes[entry.first] = entry.second;
    }
  }
  if (divisor != 0) {
    return RESULT_ERR_INVALID_ARG;  // cannot use other than current divisor for constant value field
  }
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot use value list for constant value field
  }
  fields.push_back(new ConstantDataField(useName, attributes, m_dataType, partType, m_length, m_value, m_verify));
  return RESULT_OK;
}

void ConstantDataField::dump(ostream& output) const {
  output << setw(0) << dec;  // initialize formatting
  dumpString(output, m_name, false);
  output << FIELD_SEPARATOR;
  if (m_partType == pt_masterData) {
    output << "m";
  } else if (m_partType == pt_slaveData) {
    output << "s";
  }
  output << FIELD_SEPARATOR;
  if (!m_dataType->dump(output, m_length)) {  // no divisor appended
    output << (m_verify?"==":"=") << m_value;
  }  // else: impossible since divisor is not allowed for ConstantDataField
  dumpAttribute(output, "unit");
  dumpAttribute(output, "comment");
}

result_t ConstantDataField::readSymbols(const SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const {
  ostringstream coutput;
  result_t result = SingleDataField::readSymbols(input, offset, coutput, 0);
  if (result != RESULT_OK) {
    return result;
  }
  if (m_verify) {
    string value = coutput.str();
    FileReader::trim(value);
    if (value != m_value) {
      return RESULT_ERR_OUT_OF_RANGE;
    }
  }
  return RESULT_OK;
}

result_t ConstantDataField::writeSymbols(istringstream& input,
    const size_t offset,
    SymbolString& output, size_t* usedLength) const {
  istringstream cinput(m_value);
  return SingleDataField::writeSymbols(cinput, offset, output, usedLength);
}


DataFieldSet* DataFieldSet::s_identFields = NULL;

DataFieldSet* DataFieldSet::getIdentFields() {
  if (s_identFields == NULL) {
    const NumberDataType* uchDataType = reinterpret_cast<const NumberDataType*>(
        DataTypeList::getInstance()->get("UCH"));
    const StringDataType* stringDataType = reinterpret_cast<const StringDataType*>(
        DataTypeList::getInstance()->get("STR"));
    const NumberDataType* pinDataType = reinterpret_cast<const NumberDataType*>(
        DataTypeList::getInstance()->get("PIN"));
    map<unsigned int, string> manufacturers;
    manufacturers[0x06] = "Dungs";
    manufacturers[0x0f] = "FH Ostfalia";
    manufacturers[0x10] = "TEM";
    manufacturers[0x11] = "Lamberti";
    manufacturers[0x14] = "CEB";
    manufacturers[0x15] = "Landis-Staefa";
    manufacturers[0x16] = "FERRO";
    manufacturers[0x17] = "MONDIAL";
    manufacturers[0x18] = "Wikon";
    manufacturers[0x19] = "Wolf";
    manufacturers[0x20] = "RAWE";
    manufacturers[0x30] = "Satronic";
    manufacturers[0x40] = "ENCON";
    manufacturers[0x50] = "Kromschroeder";
    manufacturers[0x60] = "Eberle";
    manufacturers[0x65] = "EBV";
    manufacturers[0x75] = "Graesslin";
    manufacturers[0x85] = "ebm-papst";
    manufacturers[0x95] = "SIG";
    manufacturers[0xa5] = "Theben";
    manufacturers[0xa7] = "Thermowatt";
    manufacturers[0xb5] = "Vaillant";
    manufacturers[0xc0] = "Toby";
    manufacturers[0xc5] = "Weishaupt";
    manufacturers[0xfd] = "ebusd.eu";
    vector<const SingleDataField*> fields;
    map<string, string> attributes;
    fields.push_back(new ValueListDataField("MF", attributes, uchDataType, pt_slaveData, 1, manufacturers));
    fields.push_back(new SingleDataField("ID", attributes, stringDataType, pt_slaveData, 5));
    fields.push_back(new SingleDataField("SW", attributes, pinDataType, pt_slaveData, 2));
    fields.push_back(new SingleDataField("HW", attributes, pinDataType, pt_slaveData, 2));
    s_identFields = new DataFieldSet("ident", fields);
  }
  return s_identFields;
}

DataFieldSet::~DataFieldSet() {
  for (auto it : m_fields) {
    delete it;
  }
}

const DataFieldSet* DataFieldSet::clone() const {
  vector<const SingleDataField*> fields;
  for (auto it : m_fields) {
    fields.push_back(it->clone());
  }
  return new DataFieldSet(m_name, fields);
}

size_t DataFieldSet::getLength(PartType partType, size_t maxLength) const {
  size_t length = 0;
  bool previousFullByteOffset[] = { true, true, true, true };

  for (auto field : m_fields) {
    if (field->getPartType() == partType) {
      if (!previousFullByteOffset[partType] && !field->hasFullByteOffset(false)) {
        length--;
      }
      size_t fieldLength = field->getLength(partType, maxLength);
      if (fieldLength >= maxLength) {
        maxLength = 0;
      } else {
        maxLength = maxLength - fieldLength;
      }
      length = length + fieldLength;

      previousFullByteOffset[partType] = field->hasFullByteOffset(true);
    }
  }

  return length;
}

string DataFieldSet::getName(const ssize_t fieldIndex) const {
  if (fieldIndex < 0) {
    return m_name;
  }
  if ((size_t)fieldIndex >= m_fields.size()) {
    return "";
  }
  if (m_uniqueNames) {
    return m_fields[fieldIndex]->getName();
  }
  ostringstream ostream;
  ostream << static_cast<signed>(fieldIndex);
  return ostream.str();
}

result_t DataFieldSet::derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const {
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // value list not allowed in set derive
  }
  for (auto it : m_fields) {
    result_t result = it->derive("", attributes, partType, divisor, values, fields);
    if (result != RESULT_OK) {
      return result;
    }
    pluck(attributes, "comment");
    pluck(attributes, "unit");
  }

  return RESULT_OK;
}

bool DataFieldSet::hasField(const char* fieldName, bool numeric) const {
  for (auto field : m_fields) {
    if (field->hasField(fieldName, numeric) == 0) {
      return true;
    }
  }
  return false;
}

void DataFieldSet::dump(ostream& output) const {
  bool first = true;
  for (auto it : m_fields) {
    if (first) {
      first = false;
    } else {
      output << FIELD_SEPARATOR;
    }
    it->dump(output);
  }
}

result_t DataFieldSet::read(const SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName, ssize_t fieldIndex) const {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (auto field : m_fields) {
    if (field->getPartType() != partType) {
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false)) {
      offset--;
    }
    result_t result = field->read(data, offset, output, fieldName, fieldIndex);
    if (result < RESULT_OK) {
      return result;
    }
    offset += field->getLength(partType, data.getDataSize()-offset);
    previousFullByteOffset = field->hasFullByteOffset(true);
    if (result != RESULT_EMPTY) {
      found = true;
    }
    if (findFieldIndex && fieldName == field->getName()) {
      if (fieldIndex == 0) {
        if (!found) {
          return RESULT_ERR_NOTFOUND;
        }
        break;
      }
      fieldIndex--;
    }
  }

  if (!found) {
    return RESULT_EMPTY;
  }

  return RESULT_OK;
}

result_t DataFieldSet::read(const SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) const {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
  if (outputIndex < 0 && (!m_uniqueNames || ((outputFormat & OF_JSON) && !(outputFormat & OF_NAMES)))) {
    outputIndex = 0;
  }
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (auto field : m_fields) {
    if (field->getPartType() != partType) {
      if (outputIndex >= 0 && !field->isIgnored()) {
        outputIndex++;
      }
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false)) {
      offset--;
    }
    result_t result = field->read(data, offset, output, outputFormat, outputIndex, leadingSeparator,
        fieldName, fieldIndex);
    if (result < RESULT_OK) {
      return result;
    }
    offset += field->getLength(partType, data.getDataSize()-offset);
    previousFullByteOffset = field->hasFullByteOffset(true);
    if (result != RESULT_EMPTY) {
      found = true;
      leadingSeparator = true;
    }
    if (findFieldIndex && fieldName == field->getName()) {
      if (fieldIndex == 0) {
        if (!found) {
          return RESULT_ERR_NOTFOUND;
        }
        break;
      }
      fieldIndex--;
    }
    if (outputIndex >= 0 && !field->isIgnored()) {
      outputIndex++;
    }
  }

  if (!found) {
    return RESULT_EMPTY;
  }
  return RESULT_OK;
}

result_t DataFieldSet::write(istringstream& input, SymbolString& data,
    size_t offset, char separator, size_t* length) const {
  string token;
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  bool previousFullByteOffset = true;
  size_t baseOffset = offset;
  for (auto field : m_fields) {
    if (field->getPartType() != partType) {
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false)) {
      offset--;
    }
    result_t result;
    size_t fieldLength;
    if (m_fields.size() > 1) {
      if (field->isIgnored()) {
        token.clear();
      } else if (!getline(input, token, separator)) {
        token.clear();
      }
      istringstream single(token);
      result = field->write(single, data, offset, separator, &fieldLength);
    } else {
      result = field->write(input, data, offset, separator, &fieldLength);
    }
    if (result != RESULT_OK) {
      return result;
    }
    offset += fieldLength;
    previousFullByteOffset = field->hasFullByteOffset(true);
  }

  if (length != NULL) {
    *length = offset-baseOffset;
  }
  return RESULT_OK;
}


DataFieldTemplates::DataFieldTemplates(DataFieldTemplates& other)
    : MappedFileReader::MappedFileReader(false) {
  for (auto it : other.m_fieldsByName) {
    m_fieldsByName[it.first] = it.second->clone();
  }
}

void DataFieldTemplates::clear() {
  for (auto it : m_fieldsByName) {
    delete it.second;
    it.second = NULL;
  }
  m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(const DataField* field, string name, bool replace) {
  if (name.length() == 0) {
    name = field->getName();
  }
  auto it = m_fieldsByName.find(name);
  if (it != m_fieldsByName.end()) {
    if (!replace) {
      return RESULT_ERR_DUPLICATE_NAME;  // duplicate key
    }
    delete it->second;
    it->second = field;
    return RESULT_OK;
  }

  m_fieldsByName[name] = field;
  return RESULT_OK;
}

result_t DataFieldTemplates::getFieldMap(vector<string>& row, string& errorDescription) const {
  // name[:usename],basetype[:len]|template[:usename][,[divisor|values][,[unit][,[comment]]]]
  if (row.empty()) {
    // default map does not include separate field name
    row.push_back("name");
    for (size_t cnt = 0; cnt < 2; cnt++) {
      bool first = true;
      for (size_t fieldId = DATAFIELD_RANGE_MIN; fieldId <= DATAFIELD_RANGE_MAX; fieldId++) {
        if (cnt == 0 && fieldId == DATAFIELD_NAME) {
          continue;
        }
        if (fieldId == DATAFIELD_PART) {  // not included in default map
          continue;
        }
        // subsequent fields start with field name
        if (first) {
          first = false;
          row.push_back("*"+getDataFieldName(fieldId));
        } else {
          row.push_back(getDataFieldName(fieldId));
        }
      }
    }
    return RESULT_OK;
  }
  bool inDataFields = false;
  map<string, string> seen;
  for (auto &name : row) {
    string useName = name;
    tolower(useName);
    if (inDataFields) {
      size_t fieldId = getDataFieldId(useName);
      if (fieldId != UINT_MAX) {
        useName = getDataFieldName(fieldId);
        if (seen.find(useName) != seen.end()) {
          if (seen.find("type") == seen.end()) {
            errorDescription = "missing type";
            return RESULT_ERR_EOF;  // require at least type
          }
          seen.clear();
        }
      }
    } else {
      if (useName == "name" && seen.find("name") == seen.end()) {
        // keep first name for template
      } else {
        size_t fieldId = getDataFieldId(useName);
        if (fieldId != UINT_MAX) {
          useName = getDataFieldName(fieldId);
          if (seen.find("name") == seen.end()) {
            errorDescription = "missing name";
            return RESULT_ERR_EOF;  // require at least name
          }
          inDataFields = true;
          seen.clear();
        }
      }
      if (!inDataFields && seen.find(useName) != seen.end()) {
        errorDescription = "duplicate " + useName;
        return RESULT_ERR_INVALID_ARG;
      }
    }
    if (seen.empty() && inDataFields) {
      name = "*" + useName;  // data field repetition
    } else {
      name = useName;
    }
    seen[useName] = useName;
  }
  if (!inDataFields) {
    errorDescription = "missing fields";
    return RESULT_ERR_EOF;  // require at least one field
  }
  if (seen.find("type") == seen.end()) {
    errorDescription = "missing type";
    return RESULT_ERR_EOF;  // require at least type
  }
  return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
    string& errorDescription, const string filename, unsigned int lineNo) {
  string name = row["name"];  // required
  string firstFieldName;
  size_t colon = name.find(':');
  if (colon == string::npos) {
    firstFieldName = name;
  } else {
    firstFieldName = name.substr(colon+1);
    name = name.substr(0, colon);
  }
  const DataField* field = NULL;
  if (!subRows.empty() && subRows[0].find("name") == subRows[0].end()) {
    subRows[0]["name"] = firstFieldName;
  }
  result_t result = DataField::create(subRows, errorDescription, this, field, false, true, false);
  if (result != RESULT_OK) {
    return result;
  }
  result = add(field, name, true);
  if (result == RESULT_ERR_DUPLICATE_NAME) {
    errorDescription = name;
  }
  if (result != RESULT_OK) {
    delete field;
  }
  return result;
}

const DataField* DataFieldTemplates::get(const string name) const {
  auto ref = m_fieldsByName.find(name);
  if (ref == m_fieldsByName.end()) {
    return NULL;
  }
  return ref->second;
}

}  // namespace ebusd

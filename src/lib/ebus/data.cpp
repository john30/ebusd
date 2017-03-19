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
  };
}

result_t DataField::create(vector< map<string, string> >& rows, string& errorDescription,
    DataFieldTemplates* templates, DataField*& returnField,
    const bool isWriteMessage,
    const bool isTemplate, const bool isBroadcastOrMasterDestination,
    const size_t maxFieldLength) {
  // template: name,[,part]basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  // std: name,part,basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  vector<SingleDataField*> fields;
  string firstName;
  result_t result = RESULT_OK;
  if (rows.empty()) {
    return RESULT_ERR_EOF;
  }
  for (auto row : rows) {
    if (result != RESULT_OK) {
      break;
    }
    const string name = row["name"];
    PartType partType;
    int divisor = 0;
    bool hasPart = false;
    if (isTemplate) {
      partType = pt_any;
    } else {
      string part = row["part"];
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
        errorDescription = "part "+part+" in "+MappedFileReader::combineRow(row);
        result = hasPart ? RESULT_ERR_INVALID_ARG : RESULT_ERR_MISSING_ARG;
        break;
      }
    }
    string comment = row["comment"];
    if (comment == NULL_VALUE) {
      comment = "";
    }
    if (fields.empty()) {
      firstName = name;
    }

    const string typeStr = row["type"];  // basetype[:len]|template[:name]
    if (typeStr.empty()) {
      errorDescription = "field type in "+MappedFileReader::combineRow(row);
      result = RESULT_ERR_MISSING_ARG;
      break;
    }

    map<unsigned int, string> values;
    string constantValue;
    bool verifyValue = false;
    const string divisorStr = row["divisor/values"];  // [divisor|values]
    if (!divisorStr.empty()) {
      size_t equalPos = divisorStr.find('=');
      if (equalPos == string::npos) {
        divisor = parseSignedInt(divisorStr.c_str(), 10, -MAX_DIVISOR, MAX_DIVISOR, result);
        if (result != RESULT_OK) {
          errorDescription = "divisor "+divisorStr+" in "+MappedFileReader::combineRow(row);
        }
      } else if (equalPos == 0 && divisorStr.length() > 1) {
        verifyValue = divisorStr[1] == '=';  // == forced verification of constant value
        if (verifyValue && divisorStr.length() == 1) {
          errorDescription = "divisor "+divisorStr+" in "+MappedFileReader::combineRow(row);
          result = RESULT_ERR_INVALID_LIST;
          break;
        }
        constantValue = divisorStr.substr(equalPos+(verifyValue?2:1));
      } else {
        string token;
        istringstream stream(divisorStr);
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
            errorDescription = "value "+token+" in "+MappedFileReader::combineRow(row);
            result = RESULT_ERR_INVALID_LIST;
            break;
          }
          // remove blanks around '=' sign
          while (*strEnd == ' ') strEnd++;
          if (*strEnd != '=') {
            errorDescription = "value "+token+" in "+MappedFileReader::combineRow(row);
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

    string unit = row["unit"];
    if (unit == NULL_VALUE) {
      unit = "";
    }
    bool firstType = true;
    string token;
    istringstream stream(typeStr);
    while (result == RESULT_OK && getline(stream, token, VALUE_SEPARATOR)) {
      FileReader::trim(token);
      DataField* templ = templates->get(token);
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
              errorDescription = "field type "+token+" in "+MappedFileReader::combineRow(row);
              break;
            }
          }
          typeName = token.substr(0, pos);
        }
        transform(typeName.begin(), typeName.end(), typeName.begin(), ::toupper);
        SingleDataField* add = NULL;
        result = SingleDataField::create(typeName, length, firstType ? name : "", firstType ? comment : "",
            firstType ? unit : "", partType, divisor, values, constantValue, verifyValue, add);
        if (add != NULL) {
          fields.push_back(add);
        } else {
          if (result == RESULT_OK) {
            errorDescription = "field type " + typeName+" in "+MappedFileReader::combineRow(row);
            result = RESULT_ERR_NOTFOUND;  // type not found
          } else {
            errorDescription = "create field in "+MappedFileReader::combineRow(row);
          }
        }
      } else if (!constantValue.empty()) {
        errorDescription = "constant value "+constantValue+" in "+MappedFileReader::combineRow(row);
        result = RESULT_ERR_INVALID_ARG;  // invalid value list
      } else {  // template[:name]
        string fieldName;
        bool lastType = stream.eof();
        if (pos != string::npos) {  // replacement name specified
          fieldName = token.substr(pos+1);
        } else {
          fieldName = (firstType && lastType) ? name : "";
        }
        result = templ->derive(fieldName, firstType ? comment : "", firstType ? unit : "", partType, divisor, values,
            fields);
        if (result != RESULT_OK) {
          errorDescription = "derive field "+fieldName+" in "+MappedFileReader::combineRow(row);
        }
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
    returnField = new DataFieldSet(firstName, "", fields);
  }
  return RESULT_OK;
}

void DataField::dumpString(ostream& output, const string str, const bool prependFieldSeparator) {
  if (prependFieldSeparator) {
    output << FIELD_SEPARATOR;
  }
  if (str.find_first_of(FIELD_SEPARATOR) == string::npos) {
    output << str;
  } else {
    output << TEXT_SEPARATOR << str << TEXT_SEPARATOR;
  }
}

string DataField::getDayName(int day) {
  if (day < 0 || day > 6) {
    return "";
  }
  return dayNames[day];
}

result_t SingleDataField::create(const string id, const size_t length,
  const string name, const string comment, const string unit,
  const PartType partType, int divisor, map<unsigned int, string> values,
  const string constantValue, const bool verifyValue, SingleDataField* &returnField) {
  DataType* dataType = DataTypeList::getInstance()->get(id, length == REMAIN_LEN ? 0 : length);
  if (!dataType) {
    return RESULT_ERR_NOTFOUND;
  }
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
    returnField = new ConstantDataField(name, comment, unit, dataType, partType, byteCount, constantValue, verifyValue);
    return RESULT_OK;
  }
  if (dataType->isNumeric()) {
    NumberDataType* numType = reinterpret_cast<NumberDataType*>(dataType);
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
      returnField = new SingleDataField(name, comment, unit, numType, partType, byteCount);
      return RESULT_OK;
    }
    if (values.begin()->first < numType->getMinValue() || values.rbegin()->first > numType->getMaxValue()) {
      return RESULT_ERR_OUT_OF_RANGE;
    }
    returnField = new ValueListDataField(name, comment, unit, numType, partType, byteCount, values);
    return RESULT_OK;
  }
  if (divisor != 0 || !values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot set divisor or values for string field
  }
  returnField = new SingleDataField(name, comment, unit, dataType, partType, byteCount);
  return RESULT_OK;
}

void SingleDataField::dump(ostream& output) {
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
  dumpString(output, m_unit);
  dumpString(output, m_comment);
}


result_t SingleDataField::read(SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName, ssize_t fieldIndex) {
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
  return m_dataType->readRawValue(data, offset, m_length, output);
}

result_t SingleDataField::read(SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) {
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
  if (!shortFormat && (outputFormat & OF_UNITS) && m_unit.length() > 0) {
    if (outputFormat & OF_JSON) {
      output << ", \"unit\": \"" << m_unit << '"';
    } else {
      output << " " << m_unit;
    }
  }
  if (!shortFormat && (outputFormat & OF_COMMENTS) && m_comment.length() > 0) {
    if (outputFormat & OF_JSON) {
      output << ", \"comment\": \"" << m_comment << '"';
    } else {
      output << " [" << m_comment << "]";
    }
  }
  if (!shortFormat && (outputFormat & OF_JSON)) {
    output << "}";
  }
  return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input, SymbolString& data,
    size_t offset, char separator, size_t* length) {
  if (m_partType == pt_any) {
    return RESULT_ERR_INVALID_PART;
  }
  if ((data.isMaster() ? pt_masterData : pt_slaveData) != m_partType) {
    return RESULT_OK;
  }
  return writeSymbols(input, (const size_t)offset, data, length);
}

result_t SingleDataField::readSymbols(SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) {
  return m_dataType->readSymbols(input, offset, m_length, output, outputFormat);
}

result_t SingleDataField::writeSymbols(istringstream& input,
  const size_t offset,
  SymbolString& output, size_t* usedLength) {
  return m_dataType->writeSymbols(input, offset, m_length, output, usedLength);
}

SingleDataField* SingleDataField::clone() {
  return new SingleDataField(*this);
}

result_t SingleDataField::derive(string name, string comment,
    string unit, const PartType partType,
    int divisor, map<unsigned int, string> values,
    vector<SingleDataField*>& fields) {
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
  if (comment.empty()) {
    comment = m_comment;
  }
  if (unit.empty()) {
    unit = m_unit;
  }
  DataType* dataType = m_dataType;
  if (numeric) {
    NumberDataType* numType = reinterpret_cast<NumberDataType*>(dataType);
    result_t result = numType->derive(divisor, 0, numType);
    if (result != RESULT_OK) {
      return result;
    }
    dataType = numType;
  }
  if (values.empty()) {
    fields.push_back(new SingleDataField(name, comment, unit, dataType, partType, m_length));
  } else if (numeric) {
    fields.push_back(new ValueListDataField(name, comment, unit, reinterpret_cast<NumberDataType*>(dataType),
        partType, m_length, values));
  } else {
    return RESULT_ERR_INVALID_ARG;
  }
  return RESULT_OK;
}

bool SingleDataField::hasField(const char* fieldName, bool numeric) {
  bool numericType = m_dataType->isNumeric();
  return numeric == numericType && (fieldName == NULL || fieldName == m_name);
}

size_t SingleDataField::getLength(PartType partType, size_t maxLength) {
  if (partType != m_partType) {
    return 0;
  }
  bool remainder = m_length == REMAIN_LEN && m_dataType->isAdjustableLength();
  return remainder ? maxLength : m_length;
}

bool SingleDataField::hasFullByteOffset(bool after) {
  if (m_length > 1) {
    return true;
  }
  int16_t firstBit;
  if (m_dataType->isNumeric()) {
    NumberDataType* num = reinterpret_cast<NumberDataType*>(m_dataType);
    firstBit = num->getFirstBit();
  } else {
    firstBit = 0;
  }
  return (m_dataType->getBitCount() % 8) == 0
  || (after && firstBit + (m_dataType->getBitCount() % 8) >= 8);
}


ValueListDataField* ValueListDataField::clone() {
  return new ValueListDataField(*this);
}

result_t ValueListDataField::derive(string name, string comment,
    string unit, const PartType partType,
    int divisor, map<unsigned int, string> values,
    vector<SingleDataField*>& fields) {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  if (name.empty()) {
    name = m_name;
  }
  if (comment.empty()) {
    comment = m_comment;
  }
  if (unit.empty()) {
    unit = m_unit;
  }
  if (divisor != 0 && divisor != 1) {
    return RESULT_ERR_INVALID_ARG;  // cannot use divisor != 1 for value list field
  }
  if (!m_dataType->isNumeric()) {
    return RESULT_ERR_INVALID_ARG;
  }
  if (!values.empty()) {
    NumberDataType* num = reinterpret_cast<NumberDataType*>(m_dataType);
    if (values.begin()->first < num->getMinValue() || values.rbegin()->first > num->getMaxValue()) {
      return RESULT_ERR_INVALID_ARG;  // cannot use divisor != 1 for value list field
    }
  } else {
    values = m_values;
  }
  fields.push_back(new ValueListDataField(name, comment, unit, reinterpret_cast<NumberDataType*>(m_dataType),
      partType, m_length, values));
  return RESULT_OK;
}

void ValueListDataField::dump(ostream& output) {
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
    for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++) {
      if (it != m_values.begin()) {
        output << VALUE_SEPARATOR;
      }
      output << static_cast<unsigned>(it->first) << "=" << it->second;
    }
  }  // else: impossible since divisor is not allowed for ValueListDataField
  dumpString(output, m_unit);
  dumpString(output, m_comment);
}

result_t ValueListDataField::readSymbols(SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) {
  unsigned int value = 0;

  result_t result = m_dataType->readRawValue(input, offset, m_length, value);
  if (result != RESULT_OK) {
    return result;
  }
  map<unsigned int, string>::iterator it = m_values.find(value);
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
    SymbolString& output, size_t* usedLength) {
  NumberDataType* numType = reinterpret_cast<NumberDataType*>(m_dataType);
  if (isIgnored()) {
    // replacement value
    return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength);
  }
  const char* str = input.str().c_str();

  for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++) {
    if (it->second.compare(str) == 0) {
      return numType->writeRawValue(it->first, offset, m_length, output, usedLength);
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


ConstantDataField* ConstantDataField::clone() {
  return new ConstantDataField(*this);
}

result_t ConstantDataField::derive(string name, string comment,
    string unit, const PartType partType,
    int divisor, map<unsigned int, string> values,
    vector<SingleDataField*>& fields) {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  if (name.empty()) {
    name = m_name;
  }
  if (comment.empty()) {
    comment = m_comment;
  }
  if (unit.empty()) {
    unit = m_unit;
  }
  if (divisor != 0) {
    return RESULT_ERR_INVALID_ARG;  // cannot use other than current divisor for constant value field
  }
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot use value list for constant value field
  }
  fields.push_back(new ConstantDataField(name, comment, unit, m_dataType, partType, m_length, m_value, m_verify));
  return RESULT_OK;
}

void ConstantDataField::dump(ostream& output) {
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
  dumpString(output, m_unit);
  dumpString(output, m_comment);
}

result_t ConstantDataField::readSymbols(SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) {
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
    SymbolString& output, size_t* usedLength) {
  istringstream cinput(m_value);
  return SingleDataField::writeSymbols(cinput, offset, output, usedLength);
}


DataFieldSet* DataFieldSet::s_identFields = NULL;

DataFieldSet* DataFieldSet::getIdentFields() {
  if (s_identFields == NULL) {
    NumberDataType* uchDataType = reinterpret_cast<NumberDataType*>(DataTypeList::getInstance()->get("UCH"));
    StringDataType* stringDataType = reinterpret_cast<StringDataType*>(DataTypeList::getInstance()->get("STR"));
    NumberDataType* pinDataType = reinterpret_cast<NumberDataType*>(DataTypeList::getInstance()->get("PIN"));
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
    vector<SingleDataField*> fields;
    fields.push_back(new ValueListDataField("MF", "", "", uchDataType, pt_slaveData, 1, manufacturers));
    fields.push_back(new SingleDataField("ID", "", "", stringDataType, pt_slaveData, 5));
    fields.push_back(new SingleDataField("SW", "", "", pinDataType, pt_slaveData, 2));
    fields.push_back(new SingleDataField("HW", "", "", pinDataType, pt_slaveData, 2));
    s_identFields = new DataFieldSet("ident", "", fields);
  }
  return s_identFields;
}

DataFieldSet::~DataFieldSet() {
  while (!m_fields.empty()) {
    delete m_fields.back();
    m_fields.pop_back();
  }
}

DataFieldSet* DataFieldSet::clone() {
  vector<SingleDataField*> fields;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    fields.push_back((*it)->clone());
  }
  return new DataFieldSet(m_name, m_comment, fields);
}

size_t DataFieldSet::getLength(PartType partType, size_t maxLength) {
  size_t length = 0;
  bool previousFullByteOffset[] = { true, true, true, true };

  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    SingleDataField* field = *it;
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

string DataFieldSet::getName(ssize_t fieldIndex) {
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

result_t DataFieldSet::derive(string name, string comment,
    string unit, const PartType partType,
    int divisor, map<unsigned int, string> values,
    vector<SingleDataField*>& fields) {
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // value list not allowed in set derive
  }
  bool first = true;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    result_t result = (*it)->derive("", first?comment:"", first?unit:"", partType, divisor, values, fields);
    if (result != RESULT_OK) {
      return result;
    }
    first = false;
  }

  return RESULT_OK;
}

bool DataFieldSet::hasField(const char* fieldName, bool numeric) {
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    SingleDataField* field = *it;
    if (field->hasField(fieldName, numeric) == 0) {
      return true;
    }
  }
  return false;
}

void DataFieldSet::dump(ostream& output) {
  bool first = true;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    if (first) {
      first = false;
    } else {
      output << FIELD_SEPARATOR;
    }
    (*it)->dump(output);
  }
}

result_t DataFieldSet::read(SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName, ssize_t fieldIndex) {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    SingleDataField* field = *it;
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

result_t DataFieldSet::read(SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex) {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
  if (outputIndex < 0 && (!m_uniqueNames || ((outputFormat & OF_JSON) && !(outputFormat & OF_NAMES)))) {
    outputIndex = 0;
  }
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    SingleDataField* field = *it;
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
  if (!(outputFormat & OF_SHORT) && (outputFormat & OF_COMMENTS) && m_comment.length() > 0) {
    if (outputFormat & OF_JSON) {
      output << ",\"comment\": \"" << m_comment << '"';
    } else {
      output << " [" << m_comment << "]";
    }
  }

  return RESULT_OK;
}

result_t DataFieldSet::write(istringstream& input, SymbolString& data,
    size_t offset, char separator, size_t* length) {
  string token;
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  bool previousFullByteOffset = true;
  size_t baseOffset = offset;
  for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
    SingleDataField* field = *it;
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
      result = (*it)->write(single, data, offset, separator, &fieldLength);
    } else {
      result = (*it)->write(input, data, offset, separator, &fieldLength);
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
  for (map<string, DataField*>::iterator it = other.m_fieldsByName.begin(); it != other.m_fieldsByName.end(); it++) {
    m_fieldsByName[it->first] = it->second->clone();
  }
}

void DataFieldTemplates::clear() {
  for (map<string, DataField*>::iterator it = m_fieldsByName.begin(); it != m_fieldsByName.end(); it++) {
    delete it->second;
    it->second = NULL;
  }
  m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(DataField* field, string name, bool replace) {
  if (name.length() == 0) {
    name = field->getName();
  }
  map<string, DataField*>::iterator it = m_fieldsByName.find(name);
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

result_t DataFieldTemplates::getFieldMap(vector<string>& row, string& errorDescription) {
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
    tolower(name);
    size_t fieldId;
    if (inDataFields) {
      fieldId = getDataFieldId(name);
      if (fieldId == UINT_MAX) {
        errorDescription = "unknown field " + name;
        return RESULT_ERR_INVALID_ARG;
      }
      if (seen.find(name) != seen.end()) {
        if (seen.find("type") == seen.end()) {
          return RESULT_ERR_EOF;  // require at least type
        }
        seen.clear();
        name = "*" + getDataFieldName(fieldId);  // data field repetition
      } else {
        name = getDataFieldName(fieldId);
      }
    } else if (name == "name") {
      if (seen.find(name) != seen.end()) {
        errorDescription = "duplicate field " + name;
        return RESULT_ERR_INVALID_ARG;
      }
      name = "name";
    } else {
      fieldId = getDataFieldId(name);
      if (fieldId == UINT_MAX) {
        errorDescription = "unknown field " + name;
        return RESULT_ERR_INVALID_ARG;
      }
      if (seen.find("name") == seen.end()) {
        return RESULT_ERR_EOF;  // require at least name
      }
      inDataFields = true;
      seen.clear();
      name = "*" + getDataFieldName(fieldId);
    }
    seen[name] = name;
  }
  if (!inDataFields) {
    return RESULT_ERR_EOF;  // require at least one field
  }
  if (seen.find("name") == seen.end() || seen.find("type") == seen.end()) {
    return RESULT_ERR_EOF;  // require at least name and type
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
  DataField* field = NULL;
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

DataField* DataFieldTemplates::get(const string name) {
  map<string, DataField*>::const_iterator ref = m_fieldsByName.find(name);
  if (ref == m_fieldsByName.end()) {
    return NULL;
  }
  return ref->second;
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2021 John Baier <ebusd@ebusd.eu>
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

/** the default field map for field templates. */
static const char* defaultTemplateFieldMap[] = {
    "name", "*type", "divisor/values", "unit", "comment",
    "*name", "type", "divisor/values", "unit", "comment",
};

/** the default field map for fields only. */
static const char* defaultFieldsFieldMap[] = {
    "*type", "divisor/values", "unit", "comment",
};


string AttributedItem::formatInt(size_t value) {
  ostringstream stream;
  stream << dec << static_cast<unsigned>(value);
  return stream.str();
}

string AttributedItem::pluck(const string& key, map<string, string>* row) {
  const auto it = row->find(key);
  if (it == row->end()) {
    return "";
  }
  const string ret = it->second;
  row->erase(it);
  return ret;
}

void AttributedItem::dumpString(bool prependFieldSeparator, const string& str, ostream* output) {
  if (prependFieldSeparator) {
    *output << FIELD_SEPARATOR;
  }
  if (str.find_first_of(FIELD_SEPARATOR) == string::npos) {
    *output << str;
  } else {
    *output << TEXT_SEPARATOR << str << TEXT_SEPARATOR;
  }
}

void AttributedItem::appendJson(bool prependFieldSeparator, const string& name, const string& value,
    bool forceString, ostream* output) {
  bool plain = !forceString && !value.empty();
  if (plain) {
    plain = value == "false" || value == "true";
    if (!plain) {
      const char* str = value.c_str();
      char* strEnd = nullptr;
      strtod(str, &strEnd);
      plain = strEnd && !*strEnd;
    }
  }
  if (prependFieldSeparator) {
    *output << FIELD_SEPARATOR;
  }
  *output << " \"" << name << "\": ";
  if (plain) {
    *output << value;
  } else {
    *output << '"';
    if (value.find_first_of('"') == string::npos) {  // check for nested '"'
      *output << value;
    } else {
      string replaced = value;
      replace(replaced.begin(), replaced.end(), '"', '\'');
      *output << replaced;
    }
    *output << '"';
  }
}

void AttributedItem::mergeAttributes(map<string, string>* attributes) const {
  for (const auto& entry : m_attributes) {
    const auto it = attributes->find(entry.first);
    if (it == attributes->end() || it->second.empty()) {
      (*attributes)[entry.first] = entry.second;
    }
  }
}

void AttributedItem::dumpAttribute(bool prependFieldSeparator, bool asJson, const string& name, ostream* output)
    const {
  if (asJson) {
    appendJson(prependFieldSeparator, name, getAttribute(name), false, output);
  } else {
    dumpString(prependFieldSeparator, getAttribute(name), output);
  }
}

bool AttributedItem::appendAttribute(OutputFormat outputFormat, const string& name, bool onlyIfNonEmpty,
    const string& prefix, const string& suffix, ostream* output) const {
  const auto it = m_attributes.find(name);
  string value = it == m_attributes.end() ? "" : it->second;
  if (onlyIfNonEmpty && value.empty()) {
    return false;
  }
  if (outputFormat & OF_JSON) {
    appendJson(true, name, value, false, output);
  } else {
    *output << " " << prefix << value << suffix;
  }
  return true;
}

bool AttributedItem::appendAttributes(OutputFormat outputFormat, ostream* output) const {
  bool ret = false;
  if ((outputFormat & OF_UNITS)) {
    ret = appendAttribute(outputFormat, "unit", true, "", "", output) || ret;
  }
  if ((outputFormat & OF_COMMENTS)) {
    ret = appendAttribute(outputFormat, "comment", true, "[", "]", output) || ret;
  }
  if (outputFormat & OF_ALL_ATTRS) {
    for (const auto entry : m_attributes) {
      ret = true;
      if (!entry.second.empty() && entry.first != "unit" && entry.first != "comment") {
        const string& key = entry.first;
        if (outputFormat & OF_JSON) {
          if (key == "zz" || key == "qq") {
            result_t result = RESULT_EMPTY;
            size_t addr = parseInt(entry.second.c_str(), 16, 0, 255, &result);
            if (result == RESULT_OK) {
              *output << FIELD_SEPARATOR << " \"" << key << "\": " << addr;
              continue;
            }
          }
          appendJson(true, key, entry.second, false, output);
        } else {
          *output << " " << key << "=" << entry.second;
        }
      }
    }
  }
  return ret;
}

string AttributedItem::getAttribute(const string& name) const {
  const auto it = m_attributes.find(name);
  return it == m_attributes.end() ? "" : it->second;
}


result_t DataField::create(bool isWriteMessage, bool isTemplate, bool isBroadcastOrMasterDestination,
    size_t maxFieldLength, const DataFieldTemplates* templates, vector< map<string, string> >* rows,
    string* errorDescription, const DataField** returnField) {
  // template: name[,part]basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  // std: name[,part],basetype[:len]|template[:name][,[divisor|values][,[unit][,[comment]]]]
  vector<const SingleDataField*> fields;
  string firstName;
  result_t result = RESULT_OK;
  if (rows->empty()) {
    *errorDescription = "no fields";
    return RESULT_ERR_EOF;
  }
  size_t fieldIndex = 0;
  for (auto& row : *rows) {
    if (result != RESULT_OK) {
      break;
    }
    const string name = pluck("name", &row);
    PartType partType;
    bool hasPart = false;
    string part = pluck("part", &row);
    if (isTemplate) {
      partType = pt_any;
    } else {
      hasPart = !part.empty();
      if (hasPart) {
        FileReader::tolower(&part);
      }
      if (isBroadcastOrMasterDestination
        || (isWriteMessage && !hasPart)
        || part == "m") {  // master data
        partType = pt_masterData;
      } else if ((!isWriteMessage && !hasPart)
        || part == "s") {  // slave data
        partType = pt_slaveData;
      } else {
        *errorDescription = "part "+part+" in field "+formatInt(fieldIndex);
        result = hasPart ? RESULT_ERR_INVALID_ARG : RESULT_ERR_MISSING_ARG;
        break;
      }
    }
    if (fields.empty()) {
      firstName = name;
    }

    const string typeStr = pluck("type", &row);  // basetype[:len]|template[:name]
    if (typeStr.empty()) {
      *errorDescription = "field type in field "+formatInt(fieldIndex);
      result = RESULT_ERR_MISSING_ARG;
      break;
    }

    string divisorStr = pluck("divisor", &row);
    string valuesStr = pluck("values", &row);
    if (divisorStr.empty() && valuesStr.empty()) {
      divisorStr = pluck("divisor/values", &row);  // [divisor|values]
      if (divisorStr.find('=') != string::npos) {
        valuesStr = divisorStr;
        divisorStr = "";
      }
    }
    int divisor = 0;
    if (!divisorStr.empty()) {
      divisor = parseSignedInt(divisorStr.c_str(), 10, -MAX_DIVISOR, MAX_DIVISOR, &result);
      if (result != RESULT_OK) {
        *errorDescription = "divisor "+divisorStr+" in field "+formatInt(fieldIndex);
      }
    }
    bool verifyValue = false;
    map<unsigned int, string> values;
    string constantValue;
    if (!valuesStr.empty()) {
      size_t equalPos = valuesStr.find('=');
      if (equalPos == string::npos) {
        *errorDescription = "values "+valuesStr+" in field "+formatInt(fieldIndex);
        result = RESULT_ERR_INVALID_LIST;
      } else if (equalPos == 0 && valuesStr.length() > 1) {
        verifyValue = valuesStr[1] == '=';  // == forced verification of constant value
        if (verifyValue && valuesStr.length() == 1) {
          *errorDescription = "values "+valuesStr+" in field "+formatInt(fieldIndex);
          result = RESULT_ERR_INVALID_LIST;
          break;
        }
        constantValue = valuesStr.substr(equalPos+(verifyValue?2:1));
      } else {
        string token;
        istringstream stream(valuesStr);
        while (getline(stream, token, VALUE_SEPARATOR)) {
          FileReader::trim(&token);
          const char* str = token.c_str();
          char* strEnd = nullptr;
          unsigned long id;
          if (strncasecmp(str, "0x", 2) == 0) {
            str += 2;
            id = strtoul(str, &strEnd, 16);  // hexadecimal
          } else {
            id = strtoul(str, &strEnd, 10);  // decimal
          }
          if (strEnd == nullptr || strEnd == str || id > MAX_VALUE) {
            *errorDescription = "value "+token+" in field "+formatInt(fieldIndex);
            result = RESULT_ERR_INVALID_LIST;
            break;
          }
          // remove blanks around '=' sign
          while (*strEnd == ' ') strEnd++;
          if (*strEnd != '=') {
            *errorDescription = "value "+token+" in field "+formatInt(fieldIndex);
            result = RESULT_ERR_INVALID_LIST;
            break;
          }
          token = string(strEnd + 1);
          FileReader::trim(&token);
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
      FileReader::trim(&token);
      const DataField* templ = templates->get(token);
      size_t pos = token.find(LENGTH_SEPARATOR);
      if (templ == nullptr && pos != string::npos) {
        templ = templates->get(token.substr(0, pos));
      }
      if (templ == nullptr) {  // basetype[:len]
        size_t length;
        string typeName;
        if (pos == string::npos) {
          length = 0;  // no length specified
          typeName = token;
        } else {
          if (pos+2 == token.length() && token[pos+1] == '*') {
            length = REMAIN_LEN;
          } else {
            length = (size_t)parseInt(token.substr(pos+1).c_str(), 10, 1, (unsigned int)maxFieldLength, &result);
            if (result != RESULT_OK) {
              *errorDescription = "field type "+token+" in field "+formatInt(fieldIndex);
              break;
            }
          }
          typeName = token.substr(0, pos);
        }
        transform(typeName.begin(), typeName.end(), typeName.begin(), ::toupper);
        const DataType* dataType = DataTypeList::getInstance()->get(typeName, length == REMAIN_LEN ? 0 : length);
        if (!dataType) {
          result = RESULT_ERR_NOTFOUND;
          *errorDescription = "field type "+typeName+" in field "+formatInt(fieldIndex);
        } else {
          SingleDataField* add = nullptr;
          result = SingleDataField::create(firstType ? name : "", row, dataType, partType, length, divisor,
            constantValue, verifyValue, &values, &add);
          if (add != nullptr) {
            fields.push_back(add);
          } else {
            if (result == RESULT_OK) {
              *errorDescription = "field type "+typeName+" in field "+formatInt(fieldIndex);
              result = RESULT_ERR_NOTFOUND;  // type not found
            } else {
              *errorDescription = "create field in field "+formatInt(fieldIndex);
            }
          }
        }
      } else if (!constantValue.empty()) {
        *errorDescription = "constant value "+constantValue+" in field "+formatInt(fieldIndex);
        result = RESULT_ERR_INVALID_ARG;  // invalid value list
      } else {  // template[:name]
        string fieldName;
        if (pos != string::npos) {  // replacement name specified
          fieldName = token.substr(pos+1);
        } else {
          fieldName = (firstType && lastType) ? name : "";
        }
        if (lastType) {
          result = templ->derive(fieldName, partType, divisor, values, &row, &fields);
        } else {
          map<string, string> attrs = row;  // don't let DataField::derive() consume the row
          result = templ->derive(fieldName, partType, divisor, values, &attrs, &fields);
        }
        if (result != RESULT_OK) {
          *errorDescription = "derive field "+fieldName+" in field "+formatInt(fieldIndex);
        }
      }
      if (firstType && !lastType) {
        row.erase("comment");
        row.erase("unit");
      }
      firstType = false;
    }
    fieldIndex++;
  }

  if (result != RESULT_OK) {
    while (!fields.empty()) {  // cleanup already created fields
      delete fields.back();
      fields.pop_back();
    }
    return result;
  }

  if (fields.size() == 1) {
    *returnField = fields[0];
  } else {
    *returnField = new DataFieldSet(firstName, fields);
  }
  return RESULT_OK;
}

const char* DataField::getDayName(int day) {
  if (day < 0 || day > 6) {
    return "";
  }
  return dayNames[day];
}


result_t SingleDataField::create(const string& name, const map<string, string>& attributes, const DataType* dataType,
    PartType partType, size_t length, int divisor, const string& constantValue,
    bool verifyValue, map<unsigned int, string>* values, SingleDataField** returnField) {
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
    *returnField = new ConstantDataField(name, attributes, dataType, partType, byteCount, constantValue, verifyValue);
    return RESULT_OK;
  }
  if (dataType->isNumeric()) {
    const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(dataType);
    if (values->empty() && numType->hasFlag(DAY)) {
      for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++) {
        (*values)[numType->getMinValue() + i] = dayNames[i];
      }
    }
    result_t result = numType->derive(divisor, bitCount, &numType);
    if (result != RESULT_OK) {
      return result;
    }
    if (values->empty()) {
      *returnField = new SingleDataField(name, attributes, numType, partType, byteCount);
      return RESULT_OK;
    }
    if (values->begin()->first < numType->getMinValue() || values->rbegin()->first > numType->getMaxValue()) {
      return RESULT_ERR_OUT_OF_RANGE;
    }
    *returnField = new ValueListDataField(name, attributes, numType, partType, byteCount, *values);
    return RESULT_OK;
  }
  if (divisor != 0 || !values->empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot set divisor or values for string field
  }
  *returnField = new SingleDataField(name, attributes, dataType, partType, byteCount);
  return RESULT_OK;
}

void SingleDataField::dumpPrefix(bool prependFieldSeparator, bool asJson, ostream* output) const {
  *output << setw(0) << dec;  // initialize formatting
  if (asJson) {
    if (prependFieldSeparator) {
      *output << FIELD_SEPARATOR;
    }
    *output << "\n     {";
    appendJson(false, "name", m_name, true, output);
  } else {
    dumpString(prependFieldSeparator, m_name, output);
  }
  *output << FIELD_SEPARATOR;
  if (asJson) {
    *output << " \"slave\": " << (m_partType == pt_slaveData ? "true" : "false") << ", ";
  } else {
    if (m_partType == pt_masterData) {
      *output << "m";
    } else if (m_partType == pt_slaveData) {
      *output << "s";
    }
  }
  if (!asJson) {
    *output << FIELD_SEPARATOR;
  }
  m_dataType->dump(asJson, m_length, true, output);
}

void SingleDataField::dumpSuffix(bool asJson, ostream* output) const {
  dumpAttribute(true, asJson, "unit", output);
  dumpAttribute(true, asJson, "comment", output);
  if (asJson) {
    *output << "}";
  }
}

void SingleDataField::dump(bool prependFieldSeparator, bool asJson, ostream* output) const {
  dumpPrefix(prependFieldSeparator, asJson, output);
  dumpSuffix(asJson, output);
}

result_t SingleDataField::read(const SymbolString& data, size_t offset,
    const char* fieldName, ssize_t fieldIndex, unsigned int* output) const {
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
  if (isIgnored() || (fieldName != nullptr && m_name != fieldName) || fieldIndex > 0) {
    return RESULT_EMPTY;
  }
  result_t res = m_dataType->readRawValue(offset, m_length, data, output);
  return res;
}

result_t SingleDataField::read(const SymbolString& data, size_t offset,
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex,
    OutputFormat outputFormat, ssize_t outputIndex, ostream* output) const {
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
  if (isIgnored() || (fieldName != nullptr && m_name != fieldName) || fieldIndex > 0) {
    return RESULT_EMPTY;
  }
  bool shortFormat = outputFormat & OF_SHORT;
  if (outputFormat & OF_JSON) {
    if (leadingSeparator) {
      *output << ",";
    }
    if (fieldIndex < 0 && !shortFormat) {
      *output << "\n     ";
    }
    if (outputIndex >= 0 || m_name.empty() || !(outputFormat & OF_NAMES)) {
      if (fieldIndex < 0) {
        *output << "\"" << static_cast<signed int>(outputIndex < 0 ? 0 : outputIndex) << "\":";
      }
      if (!shortFormat) {
        *output << " {\"name\": \"" << m_name << "\"" << ", \"value\": ";
      }
    } else {
      if (fieldIndex < 0) {
        *output << "\"" << m_name << "\":";
      }
      if (!shortFormat) {
        *output << " {\"value\": ";
      }
    }
  } else {
    if (leadingSeparator) {
      *output << UI_FIELD_SEPARATOR;
    }
    if (outputFormat & OF_NAMES) {
      *output << m_name << "=";
    }
  }

  result_t result = readSymbols(data, offset, outputFormat, output);
  if (result != RESULT_OK) {
    return result;
  }
  if (!shortFormat) {
    appendAttributes(outputFormat, output);
  }
  if (!shortFormat && (outputFormat & OF_JSON)) {
    *output << "}";
  }
  return RESULT_OK;
}

result_t SingleDataField::write(char separator, size_t offset, istringstream* input,
    SymbolString* data, size_t* usedLength) const {
  if (m_partType == pt_any) {
    return RESULT_ERR_INVALID_PART;
  }
  if ((data->isMaster() ? pt_masterData : pt_slaveData) != m_partType) {
    return RESULT_OK;
  }
  return writeSymbols(offset, input, data, usedLength);
}

result_t SingleDataField::readSymbols(const SymbolString& input, size_t offset,
    OutputFormat outputFormat, ostream* output) const {
  return m_dataType->readSymbols(offset, m_length, input, outputFormat, output);
}

result_t SingleDataField::writeSymbols(size_t offset, istringstream* input,
    SymbolString* output, size_t* usedLength) const {
  return m_dataType->writeSymbols(offset, m_length, input, output, usedLength);
}

const SingleDataField* SingleDataField::clone() const {
  return new SingleDataField(*this);
}

result_t SingleDataField::derive(const string& name, PartType partType, int divisor,
    const map<unsigned int, string>& values, map<string, string>* attributes,
    vector<const SingleDataField*>* fields) const {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  bool numeric = m_dataType->isNumeric();
  if (!numeric && (divisor != 0 || !values.empty())) {
    return RESULT_ERR_INVALID_ARG;  // cannot set divisor or values for non-numeric field
  }
  string useName = name.empty() ? m_name : name;
  mergeAttributes(attributes);
  const DataType* dataType = m_dataType;
  if (numeric) {
    const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(dataType);
    result_t result = numType->derive(divisor, 0, &numType);
    if (result != RESULT_OK) {
      return result;
    }
    dataType = numType;
  }
  if (values.empty()) {
    fields->push_back(new SingleDataField(useName, *attributes, dataType, partType, m_length));
  } else if (numeric) {
    fields->push_back(new ValueListDataField(useName, *attributes, reinterpret_cast<const NumberDataType*>(dataType),
      partType, m_length, values));
  } else {
    return RESULT_ERR_INVALID_ARG;
  }
  return RESULT_OK;
}

bool SingleDataField::hasField(const char* fieldName, bool numeric) const {
  bool numericType = m_dataType->isNumeric();
  return numeric == numericType && (fieldName == nullptr || fieldName == m_name);
}

size_t SingleDataField::getLength(PartType partType, size_t maxLength) const {
  if (partType != m_partType) {
    return 0;
  }
  bool remainder = m_length == REMAIN_LEN && m_dataType->isAdjustableLength();
  return remainder ? maxLength : m_length;
}

bool SingleDataField::hasFullByteOffset(bool after, int16_t& previousFirstBit) const {
  if (m_length > 1) {
    if (after) {
      previousFirstBit = -1;
    }
    return true;
  }
  int16_t firstBit;
  if (m_dataType->isNumeric()) {
    const NumberDataType* num = reinterpret_cast<const NumberDataType*>(m_dataType);
    firstBit = num->getFirstBit();
  } else {
    firstBit = 0;
  }
  bool ret = (m_dataType->getBitCount() % 8) == 0
  || (firstBit == previousFirstBit) || (after && firstBit + (m_dataType->getBitCount() % 8) >= 8);
  // std::cout<<(after?"after,":"before,")<<"prev="<<static_cast<unsigned>(previousFirstBit)<<",first="
  // <<static_cast<unsigned>(firstBit)<<",length="<<static_cast<unsigned>(m_dataType->getBitCount())
  // <<" => "<<(ret?"true":"false")<<"\n";
  if (after) {
    previousFirstBit = ret ? -1 : firstBit;
  }
  return ret;
}

size_t SingleDataField::getCount(PartType partType, const char* fieldName) const {
  return isIgnored() || (partType != pt_any && partType != m_partType) || (fieldName != nullptr && m_name != fieldName)
  ? 0 : 1;
}


const ValueListDataField* ValueListDataField::clone() const {
  return new ValueListDataField(*this);
}

result_t ValueListDataField::derive(const string& name, PartType partType, int divisor,
    const map<unsigned int, string>& values, map<string, string>* attributes,
    vector<const SingleDataField*>* fields) const {
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
    fields->push_back(new ValueListDataField(useName, *attributes,
        reinterpret_cast<const NumberDataType*>(m_dataType), partType, m_length, values));
  } else {
    fields->push_back(new ValueListDataField(useName, *attributes,
        reinterpret_cast<const NumberDataType*>(m_dataType), partType, m_length, m_values));
  }
  return RESULT_OK;
}

void ValueListDataField::dump(bool prependFieldSeparator, bool asJson, ostream* output) const {
  dumpPrefix(prependFieldSeparator, asJson, output);
  // no divisor appended since it is not allowed for ValueListDataField
  bool first = true;
  if (asJson) {
    *output << ", \"values\": {";
    for (const auto it : m_values) {
      appendJson(!first, formatInt(it.first), it.second, true, output);  // TODO optimize?
      first = false;
    }
    *output << " }";
  } else {
    for (const auto it : m_values) {
      if (first) {
        first = false;
      } else {
        *output << VALUE_SEPARATOR;
      }
      *output << it.first << "=" << it.second;
    }
  }
  dumpSuffix(asJson, output);
}

result_t ValueListDataField::readSymbols(const SymbolString& input, size_t offset,
    OutputFormat outputFormat, ostream* output) const {
  unsigned int value = 0;

  result_t result = m_dataType->readRawValue(offset, m_length, input, &value);
  if (result != RESULT_OK) {
    return result;
  }
  const auto it = m_values.find(value);
  if (it == m_values.end() && value != m_dataType->getReplacement()) {
    // fall back to raw value in input
    *output << setw(0) << dec << value;
    return RESULT_OK;
  }
  if (it == m_values.end()) {
    if (outputFormat & OF_JSON) {
      *output << "null";
    } else if (value == m_dataType->getReplacement()) {
      *output << NULL_VALUE;
    }
  } else if (outputFormat & OF_NUMERIC) {
    *output << setw(0) << dec << value;
  } else if (outputFormat & OF_JSON) {
    if (outputFormat & OF_VALUENAME) {
      *output << "{\"value\":" << setw(0) << dec << value;
      *output << ",\"name\":\"" << it->second << "\"}";
    } else {
      *output << '"' << it->second << '"';
    }
  } else {
    if (outputFormat & OF_VALUENAME) {
      *output << setw(0) << dec << value << '=';
    }
    *output << it->second;
  }
  return RESULT_OK;
}

result_t ValueListDataField::writeSymbols(size_t offset, istringstream* input,
    SymbolString* output, size_t* usedLength) const {
  const NumberDataType* numType = reinterpret_cast<const NumberDataType*>(m_dataType);
  const string inputStr = input->str();
  if (isIgnored() || inputStr == NULL_VALUE) {
    // replacement value
    return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength);
  }

  for (map<unsigned int, string>::const_iterator it = m_values.begin(); it != m_values.end(); ++it) {
    if (it->second == inputStr) {
      return numType->writeRawValue(it->first, offset, m_length, output, usedLength);
    }
  }
  const char* str = inputStr.c_str();
  char* strEnd = nullptr;  // fall back to raw value in input
  unsigned int value;
  value = (unsigned int)strtoul(str, &strEnd, 10);
  if (strEnd == nullptr || strEnd == str || (*strEnd != 0 && *strEnd != '.')) {
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

result_t ConstantDataField::derive(const string& name, PartType partType, int divisor,
    const map<unsigned int, string>& values, map<string, string>* attributes,
    vector<const SingleDataField*>* fields) const {
  if (m_partType != pt_any && partType == pt_any) {
    return RESULT_ERR_INVALID_PART;  // cannot create a template from a concrete instance
  }
  string useName = name.empty() ? m_name : name;
  for (const auto entry : m_attributes) {  // merge with this attributes
    if ((*attributes)[entry.first].empty()) {
      (*attributes)[entry.first] = entry.second;
    }
  }
  if (divisor != 0) {
    return RESULT_ERR_INVALID_ARG;  // cannot use other than current divisor for constant value field
  }
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // cannot use value list for constant value field
  }
  fields->push_back(new ConstantDataField(useName, *attributes, m_dataType, partType, m_length, m_value, m_verify));
  return RESULT_OK;
}

void ConstantDataField::dump(bool prependFieldSeparator, bool asJson, ostream* output) const {
  dumpPrefix(prependFieldSeparator, asJson, output);
  // no divisor appended since it is not allowed for ConstantDataField
  if (asJson) {
    appendJson(false, "value", m_value, true, output);
    *output << ", \"verify\":" << (m_verify ? "true" : "false");
  } else {
    *output << (m_verify?"==":"=") << m_value;
  }
  dumpSuffix(asJson, output);
}

result_t ConstantDataField::readSymbols(const SymbolString& input, size_t offset,
    OutputFormat outputFormat, ostream* output) const {
  ostringstream coutput;
  result_t result = SingleDataField::readSymbols(input, offset, 0, &coutput);
  if (result != RESULT_OK) {
    return result;
  }
  if (m_verify) {
    string value = coutput.str();
    FileReader::trim(&value);
    if (value != m_value) {
      return RESULT_ERR_OUT_OF_RANGE;
    }
  }
  return RESULT_OK;
}

result_t ConstantDataField::writeSymbols(size_t offset, istringstream* input,
    SymbolString* output, size_t* usedLength) const {
  istringstream cinput(m_value);
  return SingleDataField::writeSymbols(offset, &cinput, output, usedLength);
}


DataFieldSet* DataFieldSet::s_identFields = nullptr;

DataFieldSet* DataFieldSet::getIdentFields() {
  if (s_identFields == nullptr) {
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
  for (const auto field : m_fields) {
    delete field;
  }
}

const DataFieldSet* DataFieldSet::clone() const {
  vector<const SingleDataField*> fields;
  for (const auto field : m_fields) {
    fields.push_back(field->clone());
  }
  return new DataFieldSet(m_name, fields);
}

size_t DataFieldSet::getLength(PartType partType, size_t maxLength) const {
  size_t length = 0;
  bool previousFullByteOffset[] = { true, true, true, true };
  int16_t previousFirstBit[] = { -1, -1, -1, -1 };
  for (const auto field : m_fields) {
    if (field->getPartType() == partType) {
      if (!previousFullByteOffset[partType] && !field->hasFullByteOffset(false, previousFirstBit[partType])) {
        length--;
      }
      size_t fieldLength = field->getLength(partType, maxLength);
      if (fieldLength >= maxLength) {
        maxLength = 0;
      } else {
        maxLength = maxLength - fieldLength;
      }
      length = length + fieldLength;

      previousFullByteOffset[partType] = field->hasFullByteOffset(true, previousFirstBit[partType]);
    }
  }

  return length;
}

size_t DataFieldSet::getCount(PartType partType, const char* fieldName) const {
  if (partType == pt_any && fieldName == nullptr) {
    return m_fields.size() - m_ignoredCount;
  }
  size_t count = 0;
  for (auto field : m_fields) {
    count += field->getCount(partType, fieldName);
  }
  return count;
}

string DataFieldSet::getName(ssize_t fieldIndex) const {
  if (fieldIndex < (ssize_t)m_ignoredCount) {
    return m_name;
  }
  if ((size_t)fieldIndex + m_ignoredCount >= m_fields.size()) {
    return "";
  }
  if (m_uniqueNames) {
    if (m_ignoredCount == 0) {
      return m_fields[fieldIndex]->getName(-1);
    }
    ssize_t remain = fieldIndex;
    for (const auto field : m_fields) {
      if (field->isIgnored()) {
        continue;
      }
      remain--;
      if (remain == 0) {
        return field->getName(-1);
      }
    }
  }
  ostringstream ostream;
  ostream << static_cast<signed>(fieldIndex);
  return ostream.str();
}

result_t DataFieldSet::derive(const string& name, PartType partType, int divisor,
    const map<unsigned int, string>& values, map<string, string>* attributes,
    vector<const SingleDataField*>* fields) const {
  if (!values.empty()) {
    return RESULT_ERR_INVALID_ARG;  // value list not allowed in set derive
  }
  for (const auto field : m_fields) {
    result_t result = field->derive("", partType, divisor, values, attributes, fields);
    if (result != RESULT_OK) {
      return result;
    }
    attributes->erase("comment");
    attributes->erase("unit");
  }

  return RESULT_OK;
}

bool DataFieldSet::hasField(const char* fieldName, bool numeric) const {
  for (const auto field : m_fields) {
    if (field->hasField(fieldName, numeric) == 0) {
      return true;
    }
  }
  return false;
}

void DataFieldSet::dump(bool prependFieldSeparator, bool asJson, ostream* output) const {
  for (const auto field : m_fields) {
    field->dump(prependFieldSeparator, asJson, output);
    prependFieldSeparator = true;
  }
}

result_t DataFieldSet::read(const SymbolString& data, size_t offset,
    const char* fieldName, ssize_t fieldIndex, unsigned int* output) const {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldIndex >= 0;
  int16_t previousFirstBit = -1;
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (const auto field : m_fields) {
    if (field->getPartType() != partType) {
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false, previousFirstBit)) {
      offset--;
    }
    result_t result = field->read(data, offset, fieldName, fieldIndex, output);
    if (result < RESULT_OK) {
      return result;
    }
    offset += field->getLength(partType, data.getDataSize()-offset);
    previousFullByteOffset = field->hasFullByteOffset(true, previousFirstBit);
    if (result != RESULT_EMPTY) {
      found = true;
    }
    if (findFieldIndex && !field->isIgnored() && (fieldName == nullptr || fieldName == field->getName(-1))) {
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
    bool leadingSeparator, const char* fieldName, ssize_t fieldIndex,
    OutputFormat outputFormat, ssize_t outputIndex, ostream* output) const {
  bool previousFullByteOffset = true, found = false, findFieldIndex = fieldIndex >= 0;
  int16_t previousFirstBit = -1;
  if (outputIndex < 0 && (!m_uniqueNames || ((outputFormat & OF_JSON) && !(outputFormat & OF_NAMES)))) {
    outputIndex = 0;
  }
  PartType partType = data.isMaster() ? pt_masterData : pt_slaveData;
  for (const auto field : m_fields) {
    if (field->getPartType() != partType) {
      if (outputIndex >= 0 && !field->isIgnored()) {
        outputIndex++;
      }
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false, previousFirstBit)) {
      offset--;
    }
    result_t result = field->read(data, offset, leadingSeparator, fieldName, fieldIndex,
        outputFormat, outputIndex, output);
    if (result < RESULT_OK) {
      return result;
    }
    offset += field->getLength(partType, data.getDataSize()-offset);
    previousFullByteOffset = field->hasFullByteOffset(true, previousFirstBit);
    if (result != RESULT_EMPTY) {
      found = true;
      leadingSeparator = true;
    }
    if (findFieldIndex && !field->isIgnored() && (fieldName == nullptr || fieldName == field->getName(-1))) {
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

result_t DataFieldSet::write(char separator, size_t offset, istringstream* input,
    SymbolString* data, size_t* usedLength) const {
  string token;
  PartType partType = data->isMaster() ? pt_masterData : pt_slaveData;
  bool previousFullByteOffset = true;
  int16_t previousFirstBit = -1;
  size_t baseOffset = offset;
  for (const auto field : m_fields) {
    if (field->getPartType() != partType) {
      continue;
    }
    if (!previousFullByteOffset && !field->hasFullByteOffset(false, previousFirstBit)) {
      offset--;
    }
    result_t result;
    size_t fieldLength;
    if (m_fields.size() > 1) {
      if (field->isIgnored()) {
        token.clear();
      } else if (!getline(*input, token, separator)) {
        token.clear();
      }
      istringstream single(token);
      result = field->write(separator, offset, &single, data, &fieldLength);
    } else {
      result = field->write(separator, offset, input, data, &fieldLength);
    }
    if (result != RESULT_OK) {
      return result;
    }
    offset += fieldLength;
    previousFullByteOffset = field->hasFullByteOffset(true, previousFirstBit);
  }

  if (usedLength != nullptr) {
    *usedLength = offset-baseOffset;
  }
  return RESULT_OK;
}


result_t LoadableDataFieldSet::getFieldMap(const string& preferLanguage, vector<string>* row,
    string* errorDescription) const {
  // *type,divisor/values,unit,comment
  if (row->empty()) {
    for (const auto& col : defaultFieldsFieldMap) {
      row->push_back(col);
    }
    return RESULT_OK;
  }
  map<string, size_t> seen;
  for (size_t col = 0; col < row->size(); col++) {
    string &name = (*row)[col];
    string lowerName = name;
    tolower(&lowerName);
    trim(&lowerName);
    bool toDataFields;
    if (!lowerName.empty() && lowerName[0] == '*') {
      lowerName.erase(0, 1);
      toDataFields = true;
    } else {
      toDataFields = col == 0;
    }
    if (lowerName.empty()) {
      *errorDescription = "missing name in column " + AttributedItem::formatInt(col);
      return RESULT_ERR_INVALID_ARG;
    }
    if (toDataFields) {
      if (!seen.empty() && seen.find("type") == seen.end()) {
        *errorDescription = "missing field type";
        return RESULT_ERR_EOF;  // require at least name and type
      }
      seen.clear();
    }
    size_t langPos = lowerName.find_last_of('.');
    if (langPos != string::npos && langPos > 0 && langPos == lowerName.length()-3) {
      string lang = lowerName.substr(langPos+1);
      lowerName.erase(langPos);
      map<string, size_t>::iterator previous = seen.find(lowerName);
      if (previous != seen.end()) {
        if (lang != preferLanguage) {
          // skip this column
          name = SKIP_COLUMN;
          continue;
        }
        // replace previous
        (*row)[previous->second] = SKIP_COLUMN;
        seen.erase(lowerName);
      }
    } else {
      map<string, size_t>::iterator previous = seen.find(lowerName);
      if (previous != seen.end()) {
        *errorDescription = "duplicate field " + name;
        return RESULT_ERR_INVALID_ARG;
      }
    }
    name = toDataFields ? "*"+lowerName : lowerName;
    seen[lowerName] = col;
  }
  if (seen.find("type") == seen.end()) {
    *errorDescription = "missing field type";
    return RESULT_ERR_EOF;  // require at least type
  }
  return RESULT_OK;
}

result_t LoadableDataFieldSet::addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
    vector< map<string, string> >* subRows, string* errorDescription, bool replace) {
  const DataField* field = nullptr;
  result_t result = DataField::create(false, false, false, MAX_POS, m_templates, subRows, errorDescription, &field);
  if (result != RESULT_OK) {
    return result;
  }
  if (!field) {
    return RESULT_ERR_INVALID_ARG;
  }
  map<string, string> names;
  for (auto check : m_fields) {
    if (check->isIgnored()) {
      continue;
    }
    string name = check->getName(-1);
    if (!name.empty()) {
      names[name] = name;
    }
  }
  if (field->isSet()) {
    const DataFieldSet* fieldSet = dynamic_cast<const DataFieldSet*>(field);
    for (auto sfield : fieldSet->m_fields) {
      m_fields.push_back(sfield);
      if (sfield->isIgnored()) {
        m_ignoredCount++;
        continue;
      }
      string name = sfield->getName(-1);
      if (name.empty() || names.find(name) != names.end()) {
        m_uniqueNames = false;
      } else {
        names[name] = name;
      }
    }
  } else {
    const SingleDataField* sfield = dynamic_cast<const SingleDataField*>(field);
    m_fields.push_back(sfield);
    if (sfield->isIgnored()) {
      m_ignoredCount++;
    } else {
      string name = sfield->getName(-1);
      if (name.empty() || names.find(name) != names.end()) {
        m_uniqueNames = false;
      } else {
        names[name] = name;
      }
    }
  }
  return result;
}


DataFieldTemplates::DataFieldTemplates(const DataFieldTemplates& other)
: MappedFileReader::MappedFileReader(false) {
  for (const auto it : other.m_fieldsByName) {
    m_fieldsByName[it.first] = it.second->clone();
  }
}

void DataFieldTemplates::clear() {
  for (auto it : m_fieldsByName) {
    delete it.second;
    it.second = nullptr;
  }
  m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(const DataField* field, string name, bool replace) {
  if (name.length() == 0) {
    name = field->getName(-1);
  }
  const auto it = m_fieldsByName.find(name);
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

result_t DataFieldTemplates::getFieldMap(const string& preferLanguage, vector<string>* row, string* errorDescription)
    const {
  // name[:usename],basetype[:len]|template[:usename][,[divisor|values][,[unit][,[comment]]]]
  if (row->empty()) {
    // default map does not include separate field name
    for (const auto& col : defaultTemplateFieldMap) {
      row->push_back(col);
    }
    return RESULT_OK;
  }
  bool inDataFields = false;
  map<string, size_t> seen;
  for (size_t col = 0; col < row->size(); col++) {
    string &name = (*row)[col];
    string lowerName = name;
    tolower(&lowerName);
    trim(&lowerName);
    bool toDataFields;
    if (!lowerName.empty() && lowerName[0] == '*') {
      lowerName.erase(0, 1);
      toDataFields = true;
    } else {
      toDataFields = false;
    }
    if (lowerName.empty()) {
      *errorDescription = "missing name in column " + AttributedItem::formatInt(col);
      return RESULT_ERR_INVALID_ARG;
    }
    if (toDataFields) {
      if (inDataFields) {
        if (seen.find("type") == seen.end()) {
          *errorDescription = "missing field type";
          return RESULT_ERR_EOF;  // require at least name and type
        }
      } else {
        if (seen.find("name") == seen.end()) {
          *errorDescription = "missing template name";
          return RESULT_ERR_EOF;  // require at least name
        }
        if (seen.size() > 1) {
          *errorDescription = "extra template columns";
          return RESULT_ERR_INVALID_ARG;
        }
        inDataFields = true;
      }
      seen.clear();
    }
    size_t langPos = lowerName.find_last_of('.');
    if (langPos != string::npos && langPos > 0 && langPos == lowerName.length()-3) {
      string lang = lowerName.substr(langPos+1);
      lowerName.erase(langPos);
      map<string, size_t>::iterator previous = seen.find(lowerName);
      if (previous != seen.end()) {
        if (lang != preferLanguage) {
          // skip this column
          name = SKIP_COLUMN;
          continue;
        }
        // replace previous
        (*row)[previous->second] = SKIP_COLUMN;
        seen.erase(lowerName);
      }
    } else {
      map<string, size_t>::iterator previous = seen.find(lowerName);
      if (previous != seen.end()) {
        if (inDataFields) {
          *errorDescription = "duplicate field " + name;
        } else {
          *errorDescription = "duplicate template " + name;
        }
        return RESULT_ERR_INVALID_ARG;
      }
    }
    name = toDataFields ? "*"+lowerName : lowerName;
    seen[lowerName] = col;
  }
  if (!inDataFields) {
    *errorDescription = "missing template fields";
    return RESULT_ERR_EOF;  // require at least one field
  }
  if (seen.find("type") == seen.end()) {
    *errorDescription = "missing field type";
    return RESULT_ERR_EOF;  // require at least type
  }
  return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
    vector< map<string, string> >* subRows, string* errorDescription, bool replace) {
  string name = (*row)["name"];  // required
  string firstFieldName;
  size_t colon = name.find(':');
  if (colon == string::npos) {
    firstFieldName = name;
  } else {
    firstFieldName = name.substr(colon+1);
    name = name.substr(0, colon);
  }
  const DataField* field = nullptr;
  if (!subRows->empty()) {
    map<string, string>::iterator it = (*subRows)[0].find("name");
    if (it == (*subRows)[0].end() || it->second.empty()) {
      (*subRows)[0]["name"] = firstFieldName;
    }
  }
  result_t result = DataField::create(false, true, false, MAX_POS, this, subRows, errorDescription, &field);
  if (result != RESULT_OK) {
    return result;
  }
  result = add(field, name, replace);
  if (result == RESULT_ERR_DUPLICATE_NAME) {
    *errorDescription = name;
  }
  if (result != RESULT_OK) {
    delete field;
  }
  return result;
}

const DataField* DataFieldTemplates::get(const string& name) const {
  const auto ref = m_fieldsByName.find(name);
  if (ref == m_fieldsByName.end()) {
    return nullptr;
  }
  return ref->second;
}

}  // namespace ebusd

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "data.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>
#include <typeinfo>

using namespace std;

/* additional flags for @a DataType. */
static const unsigned int DAY = LAST_DATATYPE_FLAG<<1; //!< forced value list defaulting to week days

/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

/**
 * The STR data type: >= 1 byte character string filled up with space.
 */
static StringDataType* stringDataType = new StringDataType("STR", MAX_LEN*8, ADJ, ' ');

/**
 * The PIN data type: unsigned decimal in BCD, 0000 - 9999 (fixed length).
 */
static NumberDataType* pinDataType = new NumberDataType(
	"PIN", 16, FIX|BCD|REV, 0xffff, 0, 0x9999, 1
);

/**
 * The UCH data type: unsigned integer, 0 - 254.
 */
static NumberDataType* uchDataType = new NumberDataType(
	"UCH", 8, 0, 0xff, 0, 0xfe, 1
);

/** the known base @a DataType instances by ID.
 * Note: adjustable length types are stored with the ID only, all others are stored by "ID:BITS". */
static map<string, DataType*> baseTypes;

/**
 * Register a base @a DataType.
 * @baseType the @a DataType to register.
 */
void registerBaseType(DataType* baseType) {
	if (!baseType->isAdjustableLength()) {
		ostringstream str;
		str << baseType->getId() << LENGTH_SEPARATOR << static_cast<unsigned>(baseType->getBitCount()>=8?baseType->getBitCount()/8:baseType->getBitCount());
		baseTypes[str.str()] = baseType;
		if (baseTypes.find(baseType->getId()) != baseTypes.end()) {
			return; // only store first one as default
		}
	}
	baseTypes[baseType->getId()] = baseType;
}

/** Register the known base data types. */
bool registerBaseTypes() {
	registerBaseType(stringDataType);
	registerBaseType(pinDataType);
	registerBaseType(uchDataType);
	registerBaseType(new StringDataType("IGN", MAX_LEN*8, IGN|ADJ, 0)); // >= 1 byte ignored data
	registerBaseType(new StringDataType("NTS", MAX_LEN*8, ADJ, 0)); // >= 1 byte character string filled up with 0x00 (null terminated string)
	registerBaseType(new StringDataType("HEX", MAX_LEN*8, ADJ, 0, true)); // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	registerBaseType(new DateTimeDataType("BDA", 32, BCD, 0xff, true, 0)); // date with weekday in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is weekday Mon=0x00 - Sun=0x06, replacement 0xff)
	registerBaseType(new DateTimeDataType("BDA", 24, BCD, 0xff, true, 0)); // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99, replacement 0xff)
	registerBaseType(new DateTimeDataType("HDA", 32, 0, 0xff, true, 0)); // date with weekday, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x1f,0x0c,WW,0x63, WW is weekday Mon=0x01 - Sun=0x07, replacement 0xff)
	registerBaseType(new DateTimeDataType("HDA", 24, 0, 0xff, true, 0)); // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x1f,0x0c,0x63, replacement 0xff)
	registerBaseType(new DateTimeDataType("DAY", 16, REQ, 0, true, 0)); // date, days since 01.01.1900, 01.01.1900 - 06.06.2079 (0x00,0x00 - 0xff,0xff)
	registerBaseType(new DateTimeDataType("BTI", 24, BCD|REV|REQ, 0, false, 0)); // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	registerBaseType(new DateTimeDataType("HTI", 24, REQ, 0, false, 0)); // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x17,0x3b,0x3b)
	registerBaseType(new DateTimeDataType("VTI", 24, REV, 0x63, false, 0)); // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x3b,0x3b,0x17, replacement 0x63) [Vaillant type]
	registerBaseType(new DateTimeDataType("BTM", 16, BCD|REV, 0xff, false, 0)); // time as hh:mm in BCD, 00:00 - 23:59 (0x00,0x00 - 0x59,0x23, replacement 0xff)
	registerBaseType(new DateTimeDataType("HTM", 16, REQ, 0, false, 0)); // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x17,0x3b)
	registerBaseType(new DateTimeDataType("VTM", 16, REV, 0xff, false, 0)); // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x3b,0x17, replacement 0xff) [Vaillant type]
	registerBaseType(new DateTimeDataType("TTM", 8, 0, 0x90, false, 10)); // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	registerBaseType(new DateTimeDataType("TTH", 8, 0, 0, false, 30)); // truncated time (only multiple of 30 minutes), 00:30 - 24:00 (minutes div 30 + hour * 2 as integer)
	registerBaseType(new NumberDataType("BDY", 8, DAY, 0x07, 0, 6, 1)); // weekday, "Mon" - "Sun" (0x00 - 0x06) [eBUS type]
	registerBaseType(new NumberDataType("HDY", 8, DAY, 0x00, 1, 7, 1)); // weekday, "Mon" - "Sun" (0x01 - 0x07) [Vaillant type]
	registerBaseType(new NumberDataType("BCD", 8, BCD, 0xff, 0, 99, 1)); // unsigned decimal in BCD, 0 - 99
	registerBaseType(new NumberDataType("BCD", 16, BCD, 0xffff, 0, 9999, 1)); // unsigned decimal in BCD, 0 - 9999
	registerBaseType(new NumberDataType("BCD", 24, BCD, 0xffffff, 0, 999999, 1)); // unsigned decimal in BCD, 0 - 999999
	registerBaseType(new NumberDataType("BCD", 32, BCD, 0xffffffff, 0, 99999999, 1)); // unsigned decimal in BCD, 0 - 99999999
	registerBaseType(new NumberDataType("HCD", 32, HCD|BCD|REQ, 0, 0, 99999999, 1)); // unsigned decimal in HCD, 0 - 99999999
	registerBaseType(new NumberDataType("HCD", 8, HCD|BCD|REQ, 0, 0, 99, 1)); // unsigned decimal in HCD, 0 - 99
	registerBaseType(new NumberDataType("HCD", 16, HCD|BCD|REQ, 0, 0, 9999, 1)); // unsigned decimal in HCD, 0 - 9999
	registerBaseType(new NumberDataType("HCD", 24, HCD|BCD|REQ, 0, 0, 999999, 1)); // unsigned decimal in HCD, 0 - 999999
	registerBaseType(new NumberDataType("SCH", 8, SIG, 0x80, 0x81, 0x7f, 1)); // signed integer, -127 - +127
	registerBaseType(new NumberDataType("D1B", 8, SIG, 0x80, 0x81, 0x7f, 1)); // signed integer, -127 - +127
	registerBaseType(new NumberDataType("D1C", 8, 0, 0xff, 0x00, 0xc8, 2)); // unsigned number (fraction 1/2), 0 - 100 (0x00 - 0xc8, replacement 0xff)
	registerBaseType(new NumberDataType("D2B", 16, SIG, 0x8000, 0x8001, 0x7fff, 256)); // signed number (fraction 1/256), -127.99 - +127.99
	registerBaseType(new NumberDataType("D2C", 16, SIG, 0x8000, 0x8001, 0x7fff, 16)); // signed number (fraction 1/16), -2047.9 - +2047.9
	registerBaseType(new NumberDataType("FLT", 16, SIG, 0x8000, 0x8001, 0x7fff, 1000)); // signed number (fraction 1/1000), -32.767 - +32.767, little endian
	registerBaseType(new NumberDataType("FLR", 16, SIG|REV, 0x8000, 0x8001, 0x7fff, 1000)); // signed number (fraction 1/1000), -32.767 - +32.767, big endian
	registerBaseType(new NumberDataType("EXP", 32, SIG|EXP, 0x7f800000, 0x00000000, 0xffffffff, 1)); // signed number (IEEE 754 binary32: 1 bit sign, 8 bits exponent, 23 bits significand), little endian
	registerBaseType(new NumberDataType("EXR", 32, SIG|EXP|REV, 0x7f800000, 0x00000000, 0xffffffff, 1)); // signed number (IEEE 754 binary32: 1 bit sign, 8 bits exponent, 23 bits significand), big endian
	registerBaseType(new NumberDataType("UIN", 16, 0, 0xffff, 0, 0xfffe, 1)); // unsigned integer, 0 - 65534, little endian
	registerBaseType(new NumberDataType("UIR", 16, REV, 0xffff, 0, 0xfffe, 1)); // unsigned integer, 0 - 65534, big endian
	registerBaseType(new NumberDataType("SIN", 16, SIG, 0x8000, 0x8001, 0x7fff, 1)); // signed integer, -32767 - +32767, little endian
	registerBaseType(new NumberDataType("SIR", 16, SIG|REV, 0x8000, 0x8001, 0x7fff, 1)); // signed integer, -32767 - +32767, big endian
	registerBaseType(new NumberDataType("ULG", 32, 0, 0xffffffff, 0, 0xfffffffe, 1)); // unsigned integer, 0 - 4294967294, little endian
	registerBaseType(new NumberDataType("ULR", 32, REV, 0xffffffff, 0, 0xfffffffe, 1)); // unsigned integer, 0 - 4294967294, big endian
	registerBaseType(new NumberDataType("SLG", 32, SIG, 0x80000000, 0x80000001, 0xffffffff, 1)); // signed integer, -2147483647 - +2147483647, little endian
	registerBaseType(new NumberDataType("SLR", 32, SIG|REV, 0x80000000, 0x80000001, 0xffffffff, 1)); // signed integer, -2147483647 - +2147483647, big endian
	registerBaseType(new NumberDataType("BI0", 7, ADJ|REQ, 0, 0, 1)); // bit 0 (up to 7 bits until bit 6)
	registerBaseType(new NumberDataType("BI1", 7, ADJ|REQ, 0, 1, 1)); // bit 1 (up to 7 bits until bit 7)
	registerBaseType(new NumberDataType("BI2", 6, ADJ|REQ, 0, 2, 1)); // bit 2 (up to 6 bits until bit 7)
	registerBaseType(new NumberDataType("BI3", 5, ADJ|REQ, 0, 3, 1)); // bit 3 (up to 5 bits until bit 7)
	registerBaseType(new NumberDataType("BI4", 4, ADJ|REQ, 0, 4, 1)); // bit 4 (up to 4 bits until bit 7)
	registerBaseType(new NumberDataType("BI5", 3, ADJ|REQ, 0, 5, 1)); // bit 5 (up to 3 bits until bit 7)
	registerBaseType(new NumberDataType("BI6", 2, ADJ|REQ, 0, 6, 1)); // bit 6 (up to 2 bits until bit 7)
	registerBaseType(new NumberDataType("BI7", 1, ADJ|REQ, 0, 7, 1)); // bit 7
	return true;
}

static bool baseTypesRegistered = registerBaseTypes();

result_t DataField::create(vector<string>::iterator& it,
		const vector<string>::iterator end,
		DataFieldTemplates* templates, DataField*& returnField,
		const bool isWriteMessage,
		const bool isTemplate, const bool isBroadcastOrMasterDestination,
		const unsigned char maxFieldLength)
{
	vector<SingleDataField*> fields;
	string firstName, firstComment;
	result_t result = RESULT_OK;
	if (it == end)
		return RESULT_ERR_EOF;

	while (it != end && result == RESULT_OK) {
		string unit, comment;
		PartType partType;
		int divisor = 0;
		bool hasPartStr = false;
		string token;

		// template: name,type[:len][,[divisor|values][,[unit][,[comment]]]]
		// std: name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
		const string name = *it++; // name
		if (it == end) {
			if (!name.empty())
				result = RESULT_ERR_MISSING_TYPE;
			break;
		}

		if (isTemplate)
			partType = pt_any;
		else {
			const char* partStr = (*it++).c_str(); // part
			hasPartStr = partStr[0] != 0;
			if (it == end) {
				if (!name.empty() || hasPartStr)
					result = RESULT_ERR_MISSING_TYPE;
				break;
			}
			if (isBroadcastOrMasterDestination
				|| (isWriteMessage && !hasPartStr)
				|| strcasecmp(partStr, "M") == 0) { // master data
				partType = pt_masterData;
			}
			else if ((!isWriteMessage && !hasPartStr)
				|| strcasecmp(partStr, "S") == 0) { // slave data
				partType = pt_slaveData;
			}
			else {
				result = RESULT_ERR_INVALID_PART;
				break;
			}
		}

		if (fields.empty()) {
			firstName = name;
			firstComment = comment;
		}

		const string typeStr = *it++; // type[:len]
		if (typeStr.empty()) {
			if (!name.empty() || hasPartStr)
				result = RESULT_ERR_MISSING_TYPE;
			break;
		}

		map<unsigned int, string> values;
		if (it != end) {
			const string divisorStr = *it++; // [divisor|values]
			if (!divisorStr.empty()) {
				if (divisorStr.find('=') == string::npos)
					divisor = parseSignedInt(divisorStr.c_str(), 10, -MAX_DIVISOR, MAX_DIVISOR, result);
				else {
					istringstream stream(divisorStr);
					while (getline(stream, token, VALUE_SEPARATOR)) {
						FileReader::trim(token);
						const char* str = token.c_str();
						char* strEnd = NULL;
						unsigned long int id;
						if (strncasecmp(str, "0x", 2) == 0) {
							str += 2;
							id = strtoul(str, &strEnd, 16); // hexadecimal
						} else {
							id = strtoul(str, &strEnd, 10); // decimal
						}
						if (strEnd == NULL || strEnd == str || id > MAX_VALUE) {
							result = RESULT_ERR_INVALID_LIST;
							break;
						}
						// remove blanks around '=' sign
						while (*strEnd == ' ') strEnd++;
						if (*strEnd != '=') {
							result = RESULT_ERR_INVALID_LIST;
							break;
						}
						token = string(strEnd + 1);
						FileReader::trim(token);
						values[(unsigned int)id] = token;
					}
				}
				if (result != RESULT_OK)
					break;
			}
		}

		if (it == end)
			unit = "";
		else {
			const string str = *it++; // [unit]
			if (strcasecmp(str.c_str(), NULL_VALUE) == 0)
				unit = "";
			else
				unit = str;
		}

		if (it == end)
			comment = "";
		else {
			const string str = *it++; // [comment]
			if (strcasecmp(str.c_str(), NULL_VALUE) == 0)
				comment = "";
			else
				comment = str;
		}

		bool firstType = true;
		istringstream stream(typeStr);
		while (result == RESULT_OK && getline(stream, token, VALUE_SEPARATOR)) {
			FileReader::trim(token);
			DataField* templ = templates->get(token);
			unsigned char length;
			if (templ == NULL) {
				size_t pos = token.find(LENGTH_SEPARATOR);
				if (pos == string::npos)
					length = 0; // no length specified
				else if (pos+2==token.length() && token[pos+1]=='*') {
					length = REMAIN_LEN;
				} else {
					length = (unsigned char)parseInt(token.substr(pos+1).c_str(), 10, 1, maxFieldLength, result);
					if (result != RESULT_OK)
						break;
				}
				transform(token.begin(), token.end(), token.begin(), ::toupper);
				string typeName = token.substr(0, pos);
				SingleDataField* add = NULL;
				result = SingleDataField::create(token.c_str(), typeName.c_str(), length, firstType ? name : "", firstType ? comment : "", firstType ? unit : "", partType, divisor, values, add);
				if (add != NULL)
					fields.push_back(add);
				else if (result == RESULT_OK)
					result = RESULT_ERR_NOTFOUND; // type not found
			} else {
				bool lastType = stream.eof();
				result = templ->derive((firstType && lastType) ? name : "", firstType ? comment : "", firstType ? unit : "", partType, divisor, values, fields);
			}
			firstType = false;
		}
	}

	if (result != RESULT_OK) {
		while (!fields.empty()) { // cleanup already created fields
			delete fields.back();
			fields.pop_back();
		}
		return result;
	}

	if (fields.size() == 1)
		returnField = fields[0];
	else {
		returnField = new DataFieldSet(firstName, firstComment, fields);
	}
	return RESULT_OK;
}

void DataField::dumpString(ostream& output, const string str, const bool prependFieldSeparator)
{
	if (prependFieldSeparator)
		output << FIELD_SEPARATOR;
	if (str.find_first_of(FIELD_SEPARATOR) == string::npos) {
		output << str;
	} else {
		output << TEXT_SEPARATOR << str << TEXT_SEPARATOR;
	}
}

result_t SingleDataField::create(const char* fullIdStr, const char* idStr, const unsigned char length,
	const string name, const string comment, const string unit,
	const PartType partType, int divisor, map<unsigned int, string> values,
	SingleDataField* &returnField)
{
	DataType* dataType = baseTypes[fullIdStr];
	if (!dataType) {
		dataType = baseTypes[idStr];
		if (!dataType) {
			return RESULT_ERR_NOTFOUND;
		}
		if (!dataType->isAdjustableLength()) {
			return RESULT_ERR_OUT_OF_RANGE;
		}
	}
	unsigned char bitCount = dataType->getBitCount();
	unsigned char byteCount = (unsigned char)((bitCount + 7) / 8);
	if (dataType->isAdjustableLength()) {
		// check length
		if ((bitCount % 8) != 0) {
			if (length == 0) {
				bitCount = 1; // default bit count: 1 bit
			} else if (length <= bitCount) {
				bitCount = length;
			} else {
				return RESULT_ERR_OUT_OF_RANGE; // invalid length
			}
			byteCount = (unsigned char)((bitCount + 7) / 8);
		} else if (length == 0) {
			byteCount = 1; //default byte count: 1 byte
		} else if (length <= byteCount || length == REMAIN_LEN) {
			byteCount = length;
		} else {
			return RESULT_ERR_OUT_OF_RANGE; // invalid length
		}
	}
	if (typeid(*dataType)==typeid(StringDataType)
	|| typeid(*dataType)==typeid(DateTimeDataType)) {
		if (divisor != 0 || !values.empty())
			return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for string field
		returnField = new SingleDataField(name, comment, unit, (StringDataType*)dataType, partType, byteCount);
		return RESULT_OK;
	}
	if (typeid(*dataType)==typeid(NumberDataType)) {
		NumberDataType* numType = (NumberDataType*)dataType;
		if (values.empty() && numType->hasFlag(DAY)) {
			for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++)
				values[numType->getMinValue() + i] = dayNames[i];
		}
		if (values.empty()) {
			result_t result = numType->derive(divisor, bitCount, numType);
			if (result!=RESULT_OK)
				return result;
			returnField = new SingleDataField(name, comment, unit, numType, partType, byteCount);
			return RESULT_OK;
		}
		if (values.begin()->first < numType->getMinValue() || values.rbegin()->first > numType->getMaxValue()) {
			return RESULT_ERR_OUT_OF_RANGE;
		}
		//TODO add special field for fixed values (exactly one value in the list of values)
		returnField = new ValueListDataField(name, comment, unit, numType, partType, byteCount, values);
		return RESULT_OK;
	}
	return RESULT_ERR_NOTFOUND;
}

void SingleDataField::dump(ostream& output)
{
	output << setw(0) << dec; // initialize formatting
	dumpString(output, m_name, false);
	output << FIELD_SEPARATOR;
	if (m_partType == pt_masterData)
		output << "m";
	else if (m_partType == pt_slaveData)
		output << "s";
	output << FIELD_SEPARATOR;
	if (!m_dataType->dump(output, m_length)) { // no divisor appended
		output << FIELD_SEPARATOR; // no value list, no divisor
	}
	dumpString(output, m_unit);
	dumpString(output, m_comment);
}


result_t SingleDataField::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	if (partType != m_partType)
		return RESULT_EMPTY;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
		bool remainder = m_length==REMAIN_LEN && m_dataType->isAdjustableLength();
		if (!remainder && offset + m_length > data.size()) {
			return RESULT_ERR_INVALID_POS;
		}
		return RESULT_EMPTY;
	}
	return m_dataType->readRawValue(data, offset, m_length, output);
}

result_t SingleDataField::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, OutputFormat outputFormat, signed char outputIndex,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	if (isIgnored() || (fieldName != NULL && (m_name != fieldName || fieldIndex > 0))) {
		bool remainder = m_length==REMAIN_LEN && m_dataType->isAdjustableLength();
		if (!remainder && offset + m_length > data.size()) {
			return RESULT_ERR_INVALID_POS;
		}
		return RESULT_EMPTY;
	}

	if (outputFormat & OF_JSON) {
		if (leadingSeparator)
			output << ",";
		if (outputIndex>=0 || m_name.empty())
			output << "\n    \"" << static_cast<signed int>(outputIndex<0?0:outputIndex) << "\": {\"name\": \"" << m_name << "\"" << ", \"value\": ";
		else
			output << "\n    \"" << m_name << "\": {\"value\": ";
	} else {
		if (leadingSeparator)
			output << UI_FIELD_SEPARATOR;
		if (outputFormat & OF_VERBOSE)
			output << m_name << "=";
	}

	result_t result = readSymbols(data, offset, output, outputFormat);
	if (result != RESULT_OK)
		return result;

	if (outputFormat & OF_VERBOSE) {
		if (m_unit.length() > 0) {
			if (outputFormat & OF_JSON)
				output << ", \"unit\": \"" << m_unit << '"';
			else
				output << " " << m_unit;
		}
		if (m_comment.length() > 0) {
			if (outputFormat & OF_JSON)
				output << ", \"comment\": \"" << m_comment << '"';
			else
				output << " [" << m_comment << "]";
		}
	}
	if (outputFormat & OF_JSON)
		output << "}";
	return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator, unsigned char* length)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset = (unsigned char)(offset + 5); // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset++; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	return writeSymbols(input, (const unsigned char)offset, data, length);
}

result_t SingleDataField::readSymbols(SymbolString& input,
		const unsigned char offset,
		ostringstream& output, OutputFormat outputFormat)
{
	return m_dataType->readSymbols(input, offset, m_length, output, outputFormat);
}

result_t SingleDataField::writeSymbols(istringstream& input,
	const unsigned char offset,
	SymbolString& output, unsigned char* usedLength)
{
	return m_dataType->writeSymbols(input, offset, m_length, output, usedLength);
}

SingleDataField* SingleDataField::clone()
{
	return new SingleDataField(*this);
}

result_t SingleDataField::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	bool numeric = typeid(*m_dataType)==typeid(NumberDataType);
	if (!numeric && (divisor != 0 || !values.empty()))
		return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for non-numeric field
	if (name.empty())
		name = m_name;
	if (comment.empty())
		comment = m_comment;
	if (unit.empty())
		unit = m_unit;
	DataType* dataType = m_dataType;
	if (numeric) {
		NumberDataType* numType = (NumberDataType*)m_dataType;
		result_t result = numType->derive(divisor, 0, numType);
		if (result != RESULT_OK)
			return result;
		dataType = numType;
	}
	if (values.empty()) {
		fields.push_back(new SingleDataField(name, comment, unit, dataType, partType, m_length));
	} else {
		fields.push_back(new ValueListDataField(name, comment, unit, (NumberDataType*)dataType, partType, m_length, values));
	}
	return RESULT_OK;
}

bool SingleDataField::hasField(const char* fieldName, bool numeric)
{
	bool numericType = typeid(*m_dataType)==typeid(NumberDataType);
	return numeric==numericType && (fieldName==NULL || fieldName==m_name);
}

unsigned char SingleDataField::getLength(PartType partType, unsigned char maxLength)
{
	if (partType != m_partType) {
		return (unsigned char)0;
	}
	bool remainder = m_length==REMAIN_LEN && m_dataType->isAdjustableLength();
	return remainder ? maxLength : m_length;
}

bool SingleDataField::hasFullByteOffset(bool after)
{
	if (m_length > 1 || typeid(*m_dataType) != typeid(NumberDataType)) {
		return true;
	}
	NumberDataType* num = (NumberDataType*)m_dataType;
	return (num->getBitCount() % 8) == 0
		|| (after && num->getFirstBit() + (num->getBitCount() % 8) >= 8);
}


ValueListDataField* ValueListDataField::clone()
{
	return new ValueListDataField(*this);
}

result_t ValueListDataField::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (name.empty())
		name = m_name;
	if (comment.empty())
		comment = m_comment;
	if (unit.empty())
		unit = m_unit;
	if (divisor != 0 && divisor != 1)
		return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

	if (!values.empty()) {
		NumberDataType* num = (NumberDataType*)m_dataType;
		if (values.begin()->first < num->getMinValue() || values.rbegin()->first > num->getMaxValue())
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
	}
	else
		values = m_values;

	fields.push_back(new ValueListDataField(name, comment, unit, (NumberDataType*)m_dataType, partType, m_length, values));

	return RESULT_OK;
}

void ValueListDataField::dump(ostream& output)
{
	output << setw(0) << dec; // initialize formatting
	dumpString(output, m_name, false);
	output << FIELD_SEPARATOR;
	if (m_partType == pt_masterData)
		output << "m";
	else if (m_partType == pt_slaveData)
		output << "s";
	output << FIELD_SEPARATOR;
	if (!m_dataType->dump(output, m_length)) { // no divisor appended
		bool first = true;
		for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++) {
			if (first)
				first = false;
			else
				output << VALUE_SEPARATOR;
			output << static_cast<unsigned>(it->first) << "=" << it->second;
		}
	} // TODO else does not fit into CSV format!
	dumpString(output, m_unit);
	dumpString(output, m_comment);
}

result_t ValueListDataField::readSymbols(SymbolString& input,
		const unsigned char offset,
		ostringstream& output, OutputFormat outputFormat)
{
	unsigned int value = 0;

	result_t result = m_dataType->readRawValue(input, offset, m_length, value);
	if (result != RESULT_OK)
		return result;
	map<unsigned int, string>::iterator it = m_values.find(value);
	if (it == m_values.end() && value != m_dataType->getReplacement()) {
		// fall back to raw value in input
		output << setw(0) << dec << static_cast<unsigned>(value);
		return RESULT_OK;
	}
	if (it == m_values.end()) {
		if (outputFormat & OF_JSON)
			output << "null";
		else if (value == m_dataType->getReplacement())
			output << NULL_VALUE;
	} else if (outputFormat & OF_NUMERIC)
		output << setw(0) << dec << static_cast<unsigned>(value);
	else if (outputFormat & OF_JSON)
		output << '"' << it->second << '"';
	else
		output << it->second;
	return RESULT_OK;
}

result_t ValueListDataField::writeSymbols(istringstream& input,
		const unsigned char offset,
		SymbolString& output, unsigned char* usedLength)
{
	NumberDataType* numType = (NumberDataType*)m_dataType;
	if (isIgnored())
		return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength); // replacement value

	const char* str = input.str().c_str();

	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return numType->writeRawValue(it->first, offset, m_length, output, usedLength);

	if (strcasecmp(str, NULL_VALUE) == 0)
		return numType->writeRawValue(numType->getReplacement(), offset, m_length, output, usedLength); // replacement value

	char* strEnd = NULL; // fall back to raw value in input
	unsigned int value;
	value = (unsigned int)strtoul(str, &strEnd, 10);
	if (strEnd == NULL || strEnd == str || (*strEnd != 0 && *strEnd != '.'))
		return RESULT_ERR_INVALID_NUM; // invalid value
	if (m_values.find(value) != m_values.end())
		return numType->writeRawValue(value, offset, m_length, output, usedLength);

	return RESULT_ERR_NOTFOUND; // value assignment not found
}


DataFieldSet* DataFieldSet::s_identFields = NULL;

DataFieldSet* DataFieldSet::getIdentFields()
{
	if (s_identFields==NULL) {
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

DataFieldSet::~DataFieldSet()
{
	while (!m_fields.empty()) {
		delete m_fields.back();
		m_fields.pop_back();
	}
}

DataFieldSet* DataFieldSet::clone()
{
	vector<SingleDataField*> fields;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		fields.push_back((*it)->clone());
	}
	return new DataFieldSet(m_name, m_comment, fields);
}

unsigned char DataFieldSet::getLength(PartType partType, unsigned char maxLength)
{
	unsigned char length = 0;

	bool previousFullByteOffset[] = { true, true, true, true };

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->getPartType() == partType) {
			if (!previousFullByteOffset[partType] && !field->hasFullByteOffset(false))
				length--;

			unsigned char fieldLength = field->getLength(partType, maxLength);
			if (fieldLength>=maxLength)
				maxLength = 0;
			else
				maxLength = (unsigned char)(maxLength-fieldLength);
			length = (unsigned char)(length + fieldLength);

			previousFullByteOffset[partType] = field->hasFullByteOffset(true);
		}
	}

	return length;
}

result_t DataFieldSet::derive(string name, string comment,
		string unit, const PartType partType,
		int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (!values.empty())
		return RESULT_ERR_INVALID_ARG; // value list not allowed in set derive
	bool first = true;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result = (*it)->derive("", first?comment:"", first?unit:"", partType, divisor, values, fields);
		if (result != RESULT_OK)
			return result;
		first = false;
	}

	return RESULT_OK;
}

bool DataFieldSet::hasField(const char* fieldName, bool numeric)
{
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->hasField(fieldName, numeric)==0)
			return true;
	}
	return false;
}

void DataFieldSet::dump(ostream& output)
{
	bool first = true;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		if (first)
			first = false;
		else
			output << FIELD_SEPARATOR;
		(*it)->dump(output);
	}
}

result_t DataFieldSet::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		unsigned int& output, const char* fieldName, signed char fieldIndex)
{
	bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result = field->read(partType, data, offset, output, fieldName, fieldIndex);

		if (result < RESULT_OK)
			return result;

		offset = (unsigned char)(offset + field->getLength(partType, (unsigned char)(data.size()-offset)));
		previousFullByteOffset = field->hasFullByteOffset(true);
		if (result != RESULT_EMPTY) {
			found = true;
		}
		if (findFieldIndex && fieldName == field->getName()) {
			if (fieldIndex == 0) {
				if (!found)
					return RESULT_ERR_NOTFOUND;
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

result_t DataFieldSet::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, OutputFormat outputFormat, signed char outputIndex,
		bool leadingSeparator, const char* fieldName, signed char fieldIndex)
{
	bool previousFullByteOffset = true, found = false, findFieldIndex = fieldName != NULL && fieldIndex >= 0;
	if (!m_uniqueNames && outputIndex<0)
		outputIndex = 0;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType) {
			if (outputIndex>=0 && !field->isIgnored())
				outputIndex++;
			continue;
		}
		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result = field->read(partType, data, offset, output, outputFormat, outputIndex, leadingSeparator, fieldName, fieldIndex);

		if (result < RESULT_OK)
			return result;

		offset = (unsigned char)(offset + field->getLength(partType, (unsigned char)(data.size()-offset)));
		previousFullByteOffset = field->hasFullByteOffset(true);
		if (result != RESULT_EMPTY) {
			found = true;
			leadingSeparator = true;
		}
		if (findFieldIndex && fieldName == field->getName()) {
			if (fieldIndex == 0) {
				if (!found)
					return RESULT_ERR_NOTFOUND;
				break;
			}
			fieldIndex--;
		}
		if (outputIndex>=0 && !field->isIgnored())
			outputIndex++;
	}

	if (!found) {
		return RESULT_EMPTY;
	}
	if (m_comment.length() > 0 && (outputFormat & OF_VERBOSE)) {
		if (outputFormat & OF_JSON)
			output << ",\"comment\": \"" << m_comment << '"';
		else
			output << " [" << m_comment << "]";
	}

	return RESULT_OK;
}

result_t DataFieldSet::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator, unsigned char* length)
{
	string token;

	bool previousFullByteOffset = true;
	unsigned char baseOffset = offset;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (!previousFullByteOffset && !field->hasFullByteOffset(false))
			offset--;

		result_t result;
		unsigned char fieldLength;
		if (m_fields.size() > 1) {
			if (field->isIgnored())
				token.clear();
			else if (!getline(input, token, separator))
				token.clear();

			istringstream single(token);
			result = (*it)->write(single, partType, data, offset, separator, &fieldLength);
		}
		else
			result = (*it)->write(input, partType, data, offset, separator, &fieldLength);

		if (result != RESULT_OK)
			return result;

		offset = (unsigned char)(offset+fieldLength);
		previousFullByteOffset = field->hasFullByteOffset(true);
	}

	if (length!=NULL)
		*length = (unsigned char)(offset-baseOffset);
	return RESULT_OK;
}


DataFieldTemplates::DataFieldTemplates(DataFieldTemplates& other)
	: FileReader::FileReader(false)
{
	for (map<string, DataField*>::iterator it = other.m_fieldsByName.begin(); it != other.m_fieldsByName.end(); it++) {
		m_fieldsByName[it->first] = it->second->clone();
	}
}

void DataFieldTemplates::clear()
{
	for (map<string, DataField*>::iterator it = m_fieldsByName.begin(); it != m_fieldsByName.end(); it++) {
		delete it->second;
		it->second = NULL;
	}
	m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(DataField* field, string name, bool replace)
{
	if (name.length() == 0)
		name = field->getName();
	map<string, DataField*>::iterator it = m_fieldsByName.find(name);
	if (it != m_fieldsByName.end()) {
		if (!replace)
			return RESULT_ERR_DUPLICATE_NAME; // duplicate key

		delete it->second;
		it->second = field;

		return RESULT_OK;
	}

	m_fieldsByName[name] = field;

	return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
	vector< vector<string> >* defaults, const string& defaultDest, const string& defaultCircuit, const string& defaultSuffix,
	const string& filename, unsigned int lineNo)
{
	vector<string>::iterator restart = begin;
	DataField* field = NULL;
	string name;
	if (begin != end) {
		size_t colon = begin->find(':');
		if (colon!=string::npos) {
			name = begin->substr(0, colon);
			begin->erase(0, colon+1);
		}
	}
	result_t result = DataField::create(begin, end, this, field, false, true, false);
	if (result != RESULT_OK)
		return result;

	result = add(field, name, true);
	if (result==RESULT_ERR_DUPLICATE_NAME)
		begin = restart+1; // mark name as invalid
	if (result != RESULT_OK)
		delete field;

	return result;
}

DataField* DataFieldTemplates::get(const string name)
{
	map<string, DataField*>::const_iterator ref = m_fieldsByName.find(name);
	if (ref == m_fieldsByName.end())
		return NULL;

	return ref->second;
}

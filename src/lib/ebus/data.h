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

#ifndef LIBEBUS_DATA_H_
#define LIBEBUS_DATA_H_

#include "symbol.h"
#include "result.h"
#include <string>
#include <vector>
#include <map>

/** the message part in which a data field is stored. */
enum PartType {
	pt_masterData, // stored in master data
	pt_slaveData,  // stored in slave data
	};

/** the available base data types. */
enum BaseType {
	bt_str,    // text string in a StringDataField
	bt_hexstr, // hex digit string in a StringDataField
	bt_dat,    // date in a StringDataField
	bt_tim,    // time in a StringDataField
	bt_num,    // numeric value in a NumericDataField
};

/** flags for dataType_t. */
const unsigned int ADJ = 0x01; // adjustable length, numBytes is maximum length
const unsigned int BCD = 0x02; // binary representation is BCD
const unsigned int REV = 0x04; // reverted binary representation (most significant byte first)
const unsigned int SIG = 0x08; // signed value
const unsigned int LST = 0x10; // value list is possible (without applied divisor)
const unsigned int DAY = 0x20; // default value list is week days

/** the structure for defining field types with their properties. */
typedef struct {
	const char* name;                    // field identifier
	const unsigned int numBytes;         // number of bytes (maximum length if ADJ flag is set)
	const BaseType type;                 // base data type
	const unsigned int flags;            // flags (e.g. BCD)
	const unsigned int replacement;      // replacement value (fill-up value for bt_str/bt_hexstr)
	const unsigned int minValueOrLength; // minimum binary value (minimum length of string for StringDataField)
	const unsigned int maxValueOrLength; // maximum binary value (maximum length of string for StringDataField)
	const unsigned int divisor;          // divisor for bt_number values (or 0 for non-numeric)
} dataType_t;


/**
 * @brief Base class for all kinds of data fields.
 */
class DataField
{
public:
	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param dataType the data type definition.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 */
	DataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const dataType_t dataType, const std::string unit,
			const std::string comment)
		: m_name(name), m_partType(partType), m_offset(offset),
		  m_length(length), m_dataType(dataType), m_unit(unit),
		  m_comment(comment) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~DataField() {}

	/**
	 * @brief Factory method for creating a new instance.
	 * @param dstAddress the destination bus address.
	 * @param isSetMessage whether the field is part of a set message.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param predefined a map of predefined DataFields to be referenced by name.
	 * @param fields the vector to which created instances are added.
	 * @param nextPos the variable holding the next subsequent position.
	 * @return RESULT_OK on success, RESULT_ERR_EOF if the iterator is empty, or an error code.
	 * Note: the caller needs to cleanup created instances.
	 */
	static result_t create(const unsigned char dstAddress, const bool isSetMessage,
			std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end,
			const std::map<std::string, DataField*> predefined, std::vector<DataField*>& fields,
			unsigned char& nextPos);

	/**
	 * @brief Reads the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the ostringstream to append the formatted value to.
	 * @param vervose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
			bool verbose=false);
	/**
	 * @brief Writes the value to the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @param value the formatted value as string.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t write(const std::string& value, SymbolString& masterData, SymbolString& slaveData);

protected:
	/**
	 * @brief Internal method for reading the field from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param output the ostringstream to append the formatted value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t readSymbols(SymbolString& input, std::ostringstream& output) = 0;
	/**
	 * @brief Internal method for writing the field to a @a SymbolString.
	 * @param input the istringstream to parse the formatted value from.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t writeSymbols(std::istringstream& input, SymbolString& output) = 0;

	/** the field name. */
	const std::string m_name;
	/** the message part in which the field is stored. */
	const PartType m_partType;
	/** the offset to the first symbol in the message part in which the field is stored. */
	const unsigned char m_offset;
	/** the number of symbols in the message part in which the field is stored. */
	const unsigned char m_length;
	/** the data type definition. */
	const dataType_t m_dataType;
	/** the value unit. */
	const std::string m_unit;
	/** the field comment. */
	const std::string m_comment;
};


/**
 * @brief Base class for all string based data fields.
 */
class StringDataField : public DataField
{
public:
	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param dataType the data type definition.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 */
	StringDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const dataType_t dataType, const std::string unit,
			const std::string comment)
		: DataField(name, partType, offset, length, dataType, unit, comment) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~StringDataField() {}

protected:
	virtual result_t readSymbols(SymbolString& input, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, SymbolString& output);

};


/**
 * @brief Base class for all numeric data fields.
 */
class NumericDataField : public DataField
{
public:
	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param dataType the data type definition.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 */
	NumericDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const dataType_t dataType, const std::string unit,
			const std::string comment)
		: DataField(name, partType, offset, length, dataType, unit, comment) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~NumericDataField() {}

protected:
	/**
	 * @brief Internal method for reading the raw value from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param value the variable in which to store the raw value.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t readRawValue(SymbolString& input, unsigned int& value);
	/**
	 * @brief Internal method for writing the raw value to a @a SymbolString.
	 * @param value the raw value to write.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t writeRawValue(unsigned int value, SymbolString& output);

};

/**
 * @brief Base class for all numeric data fields with a number representation.
 */
class NumberDataField : public NumericDataField
{
public:
	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param dataType the data type definition.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 * @param divisor the extra divisor to apply on the value, or 1 for none.
	 */
	NumberDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const dataType_t dataType, const std::string unit,
			const std::string comment, const unsigned int divisor)
		: NumericDataField(name, partType, offset, length, dataType, unit, comment),
		  m_divisor(divisor * dataType.divisor) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~NumberDataField() {}

protected:
	virtual result_t readSymbols(SymbolString& input, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, SymbolString& output);

	/** the combined divisor to apply on the value, or 1 for none. */
	const unsigned int m_divisor;

};

/**
 * @brief A numeric data field with a list of value=text assignments and a string representation.
 */
class ValueListDataField : public NumericDataField
{
public:
	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param dataType the data type definition.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 * @param values the value=text assignments.
	 */
	ValueListDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const dataType_t dataType, const std::string unit,
			const std::string comment, const std::map<unsigned int, std::string> values)
		: NumericDataField(name, partType, offset, length, dataType, unit, comment),
		  m_values(values) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~ValueListDataField() {}

protected:
	virtual result_t readSymbols(SymbolString& input, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, SymbolString& output);

	/** the value=text assignments. */
	std::map<unsigned int, std::string> m_values;

};

#endif // LIBEBUS_DATA_H_

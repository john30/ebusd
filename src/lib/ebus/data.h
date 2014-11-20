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
	pt_template,   // special part type for templates (relative offset)
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
const unsigned int ADJ = 0x01; // adjustable length, numBits is maximum length
const unsigned int BCD = 0x02; // binary representation is BCD
const unsigned int REV = 0x04; // reverted binary representation (most significant byte first)
const unsigned int SIG = 0x08; // signed value
const unsigned int LST = 0x10; // value list is possible (without applied divisor)
const unsigned int DAY = 0x20; // forced value list defaulting to week days

/** the structure for defining field types with their properties. */
typedef struct {
	const char* name;                        // field identifier
	const unsigned int numBits;              // number of bits (maximum length if ADJ flag is set, must be multiple of 8 with flag BCD)
	const BaseType type;                     // base data type
	const unsigned int flags;                // flags (e.g. BCD)
	const unsigned int replacement;          // replacement value (fill-up value for bt_str/bt_hexstr, no replacement if equal to minValueOrLength for bt_num)
	const unsigned int minValueOrLength;     // minimum binary value (minimum length of string for StringDataField)
	const unsigned int maxValueOrLength;     // maximum binary value (maximum length of string for StringDataField)
	const unsigned int divisor;              // bt_number: divisor
	const unsigned char precisionOrFirstBit; // bt_number: precision for formatting or offset to first bit if (numBits%8)!=0
} dataType_t;


class SingleDataField;

/**
 * @brief Base class for all kinds of data fields.
 */
class DataField
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 */
	DataField(const std::string name, const std::string comment)
		: m_name(name), m_comment(comment) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~DataField() {}
	/**
	 * @brief Factory method for creating new instances.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param templates a map of DataField templates to be referenced by name.
	 * @param fields the vector to which created instances are added.
	 * @param isSetMessage whether the field is part of a set message (default false).
	 * @param dstAddress the destination bus address (default @a SYN for creating a template DataFields).
	 * @return RESULT_OK on success, RESULT_ERR_EOF if the iterator is empty, or an error code.
	 * Note: the caller needs to cleanup created instances.
	 */
	static result_t create(std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end,
			const std::map< std::string, DataField*> templates, DataField*& fields,
			const bool isSetMessage=false, const unsigned char dstAddress=SYN);
	/**
	 * @brief Returns the offset to the first symbol in the message part for a field following this field.
	 * @return the offset to the first symbol in the message part for a field following this field.
	 */
	virtual unsigned char getNextOffset() = 0;
	/**
	 * @brief Derives a new DataField from this field.
	 * @param name the field name.
	 * @param comment the field comment, or empty to use this fields comment.
	 * @param unit the value unit, or empty to use this fields unit (if applicable).
	 * @param partType the message part in which the field is stored.
	 * @param offset the (additional) offset to the first symbol in the message part in which the field is stored.
	 * @param divisor the extra divisor to apply on the value, or 1 for none (if applicable).
	 * @param values the value=text assignments, or empty to use this fields assignments (if applicable).
	 */
	virtual result_t derive(std::string name, std::string comment,
			std::string unit, const PartType partType, unsigned char offset,
			unsigned int divisor, std::map<unsigned int, std::string> values,
			std::vector<SingleDataField*>& fields) = 0;
	/**
	 * @brief Get the field name.
	 * @return the field name.
	 */
	const std::string getName() { return m_name; }
	/**
	 * @brief Get the field comment.
	 * @return the field comment.
	 */
	const std::string getComment() { return m_comment; }
	/**
	 * @brief Reads the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the ostringstream to append the formatted value to.
	 * @param verbose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @param separator the separator character between multiple fields.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
			bool verbose=false, char separator=';') = 0;
	/**
	 * @brief Writes the value to the master or slave @a SymbolString.
	 * @param input the istringstream to parse the formatted value from.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @param separator the separator character between multiple fields.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t write(std::istringstream& input, SymbolString& masterData, SymbolString& slaveData,
			char separator=';') = 0;

protected:

	/** the field name. */
	const std::string m_name;
	/** the field comment. */
	const std::string m_comment;

};


/**
 * @brief A single DataField.
 */
class SingleDataField : public DataField
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 */
	SingleDataField(const std::string name, const std::string comment,
			const std::string unit, const dataType_t dataType, const PartType partType,
			const unsigned char offset, const unsigned char length)
		: DataField(name, comment),
		  m_unit(unit), m_dataType(dataType), m_partType(partType),
		  m_offset(offset), m_length(length) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~SingleDataField() {}
	/**
	 * @brief Get the value unit.
	 * @return the value unit.
	 */
	const std::string getUnit() { return m_unit; }
	virtual unsigned char getNextOffset();
	/**
	 * @brief Reads the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the ostringstream to append the formatted value to.
	 * @param vervose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
			bool verbose=false, char separator=';');
	/**
	 * @brief Writes the value to the master or slave @a SymbolString.
	 * @param input the istringstream to parse the formatted value from.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t write(std::istringstream& input, SymbolString& masterData, SymbolString& slaveData,
			char separator=';');

protected:

	/**
	 * @brief Internal method for reading the field from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param output the ostringstream to append the formatted value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t readSymbols(SymbolString& input, unsigned char baseOffset, std::ostringstream& output) = 0;
	/**
	 * @brief Internal method for writing the field to a @a SymbolString.
	 * @param input the istringstream to parse the formatted value from.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t writeSymbols(std::istringstream& input, unsigned char baseOffset, SymbolString& output) = 0;

	/** the value unit. */
	const std::string m_unit;
	/** the data type definition. */
	const dataType_t m_dataType;
	/** the message part in which the field is stored. */
	const PartType m_partType;
	/** the relative offset to the first symbol in the message part in which the field is stored. */
	const unsigned char m_offset;
	/** the number of symbols in the message part in which the field is stored. */
	const unsigned char m_length;

};


/**
 * @brief Base class for all string based data fields.
 */
class StringDataField : public SingleDataField
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 */
	StringDataField(const std::string name, const std::string comment,
			const std::string unit, const dataType_t dataType, const PartType partType,
			const unsigned char offset, const unsigned char length)
		: SingleDataField(name, comment, unit, dataType, partType, offset, length) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~StringDataField() {}
	virtual result_t derive(std::string name, std::string comment,
			std::string unit, const PartType partType, unsigned char offset,
			unsigned int divisor, std::map<unsigned int, std::string> values,
			std::vector<SingleDataField*>& fields);

protected:

	virtual result_t readSymbols(SymbolString& input, unsigned char baseOffset, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, unsigned char baseOffset, SymbolString& output);

};


/**
 * @brief Base class for all numeric data fields.
 */
class NumericDataField : public SingleDataField
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param bitOffset the offset to the first bit in the binary value.
	 */
	NumericDataField(const std::string name, const std::string comment,
			const std::string unit, const dataType_t dataType, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const unsigned char bitOffset)
		: SingleDataField(name, comment, unit, dataType, partType, offset, length),
  		  m_bitOffset(bitOffset) {}
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
	result_t readRawValue(SymbolString& input, unsigned char baseOffset, unsigned int& value);
	/**
	 * @brief Internal method for writing the raw value to a @a SymbolString.
	 * @param value the raw value to write.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t writeRawValue(unsigned int value, unsigned char baseOffset, SymbolString& output);

	/** the offset to the first bit in the binary value. */
	const unsigned char m_bitOffset;

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
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param divisor the extra divisor to apply on the value, or 1 for none.
	 */
	NumberDataField(const std::string name, const std::string comment,
			const std::string unit, const dataType_t dataType, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const unsigned int divisor)
		: NumericDataField(name, comment, unit, dataType, partType, offset, length,
				(dataType.numBits%8) != 0 ? dataType.precisionOrFirstBit : 0),
		m_divisor(divisor) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~NumberDataField() {}

protected:

	virtual result_t derive(std::string name, std::string comment,
			std::string unit, const PartType partType, unsigned char offset,
			unsigned int divisor, std::map<unsigned int, std::string> values,
			std::vector<SingleDataField*>& fields);
	virtual result_t readSymbols(SymbolString& input, unsigned char baseOffset, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, unsigned char baseOffset, SymbolString& output);

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
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param offset the offset to the first symbol in the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param values the value=text assignments.
	 */
	ValueListDataField(const std::string name, const std::string comment,
			const std::string unit, const dataType_t dataType, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const std::map<unsigned int, std::string> values)
		: NumericDataField(name, comment, unit, dataType, partType, offset, length,
				(dataType.numBits%8) != 0 ? dataType.precisionOrFirstBit : 0),
		m_values(values) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~ValueListDataField() {}

protected:

	virtual result_t derive(std::string name, std::string comment,
			std::string unit, const PartType partType, unsigned char offset,
			unsigned int divisor, std::map<unsigned int, std::string> values,
			std::vector<SingleDataField*>& fields);
	virtual result_t readSymbols(SymbolString& input, unsigned char baseOffset, std::ostringstream& output);
	virtual result_t writeSymbols(std::istringstream& input, unsigned char baseOffset, SymbolString& output);

	/** the value=text assignments. */
	std::map<unsigned int, std::string> m_values;

};


/**
 * @brief A set of DataFields.
 */
class DataFieldSet : public DataField
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param fields the list of SingleDataFields part of this set.
	 */
	DataFieldSet(const std::string name, const std::string comment,
			const std::vector<SingleDataField*> fields)
		: DataField(name, comment),
		  m_fields(fields) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~DataFieldSet();
	virtual unsigned char getNextOffset();
	virtual result_t derive(std::string name, std::string comment,
			std::string unit, const PartType partType, unsigned char offset,
			unsigned int divisor, std::map<unsigned int, std::string> values,
			std::vector<SingleDataField*>& fields);
	/**
	 * @brief Returns the SingleDataField at the specified index.
	 * @param index the index of the SingleDataField to return.
	 * @return the SingleDataField at the specified index, or NULL.
	 */
	SingleDataField* operator[](const size_t index) { if (index >= m_fields.size()) return NULL; return m_fields[index]; }
	/**
	 * @brief Returns the SingleDataField at the specified index.
	 * @param index the index of the SingleDataField to return.
	 * @return the SingleDataField at the specified index, or NULL.
	 */
	const SingleDataField* operator[](const size_t index) const { if (index >= m_fields.size()) return NULL; return m_fields[index]; }
	/**
	 * @brief Returns the number of SingleDataFields in this set.
	 * @return the number of available SingleDataField.
	 */
	size_t size() const { return m_fields.size(); }
	/**
	 * @brief Reads the values from the master and/or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the ostringstream to append the formatted value to.
	 * @param vervose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
			bool verbose=false, char separator=';');
	/**
	 * @brief Writes the values to the master and/or slave @a SymbolString.
	 * @param input the istringstream to parse the formatted value from.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @return RESULT_OK on success, or an error code.
	 */
	virtual result_t write(std::istringstream& input, SymbolString& masterData, SymbolString& slaveData,
			char separator=';');

protected:

	/** the list of SingleDataFields part of this set. */
	std::vector<SingleDataField*> m_fields;

};


#endif // LIBEBUS_DATA_H_

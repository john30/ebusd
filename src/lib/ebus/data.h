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

#ifndef LIBEBUS_DATA_H_
#define LIBEBUS_DATA_H_

#include "symbol.h"
#include "result.h"
#include "filereader.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

/** @file data.h
 * Classes, functions, and constants related to decoding/encoding of symbols
 * on the eBUS to/from readable values.
 *
 * A @a DataField is either a @a SingleDataField or a list of
 * @a SingleDataField instances in a @a DataFieldSet.
 *
 * A @a SingleDataField is either a string based one or a numeric one. Due to
 * their text nature, date and time fields are also treated as
 * @a StringDataField.
 *
 * The basic field types are just @a StringDataField, @a NumberDataField, and
 * @a ValueListDataField. The particular eBUS specification types like e.g.
 * @a D1C are defined by using one of these basic field types (see #BaseType)
 * with certain flags, such as #BCD, #FIX, #REQ, see #dataType_s.
 *
 * The list of available base types is defined an array of #dataType_s
 * structures and can easily be extended if necessary.
 *
 * Each @a DataField can be converted from a @a SymbolString to an
 * @a ostringstream (see @a DataField#read() methods) or vice versa from an
 * @a istringstream to a @a SymbolString (see @a DataField#write()).
 *
 * The @a DataFieldTemplates allow definition of derived types as well as
 * combined types based on the list of available base types. It reads the
 * instances from configuration files by inheriting the @a FileReader template
 * class.
 */

using namespace std;

/** the separator character used between multiple values (in CSV only). */
#define VALUE_SEPARATOR ';'

/** the separator character used between base type name and length (in CSV only). */
#define LENGTH_SEPARATOR ':'

/** the replacement string for undefined values (in UI and CSV). */
#define NULL_VALUE "-"

/** the separator character used between fields (in UI only). */
#define UI_FIELD_SEPARATOR ';'

/** the type for data output format options. */
typedef int OutputFormat;

/* the bit flags for @a OutputFormat. */
static const unsigned int OF_VERBOSE = 0x01; //!< verbose format (names, values, units, and comments).
static const unsigned int OF_NUMERIC = 0x02; //!< numeric format (keep numeric value of value=name pairs).
static const unsigned int OF_JSON = 0x04;    //!< JSON format.

/** the message part in which a data field is stored. */
enum PartType {
	pt_any,          //!< stored in any data (master or slave)
	pt_masterData,   //!< stored in master data
	pt_slaveData,    //!< stored in slave data
};

/** the available base data types. */
enum BaseType {
	bt_str,    //!< text string in a @a StringDataField
	bt_hexstr, //!< hex digit string in a @a StringDataField
	bt_dat,    //!< date in a @a StringDataField
	bt_tim,    //!< time in a @a StringDataField
	bt_num,    //!< numeric value in a @a NumericDataField
};

/* flags for dataType_t. */
static const unsigned int ADJ = 0x01; //!< adjustable length, bitCount is maximum length
static const unsigned int BCD = 0x02; //!< binary representation is BCD
static const unsigned int REV = 0x04; //!< reverted binary representation (most significant byte first)
static const unsigned int SIG = 0x08; //!< signed value
static const unsigned int LST = 0x10; //!< value list is possible (without applied divisor)
static const unsigned int DAY = 0x20; //!< forced value list defaulting to week days
static const unsigned int IGN = 0x40; //!< ignore value during read and write
static const unsigned int FIX = 0x80; //!< fixed width formatting
static const unsigned int REQ = 0x100;//!< value may not be NULL
static const unsigned int HCD = 0x200; //!< binary representation is hex converted to decimal and interpreted as 2 digits (also requires #BCD)

/** The structure for defining data types with their properties. */
typedef struct dataType_s {
	const char* name;               //!< data type identifier
	const unsigned char bitCount;   //!< number of bits (maximum length if #ADJ flag is set, must be multiple of 8 with flag #BCD)
	const BaseType type;            //!< base data type
	const unsigned short flags;     //!< flags (like #BCD)
	const unsigned int replacement; //!< replacement value (fill-up value for #bt_str / #bt_hexstr, no replacement if equal to #minValue for #bt_num)
	const unsigned int minValue;    //!< minimum binary value (ignored for @a StringDataField)
	const unsigned int maxValue;    //!< maximum binary value (ignored for @a StringDataField)
	const short divisorOrFirstBit;  //!< #bt_num: divisor (negative for reciprocal) or offset to first bit (if (#bitCount%8)!=0)
} dataType_t;

/** the maximum allowed position within master or slave data. */
#define MAX_POS 24

/** the maximum allowed field length. */
#define MAX_LEN 31

/**
 * Parse an unsigned int value.
 * @param str the string to parse.
 * @param base the numerical base.
 * @param minValue the minimum resulting value.
 * @param maxValue the maximum resulting value.
 * @param result the variable in which to store an error code when parsing failed or the value is out of bounds.
 * @param length the optional variable in which to store the number of read characters.
 * @return the parsed value.
 */
unsigned int parseInt(const char* str, int base, const unsigned int minValue, const unsigned int maxValue, result_t& result, unsigned int* length=NULL);

/**
 * Parse a signed int value.
 * @param str the string to parse.
 * @param base the numerical base.
 * @param minValue the minimum resulting value.
 * @param maxValue the maximum resulting value.
 * @param result the variable in which to store an error code when parsing failed or the value is out of bounds.
 * @param length the optional variable in which to store the number of read characters.
 * @return the parsed value.
 */
int parseSignedInt(const char* str, int base, const int minValue, const int maxValue, result_t& result, unsigned int* length=NULL);

/**
 * Print the error position of the iterator.
 * @param out the @a ostream to print to.
 * @param begin the iterator to the beginning of the items.
 * @param end the iterator to the end of the items.
 * @param pos the iterator with the erroneous position.
 * @param filename the name of the file being read.
 * @param lineNo the current line number in the file being read.
 * @param result the result code.
 */
void printErrorPos(ostream& out, vector<string>::iterator begin, const vector<string>::iterator end, vector<string>::iterator pos, string filename, size_t lineNo, result_t result);


class DataFieldTemplates;
class SingleDataField;

/**
 * Base class for all kinds of data fields.
 */
class DataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 */
	DataField(const string name, const string comment)
		: m_name(name), m_comment(comment) {}

	/**
	 * Destructor.
	 */
	virtual ~DataField() {}

	/**
	 * Clone this instance.
	 * @return a clone of this instance.
	 */
	virtual DataField* clone() = 0;

	/**
	 * Factory method for creating new instances.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param templates the @a DataFieldTemplates to be referenced by name, or NULL.
	 * @param returnField the variable in which to store the created instance.
	 * @param isWriteMessage whether the field is part of a write message (default false).
	 * @param isTemplate true for creating a template @a DataField.
	 * @param isBroadcastOrMasterDestination true if the destination bus address is @a BRODCAST or a master address.
	 * @param maxFieldLength the maximum allowed length of a single field (default @a MAX_POS).
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(vector<string>::iterator& it, const vector<string>::iterator end,
			DataFieldTemplates* templates, DataField*& returnField,
			const bool isWriteMessage,
			const bool isTemplate, const bool isBroadcastOrMasterDestination,
			const unsigned char maxFieldLength=MAX_POS);

	/**
	 * Dump the @a string optionally embedded in @a TEXT_SEPARATOR to the output.
	 * @param output the @a ostream to dump to.
	 * @param str the @a string to dump.
	 * @param prependFieldSeparator whether to start with a @a FIELD_SEPARATOR.
	 */
	static void dumpString(ostream& output, const string str, const bool prependFieldSeparator=true);

	/**
	 * Returns the length of this field (or contained fields) in bytes.
	 * @param partType the message part of the contained fields to limit the length calculation to.
	 * @return the length of this field (or contained fields) in bytes.
	 */
	virtual unsigned char getLength(PartType partType) = 0;

	/**
	 * Derive a new @a DataField from this field.
	 * @param name the field name, or empty to use this fields name.
	 * @param comment the field comment, or empty to use this fields comment.
	 * @param unit the value unit, or empty to use this fields unit (if applicable).
	 * @param partType the message part in which the field is stored.
	 * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none (if applicable).
	 * @param values the value=text assignments, or empty to use this fields assignments (if applicable).
	 * @param fields the @a vector to which created @a SingleDataField instances shall be added.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType,
			int divisor, map<unsigned int, string> values,
			vector<SingleDataField*>& fields) = 0;

	/**
	 * Get the field name.
	 * @return the field name.
	 */
	string getName() const { return m_name; }

	/**
	 * Get the field comment.
	 * @return the field comment.
	 */
	string getComment() const { return m_comment; }

	/**
	 * Dump the field settings to the output.
	 * @param output the @a ostream to dump to.
	 */
	virtual void dump(ostream& output) = 0;

	/**
	 * Return whether the field is available.
	 * @param fieldName the name of the field to find, or NULL for any.
	 * @param numeric true for a numeric field, false for a string field.
	 * @return true if the field is available.
	 */
	virtual bool hasField(const char* fieldName, bool numeric) = 0;

	/**
	 * Reads the numeric value from the @a SymbolString.
	 * @param partType the @a PartType of the data.
	 * @param data the unescaped data @a SymbolString for reading binary data.
	 * @param offset the additional offset to add for reading binary data.
	 * @param output the variable in which to store the numeric value.
	 * @param fieldName the name of the field to read, or NULL for the first field.
	 * @param fieldIndex the optional index of the named field, or -1.
	 * @return @a RESULT_OK on success,
	 * or @a RESULT_EMPTY if the field was skipped (either if the partType does
	 * not match or ignored, or due to @a fieldName or @a fieldIndex),
	 * or an error code.
	 */
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			unsigned int& output, const char* fieldName=NULL, signed char fieldIndex=-1) = 0;

	/**
	 * Reads the value from the @a SymbolString.
	 * @param partType the @a PartType of the data.
	 * @param data the unescaped data @a SymbolString for reading binary data.
	 * @param offset the additional offset to add for reading binary data.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @param outputIndex the optional index of the field when using an indexed output format, or -1.
	 * @param leadingSeparator whether to prepend a separator before the formatted value.
	 * @param fieldName the optional name of a field to limit the output to.
	 * @param fieldIndex the optional index of the named field to limit the output to, or -1.
	 * @return @a RESULT_OK on success (or if the partType does not match),
	 * or @a RESULT_EMPTY if the field was skipped (either ignored or due to @a fieldName or @a fieldIndex),
	 * or an error code.
	 */
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			ostringstream& output, OutputFormat outputFormat, signed char outputIndex=-1,
			bool leadingSeparator=false, const char* fieldName=NULL, signed char fieldIndex=-1) = 0;

	/**
	 * Writes the value to the master or slave @a SymbolString.
	 * @param input the @a istringstream to parse the formatted value from.
	 * @param partType the @a PartType of the data.
	 * @param data the unescaped data @a SymbolString for writing binary data.
	 * @param offset the additional offset to add for writing binary data.
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t write(istringstream& input,
			const PartType partType, SymbolString& data,
			unsigned char offset, char separator=UI_FIELD_SEPARATOR) = 0;

protected:

	/** the field name. */
	const string m_name;

	/** the field comment. */
	const string m_comment;

};


/**
 * A single @a DataField holding a value.
 */
class SingleDataField : public DataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 */
	SingleDataField(const string name, const string comment,
			const string unit, const dataType_t dataType, const PartType partType,
			const unsigned char length)
		: DataField(name, comment),
		  m_unit(unit), m_dataType(dataType), m_partType(partType),
		  m_length(length) {}

	/**
	 * Destructor.
	 */
	virtual ~SingleDataField() {}

	// @copydoc
	virtual SingleDataField* clone() = 0;

	/**
	 * Factory method for creating a new @a SingleDataField instance derived from a base type.
	 * @param typeNameStr the base type name string.
	 * @param length the base type length, or 0 for default.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param partType the message part in which the field is stored.
	 * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none (if applicable).
	 * @param values the value=text assignments.
	 * @param returnField the variable in which the created @a SingleDataField instance shall be stored.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(const char* typeNameStr, const unsigned char length,
		const string name, const string comment, const string unit,
		const PartType partType, int divisor, map<unsigned int, string> values,
		SingleDataField* &returnField);

	/**
	 * Get the value unit.
	 * @return the value unit.
	 */
	string getUnit() const { return m_unit; }

	/**
	 * Get whether this field is ignored.
	 * @return whether this field is ignored.
	 */
	bool isIgnored() const { return (m_dataType.flags & IGN) != 0; }

	/**
	 * Get the message part in which the field is stored.
	 * @return the message part in which the field is stored.
	 */
	PartType getPartType() const { return m_partType; }

	// @copydoc
	virtual unsigned char getLength(PartType partType) { return partType == m_partType ? m_length : (unsigned char)0; };
	// re-use same position as previous field as not all bits of fully consumed yet

	/**
	 * Get whether this field uses a full byte offset.
	 * @param after @p true to check after consuming the bits, false to check before.
	 * @return true if this field uses a full byte offset, false if this field
	 * only consumes a part of a byte and a subsequent field may re-use the same offset.
	 */
	virtual bool hasFullByteOffset(bool after) { return true; }

	// @copydoc
	virtual void dump(ostream& output);

	// @copydoc
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			unsigned int& output, const char* fieldName=NULL, signed char fieldIndex=-1);

	// @copydoc
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			ostringstream& output, OutputFormat outputFormat, signed char outputIndex=-1,
			bool leadingSeparator=false, const char* fieldName=NULL, signed char fieldIndex=-1);

	// @copydoc
	virtual result_t write(istringstream& input,
			const PartType partType, SymbolString& data,
			unsigned char offset, char separator=UI_FIELD_SEPARATOR);

protected:

	/**
	 * Internal method for reading the numeric raw value from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param offset the offset in the @a SymbolString.
	 * @param value the variable in which to store the numeric raw value.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t readRawValue(SymbolString& input, const unsigned char offset, unsigned int& value) = 0;

	/**
	 * Internal method for reading the field from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param baseOffset the base offset in the @a SymbolString.
	 * @param output the ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t readSymbols(SymbolString& input, const unsigned char baseOffset,
			ostringstream& output, OutputFormat outputFormat) = 0;

	/**
	 * Internal method for writing the field to a @a SymbolString.
	 * @param input the @a istringstream to parse the formatted value from.
	 * @param offset the offset in the @a SymbolString.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t writeSymbols(istringstream& input, const unsigned char offset, SymbolString& output) = 0;

protected:

	/** the value unit. */
	const string m_unit;

	/** the data type definition. */
	const dataType_t m_dataType;

	/** the message part in which the field is stored. */
	const PartType m_partType;

	/** the number of symbols in the message part in which the field is stored. */
	const unsigned char m_length;

};


/**
 * Base class for all string based data fields.
 */
class StringDataField : public SingleDataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 */
	StringDataField(const string name, const string comment,
			const string unit, const dataType_t dataType, const PartType partType,
			const unsigned char length)
		: SingleDataField(name, comment, unit, dataType, partType, length) {}

	/**
	 * Destructor.
	 */
	virtual ~StringDataField() {}

	// @copydoc
	virtual StringDataField* clone();

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType,
			int divisor, map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	// @copydoc
	virtual bool hasField(const char* fieldName, bool numeric);

	// @copydoc
	virtual void dump(ostream& output);

protected:

	// @copydoc
	virtual result_t readRawValue(SymbolString& input, const unsigned char offset, unsigned int& value);

	// @copydoc
	virtual result_t readSymbols(SymbolString& input, const unsigned char baseOffset,
			ostringstream& output, OutputFormat outputFormat);

	// @copydoc
	virtual result_t writeSymbols(istringstream& input, const unsigned char offset, SymbolString& output);

};


/**
 * Base class for all numeric data fields.
 */
class NumericDataField : public SingleDataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param bitCount the number of bits in the binary value (may be less than @a length * 8).
	 * @param bitOffset the offset to the first bit in the binary value.
	 */
	NumericDataField(const string name, const string comment,
			const string unit, const dataType_t dataType, const PartType partType,
			const unsigned char length, const unsigned char bitCount, const unsigned char bitOffset)
		: SingleDataField(name, comment, unit, dataType, partType, length),
		  m_bitCount(bitCount), m_bitOffset(bitOffset) {}

	/**
	 * Destructor.
	 */
	virtual ~NumericDataField() {}

	// @copydoc
	virtual NumericDataField* clone() = 0;

	// @copydoc
	virtual bool hasFullByteOffset(bool after);

	// @copydoc
	virtual bool hasField(const char* fieldName, bool numeric);

	// @copydoc
	virtual void dump(ostream& output);

protected:

	// @copydoc
	virtual result_t readRawValue(SymbolString& input, const unsigned char offset, unsigned int& value);

	/**
	 * Internal method for writing the raw value to a @a SymbolString.
	 * @param value the raw value to write.
	 * @param offset the offset in the @a SymbolString.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t writeRawValue(unsigned int value, const unsigned char offset, SymbolString& output);

	/** the number of bits in the binary value. */
	const unsigned char m_bitCount;

	/** the offset to the first bit in the binary value. */
	const unsigned char m_bitOffset;

};


/**
 * Base class for all numeric data fields with a number representation.
 */
class NumberDataField : public NumericDataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param bitCount the number of bits in the binary value (may be less than @a length * 8).
	 * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none.
	 */
	NumberDataField(const string name, const string comment,
			const string unit, const dataType_t dataType, const PartType partType,
			const unsigned char length, const unsigned char bitCount,
			const int divisor);

	/**
	 * Destructor.
	 */
	virtual ~NumberDataField() {}

	// @copydoc
	virtual NumberDataField* clone();

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType,
			int divisor, map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	// @copydoc
	virtual void dump(ostream& output);

protected:

	// @copydoc
	virtual result_t readSymbols(SymbolString& input, const unsigned char baseOffset,
			ostringstream& output, OutputFormat outputFormat);

	// @copydoc
	virtual result_t writeSymbols(istringstream& input, const unsigned char offset, SymbolString& output);

private:

	/** the combined divisor (negative for reciprocal) to apply on the value, or 1 for none. */
	const int m_divisor;

	/** the precision for formatting the value. */
	unsigned char m_precision;

};


/**
 * A numeric data field with a list of value=text assignments and a string representation.
 */
class ValueListDataField : public NumericDataField
{
public:

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param dataType the data type definition.
	 * @param partType the message part in which the field is stored.
	 * @param length the number of symbols in the message part in which the field is stored.
	 * @param bitCount the number of bits in the binary value (may be less than @a length * 8).
	 * @param values the value=text assignments.
	 */
	ValueListDataField(const string name, const string comment,
			const string unit, const dataType_t dataType, const PartType partType,
			const unsigned char length, const unsigned char bitCount,
			const map<unsigned int, string> values)
		: NumericDataField(name, comment, unit, dataType, partType, length, bitCount,
				(unsigned char)((dataType.bitCount < 8) ? dataType.divisorOrFirstBit : 0)),
		m_values(values) {}

	/**
	 * Destructor.
	 */
	virtual ~ValueListDataField() {}

	// @copydoc
	virtual ValueListDataField* clone();

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType, int divisor,
			map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	// @copydoc
	virtual void dump(ostream& output);

protected:

	// @copydoc
	virtual result_t readSymbols(SymbolString& input, const unsigned char baseOffset,
			ostringstream& output, OutputFormat outputFormat);

	// @copydoc
	virtual result_t writeSymbols(istringstream& input, const unsigned char offset, SymbolString& output);

private:

	/** the value=text assignments. */
	map<unsigned int, string> m_values;

};


/**
 * A set of @a DataField instances.
 */
class DataFieldSet : public DataField
{
public:

	/**
	 * Get the @a DataFieldSet for parsing the identification message (service 0x07 0x04).
	 * @return the @a DataFieldSet for parsing the identification message. This is:<ul>
	 * <li>manufacturer name (list of known values)</li>
	 * <li>identification string (5 characters)</li>
	 * <li>software version number (0000-9999)</li>
	 * <li>hardware version number (0000-9999)</li>
	 * </ul>
	 * Note: the returned value may only be deleted once.
	 */
	static DataFieldSet* getIdentFields();

	/**
	 * Constructs a new instance.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param fields the @a vector of @a SingleDataField instances part of this set.
	 */
	DataFieldSet(const string name, const string comment,
			const vector<SingleDataField*> fields)
		: DataField(name, comment),
		  m_fields(fields)
	{
		bool uniqueNames = true;
		map<string, string> names;
		for (vector<SingleDataField*>::const_iterator it=fields.begin(); it!=fields.end(); it++) {
			string name = (*it)->getName();
			if (name.empty() || names.find(name)!=names.end()) {
				uniqueNames = false;
				break;
			}
			names[name] = name;
		}
		m_uniqueNames = uniqueNames;
	}

	/**
	 * Destructor.
	 */
	virtual ~DataFieldSet();

	// @copydoc
	virtual DataFieldSet* clone();

	// @copydoc
	virtual unsigned char getLength(PartType partType);

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType,
			int divisor, map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	/**
	 * Returns the @a SingleDataField at the specified index.
	 * @param index the index of the @a SingleDataField to return.
	 * @return the @a SingleDataField at the specified index, or NULL.
	 */
	SingleDataField* operator[](const size_t index) { if (index >= m_fields.size()) return NULL; return m_fields[index]; }

	/**
	 * Returns the @a SingleDataField at the specified index.
	 * @param index the index of the @a SingleDataField to return.
	 * @return the @a SingleDataField at the specified index, or NULL.
	 */
	const SingleDataField* operator[](const size_t index) const { if (index >= m_fields.size()) return NULL; return m_fields[index]; }

	/**
	 * Returns the number of @a SingleDataFields instances in this set.
	 * @return the number of available @a SingleDataField instances.
	 */
	size_t size() const { return m_fields.size(); }

	// @copydoc
	virtual bool hasField(const char* fieldName, bool numeric);

	// @copydoc
	virtual void dump(ostream& output);

	// @copydoc
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			unsigned int& output, const char* fieldName=NULL, signed char fieldIndex=-1);

	// @copydoc
	virtual result_t read(const PartType partType,
			SymbolString& data, unsigned char offset,
			ostringstream& output, OutputFormat outputFormat, signed char outputIndex=-1,
			bool leadingSeparator=false, const char* fieldName=NULL, signed char fieldIndex=-1);

	// @copydoc
	virtual result_t write(istringstream& input,
			const PartType partType, SymbolString& data,
			unsigned char offset, char separator=UI_FIELD_SEPARATOR);

private:

	/** the @a DataFieldSet containing the ident message @a SingleDataField instances, or NULL. */
	static DataFieldSet* s_identFields;

	/** the @a vector of @a SingleDataField instances part of this set. */
	vector<SingleDataField*> m_fields;

	/** whether all fields have a unique name. */
	bool m_uniqueNames;

};


/**
 * A map of template @a DataField instances.
 */
class DataFieldTemplates : public FileReader
{
public:

	/**
	 * Constructs a new instance.
	 */
	DataFieldTemplates() : FileReader::FileReader(false) {}

	/**
	 * Constructs a new copied instance.
	 * @param other the @a DataFieldTemplates to copy from.
	 */
	DataFieldTemplates(DataFieldTemplates& other);

	/**
	 * Destructor.
	 */
	virtual ~DataFieldTemplates() {
		clear();
	}

	/**
	 * Removes all @a DataField instances.
	 */
	void clear();

	/**
	 * Adds a template @a DataField instance to this map.
	 * @param field the @a DataField instance to add.
	 * @param name the name to use in the map, or the empty string to use the @a DataField name.
	 * @param replace whether replacing an already stored instance is allowed.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller may not free the added instance on success.
	 */
	result_t add(DataField* field, string name="", bool replace=false);

	// @copydoc
	virtual result_t addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
		vector< vector<string> >* defaults,
		const string& filename, unsigned int lineNo);

	/**
	 * Gets the template @a DataField instance with the specified name.
	 * @param name the name of the template to get.
	 * @return the template @a DataField instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	DataField* get(string name);

private:

	/** the known template @a DataField instances by name. */
	map<string, DataField*> m_fieldsByName;

};

#endif // LIBEBUS_DATA_H_

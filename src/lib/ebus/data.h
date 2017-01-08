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

#ifndef LIBEBUS_DATA_H_
#define LIBEBUS_DATA_H_

#include "symbol.h"
#include "result.h"
#include "filereader.h"
#include "datatype.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

/** @file data.h
 * Classes related to defining fields based on data types in symbols on the
 * eBUS.
 *
 * A @a DataField is either a @a SingleDataField or a list of
 * @a SingleDataField instances in a @a DataFieldSet.
 *
 * Each @a SingleDataField has a reference to one basic field type @a DataType.
 * The list of available data types can be extended easily if necessary.
 *
 * The @a DataFieldTemplates allow definition of derived types as well as
 * combined types based on the list of available base types. It reads the
 * instances from configuration files by inheriting the @a FileReader template
 * class.
 */
using namespace std;

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
	 * @param maxLength the maximum length for calculating remainder of input.
	 * @return the length of this field (or contained fields) in bytes.
	 */
	virtual unsigned char getLength(PartType partType, unsigned char maxLength=MAX_LEN) = 0;

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
	 * Get the specified field name.
	 * @param fieldIndex the index of the field, or -1 for this.
	 * @return the field name, or the index as string if not unique or not available.
	 */
	virtual string getName(signed char fieldIndex=-1) const { return m_name; }

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
	 * @param length the variable in which to store the used length in bytes, or NULL.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t write(istringstream& input,
			const PartType partType, SymbolString& data,
			unsigned char offset, char separator=UI_FIELD_SEPARATOR, unsigned char* length=NULL) = 0;

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
			const string unit, DataType* dataType, const PartType partType,
			const unsigned char length)
		: DataField(name, comment),
		  m_unit(unit), m_dataType(dataType), m_partType(partType),
		  m_length(length) {}

	/**
	 * Destructor.
	 */
	virtual ~SingleDataField() {}

	// @copydoc
	virtual SingleDataField* clone();

	/**
	 * Factory method for creating a new @a SingleDataField instance derived from a base type.
	 * @param id the ID string (excluding optional length suffix).
	 * @param length the base type length, or 0 for default, or @a REMAIN_LEN for remainder within same message part.
	 * @param name the field name.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param partType the message part in which the field is stored.
	 * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none (if applicable).
	 * @param values the value=text assignments.
	 * @param constantValue the constant value as string, or empty.
	 * @param verifyValue whether to verify the read value against the constant value.
	 * @param returnField the variable in which the created @a SingleDataField instance shall be stored.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(const string id, const unsigned char length,
			const string name, const string comment, const string unit,
			const PartType partType, int divisor, map<unsigned int, string> values,
			const string constantValue, const bool verifyValue, SingleDataField* &returnField);

	/**
	 * Get the value unit.
	 * @return the value unit.
	 */
	string getUnit() const { return m_unit; }

	/**
	 * Get whether this field is ignored.
	 * @return whether this field is ignored.
	 */
	bool isIgnored() const { return m_dataType->isIgnored(); }

	/**
	 * Get the message part in which the field is stored.
	 * @return the message part in which the field is stored.
	 */
	PartType getPartType() const { return m_partType; }

	// @copydoc
	virtual unsigned char getLength(PartType partType, unsigned char maxLength=MAX_LEN);

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType,
			int divisor, map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	/**
	 * Get whether this field uses a full byte offset.
	 * @param after @p true to check after consuming the bits, @p false to check before.
	 * @return @p true if this field uses a full byte offset, @p false if this field
	 * only consumes a part of a byte and a subsequent field may re-use the same offset.
	 */
	bool hasFullByteOffset(bool after);

	// @copydoc
	virtual void dump(ostream& output);

	// @copydoc
	virtual bool hasField(const char* fieldName, bool numeric);

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
			unsigned char offset, char separator=UI_FIELD_SEPARATOR, unsigned char* length=NULL);

protected:

	/**
	 * Internal method for reading the field from a @a SymbolString.
	 * @param input the unescaped @a SymbolString to read the binary value from.
	 * @param isMaster whether the @a SymbolString is the master part.
	 * @param offset the offset in the @a SymbolString.
	 * @param output the ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t readSymbols(SymbolString& input, const bool isMaster,
			const unsigned char offset,
			ostringstream& output, OutputFormat outputFormat);

	/**
	 * Internal method for writing the field to a @a SymbolString.
	 * @param input the @a istringstream to parse the formatted value from.
	 * @param offset the offset in the @a SymbolString.
	 * @param output the unescaped @a SymbolString to write the binary value to.
	 * @param isMaster whether the @a SymbolString is the master part.
	 * @param usedLength the variable in which to store the used length in bytes, or NULL.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t writeSymbols(istringstream& input,
			const unsigned char offset,
			SymbolString& output, const bool isMaster, unsigned char* usedLength);

protected:

	/** the value unit. */
	const string m_unit;

	/** the data type definition. */
	DataType* m_dataType;

	/** the message part in which the field is stored. */
	const PartType m_partType;

	/** the number of symbols in the message part in which the field is stored. */
	const unsigned char m_length;

};


/**
 * A numeric data field with a list of value=text assignments and a string representation.
 */
class ValueListDataField : public SingleDataField
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
	 * @param values the value=text assignments.
	 */
	ValueListDataField(const string name, const string comment,
		const string unit, NumberDataType* dataType, const PartType partType,
		const unsigned char length, const map<unsigned int, string> values)
		: SingleDataField(name, comment, unit, dataType, partType, length),
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
	virtual result_t readSymbols(SymbolString& input, const bool isMaster,
			const unsigned char offset,
			ostringstream& output, OutputFormat outputFormat);

	// @copydoc
	virtual result_t writeSymbols(istringstream& input,
			const unsigned char offset,
			SymbolString& output, const bool isMaster, unsigned char* usedLength);

private:

	/** the value=text assignments. */
	map<unsigned int, string> m_values;

};


/**
 * A data field with a constant value.
 */
class ConstantDataField : public SingleDataField
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
	 * @param value the constant value.
	 * @param verify whether to verify the read value against the constant value.
	 */
	ConstantDataField(const string name, const string comment,
		const string unit, DataType* dataType, const PartType partType,
		const unsigned char length, const string value, const bool verify)
		: SingleDataField(name, comment, unit, dataType, partType, length),
		m_value(value), m_verify(verify) {}

	/**
	 * Destructor.
	 */
	virtual ~ConstantDataField() {}

	// @copydoc
	virtual ConstantDataField* clone();

	// @copydoc
	virtual result_t derive(string name, string comment,
			string unit, const PartType partType, int divisor,
			map<unsigned int, string> values,
			vector<SingleDataField*>& fields);

	// @copydoc
	virtual void dump(ostream& output);

protected:

	// @copydoc
	virtual result_t readSymbols(SymbolString& input, const bool isMaster,
			const unsigned char offset,
			ostringstream& output, OutputFormat outputFormat);

	// @copydoc
	virtual result_t writeSymbols(istringstream& input,
			const unsigned char offset,
			SymbolString& output, const bool isMaster, unsigned char* usedLength);

private:

	/** the constant value. */
	const string m_value;

	/** whether to verify the read value against the constant value. */
	const bool m_verify;

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
			SingleDataField* field = *it;
			if (field->isIgnored()) continue;
			string name = field->getName();
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
	virtual unsigned char getLength(PartType partType, unsigned char maxLength=MAX_LEN);

	// @copydoc
	virtual string getName(signed char fieldIndex=-1);

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
			unsigned char offset, char separator=UI_FIELD_SEPARATOR, unsigned char* length=NULL);

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
		vector< vector<string> >* defaults, const string& defaultDest, const string& defaultCircuit, const string& defaultSuffix,
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

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

#ifndef LIB_EBUS_DATA_H_
#define LIB_EBUS_DATA_H_

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <climits>
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/filereader.h"
#include "lib/ebus/datatype.h"

namespace ebusd {

/** @file lib/ebus/data.h
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

/** the field ID for the field name. */
#define DATAFIELD_NAME 0

/** the field ID for the part filter. */
#define DATAFIELD_PART (DATAFIELD_NAME+1)

/** the field ID for the data type. */
#define DATAFIELD_TYPE (DATAFIELD_PART+1)

/** the field ID for the divisor/values. */
#define DATAFIELD_DIVISORVALUES (DATAFIELD_TYPE+1)

/** the field ID for the unit. */
#define DATAFIELD_UNIT (DATAFIELD_DIVISORVALUES+1)

/** the field ID for the comment. */
#define DATAFIELD_COMMENT (DATAFIELD_UNIT+1)

/** the marker field ID for the minimum field ID. */
#define DATAFIELD_RANGE_MIN DATAFIELD_NAME

/** the marker field ID for the maximum field ID. */
#define DATAFIELD_RANGE_MAX DATAFIELD_COMMENT


/**
 * Get the data field ID for the given field name.
 * @param name the field name.
 * @return the field ID, or @a UINT_MAX if not found.
 */
size_t getDataFieldId(const string name);

/**
 * Get the data field name for the given field ID.
 * @param fieldId the field ID.
 * @return the field name, or empty if not found.
 */
string getDataFieldName(const size_t fieldId);

class DataFieldTemplates;
class SingleDataField;

/**
 * Base class for named items with optional named attributes.
 */
class AttributedItem {
 public:
  /**
   * Constructs a new instance.
   * @param name the item name.
   * @param attributes the additional named attributes.
   */
  AttributedItem(const string name, const map<string, string>& attributes)
    : m_name(name), m_attributes(attributes) {}

  /**
   * Constructs a new instance (without additional attributes).
   * @param name the field name.
   */
  explicit AttributedItem(const string name)
    : m_name(name) {}

  /**
   * Destructor.
   */
  virtual ~AttributedItem() {}


  /**
   * Remove and return a certain value from a map.
   * @param row the map to remove the value from.
   * @param key the name of the value to remove.
   * @return the named value from the map, or empty if not available.
   */
  static const string pluck(map<string, string>& row, const string key);

  /**
   * Dump the @a string optionally embedded in @a TEXT_SEPARATOR to the output.
   * @param output the @a ostream to dump to.
   * @param str the @a string to dump.
   * @param prependFieldSeparator whether to start with a @a FIELD_SEPARATOR.
   */
  static void dumpString(ostream& output, const string str, const bool prependFieldSeparator = true);

  /**
   * Append a named attribute as JSON to the output.
   * @param output the @a ostream to append to.
   * @param name the name of the attribute.
   * @param value the value of the attribute.
   * @param prependFieldSeparator whether to start with a @a FIELD_SEPARATOR.
   * @param asString true to force writing as string, false to detect the type from the value.
   */
  static void appendJson(ostream& output, const string name, const string value,
      const bool prependFieldSeparator = true, bool asString = false);

  /**
   * Merge this instance's additional named attributes into the specified attributes.
   * @param attributes the additional named attributes to merge in this instance's additional named attributes.
   */
  void mergeAttributes(map<string, string>& attributes) const;

  /**
   * Dump the attribute optionally embedded in @a TEXT_SEPARATOR to the output.
   * @param output the @a ostream to dump to.
   * @param name the name of the attribute to dump.
   * @param prependFieldSeparator whether to start with a @a FIELD_SEPARATOR.
   */
  void dumpAttribute(ostream& output, const string name, const bool prependFieldSeparator = true) const;

  /**
   * Append the attribute value to the output.
   * @param output the @a ostringstream to append the formatted value to.
   * @param outputFormat the @a OutputFormat options to use.
   * @param name the name of the attribute to append.
   * @param onlyIfNonEmpty true to append only if the value is not empty.
   * @param prefix optional prefix to use (only for non-JSON output).
   * @param suffix optional suffix to use (only for non-JSON output).
   * @return true if data was added, false otherwise.
   */
  bool appendAttribute(ostringstream& output, OutputFormat outputFormat, const string name,
      const bool onlyIfNonEmpty = true, const string prefix = "", const string suffix = "") const;

  /**
   * Append the attributes to the output.
   * @param output the @a ostringstream to append the formatted values to.
   * @param outputFormat the @a OutputFormat options to use.
   * @return true if data was added, false otherwise.
   */
  bool appendAttributes(ostringstream& output, OutputFormat outputFormat) const;

  /**
   * Get the item name.
   * @return the item name.
   */
  string getName() const { return m_name; }

  /**
   * Get a named attribute.
   * @param name the name of the attribute.
   * @return the named attribute value, or empty.
   */
  string getAttribute(const string name) const;


 protected:
  /** the field name. */
  const string m_name;

  /** the additional named attributes. */
  const map<string, string> m_attributes;
};

/**
 * Base class for all kinds of data fields.
 */
class DataField : public AttributedItem {
 public:
  /**
   * Constructs a new instance.
   * @param name the field name.
   * @param attributes the additional named attributes.
   */
  DataField(const string name, const map<string, string>& attributes)
    : AttributedItem(name, attributes) {}

  /**
   * Constructs a new instance (without additional attributes).
   * @param name the field name.
   */
  explicit DataField(const string name)
    : AttributedItem(name) {}

  /**
   * Destructor.
   */
  virtual ~DataField() {}

  /**
   * Clone this instance.
   * @return a clone of this instance.
   */
  virtual const DataField* clone() const = 0;

  /**
   * Factory method for creating new instances.
   * @param rows the mapped field definition rows.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param templates the @a DataFieldTemplates to be referenced by name, or NULL.
   * @param returnField the variable in which to store the created instance.
   * @param isWriteMessage whether the field is part of a write message (default false).
   * @param isTemplate true for creating a template @a DataField.
   * @param isBroadcastOrMasterDestination true if the destination bus address is @a BRODCAST or a master address.
   * @param maxFieldLength the maximum allowed length of a single field (default @a MAX_POS).
   * @return @a RESULT_OK on success, or an error code.
   * Note: the caller needs to free the created instance.
   */
  static result_t create(vector< map<string, string> >& rows, string& errorDescription,
    DataFieldTemplates* templates, const DataField*& returnField,
    const bool isWriteMessage,
    const bool isTemplate, const bool isBroadcastOrMasterDestination,
    const size_t maxFieldLength = MAX_POS);

  /**
   * Return the name of the specified day.
   * @param day the day (between 0 and 6).
   * @return the name of the specified day.
   */
  static string getDayName(int day);

  /**
   * Returns the length of this field (or contained fields) in bytes.
   * @param partType the message part of the contained fields to limit the length calculation to.
   * @param maxLength the maximum length for calculating remainder of input.
   * @return the length of this field (or contained fields) in bytes.
   */
  virtual size_t getLength(PartType partType, size_t maxLength = MAX_LEN) const = 0;

  /**
   * Derive a new @a DataField from this field.
   * @param name the field name, or empty to use this fields name.
   * @param attributes the additional named attributes to override.
   * @param partType the message part in which the field is stored.
   * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none (if applicable).
   * @param values the value=text assignments, or empty to use this fields assignments (if applicable).
   * @param fields the @a vector to which created @a SingleDataField instances shall be added.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const = 0;

  /**
   * Get the specified field name.
   * @param fieldIndex the index of the field, or -1 for this.
   * @return the field name, or the index as string if not unique or not available.
   */
  virtual string getName(const ssize_t fieldIndex = -1) const { return m_name; }

  /**
   * Dump the field settings to the output.
   * @param output the @a ostream to dump to.
   */
  virtual void dump(ostream& output) const = 0;

  /**
   * Return whether the field is available.
   * @param fieldName the name of the field to find, or NULL for any.
   * @param numeric true for a numeric field, false for a string field.
   * @return true if the field is available.
   */
  virtual bool hasField(const char* fieldName, bool numeric) const = 0;

  /**
   * Reads the numeric value from the @a SymbolString.
   * @param data the data @a SymbolString for reading binary data.
   * @param offset the additional offset to add for reading binary data.
   * @param output the variable in which to store the numeric value.
   * @param fieldName the name of the field to read, or NULL for the first field.
   * @param fieldIndex the optional index of the named field, or -1.
   * @return @a RESULT_OK on success,
   * or @a RESULT_EMPTY if the field was skipped (either if the partType does
   * not match or ignored, or due to @a fieldName or @a fieldIndex),
   * or an error code.
   */
  virtual result_t read(const SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName = NULL, ssize_t fieldIndex = -1) const = 0;

  /**
   * Reads the value from the @a SymbolString.
   * @param data the data @a SymbolString for reading binary data.
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
  virtual result_t read(const SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex = -1,
    bool leadingSeparator = false, const char* fieldName = NULL, ssize_t fieldIndex = -1) const = 0;

  /**
   * Writes the value to the master or slave @a SymbolString.
   * @param input the @a istringstream to parse the formatted value from.
   * @param data the unescaped data @a SymbolString for writing binary data.
   * @param offset the additional offset to add for writing binary data.
   * @param separator the separator character between multiple fields.
   * @param length the variable in which to store the used length in bytes, or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t write(istringstream& input, SymbolString& data,
    size_t offset, char separator = UI_FIELD_SEPARATOR, size_t* length = NULL) const = 0;
};


/**
 * A single @a DataField holding a value.
 */
class SingleDataField : public DataField {
 public:
  /**
   * Constructs a new instance.
   * @param name the field name.
   * @param attributes the additional named attributes.
   * @param dataType the data type definition.
   * @param partType the message part in which the field is stored.
   * @param length the number of symbols in the message part in which the field is stored.
   */
  SingleDataField(const string name, const map<string, string>& attributes, const DataType* dataType,
    const PartType partType, const size_t length)
    : DataField(name, attributes),
    m_partType(partType), m_dataType(dataType), m_length(length) {}

  /**
   * Destructor.
   */
  virtual ~SingleDataField() {}

  // @copydoc
  const SingleDataField* clone() const override;

  /**
   * Factory method for creating a new @a SingleDataField instance derived from a base type.
   * @param name the field name.
   * @param attributes the additional named attributes.
   * @param partType the message part in which the field is stored.
   * @param dataType the @a DataType instance.
   * @param length the base type length, or 0 for default, or @a REMAIN_LEN for remainder within same message part.
   * @param divisor the extra divisor (negative for reciprocal) to apply on the value, or 1 for none (if applicable).
   * @param values the value=text assignments.
   * @param constantValue the constant value as string, or empty.
   * @param verifyValue whether to verify the read value against the constant value.
   * @param returnField the variable in which the created @a SingleDataField instance shall be stored.
   * @return @a RESULT_OK on success, or an error code.
   * Note: the caller needs to free the created instance.
   */
  static result_t create(const string name, const map<string, string>& attributes, const DataType* dataType,
    const PartType partType, const size_t length, int divisor, map<unsigned int, string> values,
    const string constantValue, const bool verifyValue, SingleDataField* &returnField);

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
  size_t getLength(PartType partType, size_t maxLength = MAX_LEN) const override;

  // @copydoc
  result_t derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const override;

  /**
   * Get whether this field uses a full byte offset.
   * @param after @p true to check after consuming the bits, @p false to check before.
   * @return @p true if this field uses a full byte offset, @p false if this field
   * only consumes a part of a byte and a subsequent field may re-use the same offset.
   */
  bool hasFullByteOffset(bool after) const;

  // @copydoc
  void dump(ostream& output) const override;

  // @copydoc
  bool hasField(const char* fieldName, bool numeric) const override;

  // @copydoc
  result_t read(const SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName = NULL, ssize_t fieldIndex = -1) const override;

  // @copydoc
  result_t read(const SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex = -1,
    bool leadingSeparator = false, const char* fieldName = NULL, ssize_t fieldIndex = -1) const override;

  // @copydoc
  result_t write(istringstream& input, SymbolString& data,
    size_t offset, char separator = UI_FIELD_SEPARATOR, size_t* length = NULL) const override;


 protected:
  /**
   * Internal method for reading the field from a @a SymbolString.
   * @param input the @a SymbolString to read the binary value from.
   * @param offset the offset in the @a SymbolString.
   * @param output the ostringstream to append the formatted value to.
   * @param outputFormat the @a OutputFormat options to use.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readSymbols(const SymbolString& input,
    const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const;

  /**
   * Internal method for writing the field to a @a SymbolString.
   * @param input the @a istringstream to parse the formatted value from.
   * @param offset the offset in the @a SymbolString.
   * @param output the @a SymbolString to write the binary value to.
   * @param usedLength the variable in which to store the used length in bytes, or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t writeSymbols(istringstream& input,
    const size_t offset,
    SymbolString& output, size_t* usedLength) const;

  /** the message part in which the field is stored. */
  const PartType m_partType;

  /** the data type definition. */
  const DataType* m_dataType;

  /** the number of symbols in the message part in which the field is stored. */
  const size_t m_length;
};


/**
 * A numeric data field with a list of value=text assignments and a string representation.
 */
class ValueListDataField : public SingleDataField {
 public:
  /**
   * Constructs a new instance.
   * @param name the field name.
   * @param attributes the additional named attributes.
   * @param dataType the data type definition.
   * @param partType the message part in which the field is stored.
   * @param length the number of symbols in the message part in which the field is stored.
   * @param values the value=text assignments.
   */
  ValueListDataField(const string name, const map<string, string>& attributes, const DataType* dataType,
    const PartType partType, const size_t length, const map<unsigned int, string> values)
    : SingleDataField(name, attributes, dataType, partType, length),
    m_values(values) {}

  /**
   * Destructor.
   */
  virtual ~ValueListDataField() {}

  // @copydoc
  const ValueListDataField* clone() const override;

  // @copydoc
  result_t derive(const string name, map<string, string> attributes, const PartType partType,
      int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const override;

  // @copydoc
  void dump(ostream& output) const override;


 protected:
  // @copydoc
  result_t readSymbols(const SymbolString& input, const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const override;

  // @copydoc
  result_t writeSymbols(istringstream& input, const size_t offset,
    SymbolString& output, size_t* usedLength) const override;


 private:
  /** the value=text assignments. */
  const map<unsigned int, string> m_values;
};


/**
 * A data field with a constant value.
 */
class ConstantDataField : public SingleDataField {
 public:
  /**
   * Constructs a new instance.
   * @param name the field name.
   * @param attributes the additional named attributes.
   * @param dataType the data type definition.
   * @param partType the message part in which the field is stored.
   * @param length the number of symbols in the message part in which the field is stored.
   * @param value the constant value.
   * @param verify whether to verify the read value against the constant value.
   */
  ConstantDataField(const string name, const map<string, string>& attributes, const DataType* dataType,
    const PartType partType, const size_t length, const string value, const bool verify)
    : SingleDataField(name, attributes, dataType, partType, length),
    m_value(value), m_verify(verify) {}

  /**
   * Destructor.
   */
  virtual ~ConstantDataField() {}

  // @copydoc
  const ConstantDataField* clone() const override;

  // @copydoc
  result_t derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const override;

  // @copydoc
  void dump(ostream& output) const override;


 protected:
  // @copydoc
  result_t readSymbols(const SymbolString& input, const size_t offset,
    ostringstream& output, OutputFormat outputFormat) const override;

  // @copydoc
  result_t writeSymbols(istringstream& input, const size_t offset,
    SymbolString& output, size_t* usedLength) const override;


 private:
  /** the constant value. */
  const string m_value;

  /** whether to verify the read value against the constant value. */
  const bool m_verify;
};


/**
 * A set of @a DataField instances.
 */
class DataFieldSet : public DataField {
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
   * @param fields the @a vector of @a SingleDataField instances part of this set.
   */
  DataFieldSet(const string name, const vector<const SingleDataField*> fields)
    : DataField(name), m_fields(fields) {
    bool uniqueNames = true;
    map<string, string> names;
    for (auto field : fields) {
      if (field->isIgnored()) {
        continue;
      }
      string name = field->getName();
      if (name.empty() || names.find(name) != names.end()) {
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
  const DataFieldSet* clone() const override;

  // @copydoc
  size_t getLength(PartType partType, size_t maxLength = MAX_LEN) const override;

  // @copydoc
  string getName(const ssize_t fieldIndex = -1) const override;

  // @copydoc
  result_t derive(const string name, map<string, string> attributes, const PartType partType,
    int divisor, map<unsigned int, string> values, vector<const SingleDataField*>& fields) const override;

  /**
   * Returns the @a SingleDataField at the specified index.
   * @param index the index of the @a SingleDataField to return.
   * @return the @a SingleDataField at the specified index, or NULL.
   */
  /*SingleDataField* operator[](const size_t index) {
    if (index >= m_fields.size()) {
      return NULL;
    }
    return m_fields[index];
  }*/

  /**
   * Returns the @a SingleDataField at the specified index.
   * @param index the index of the @a SingleDataField to return.
   * @return the @a SingleDataField at the specified index, or NULL.
   */
  const SingleDataField* operator[](const size_t index) const {
    if (index >= m_fields.size()) {
      return NULL;
    }
    return m_fields[index];
  }

  /**
   * Returns the number of @a SingleDataFields instances in this set.
   * @return the number of available @a SingleDataField instances.
   */
  size_t size() const { return m_fields.size(); }

  // @copydoc
  bool hasField(const char* fieldName, bool numeric) const override;

  // @copydoc
  void dump(ostream& output) const override;

  // @copydoc
  result_t read(const SymbolString& data, size_t offset,
    unsigned int& output, const char* fieldName = NULL, ssize_t fieldIndex = -1) const override;

  // @copydoc
  result_t read(const SymbolString& data, size_t offset,
    ostringstream& output, OutputFormat outputFormat, ssize_t outputIndex = -1,
    bool leadingSeparator = false, const char* fieldName = NULL, ssize_t fieldIndex = -1) const override;

  // @copydoc
  result_t write(istringstream& input, SymbolString& data,
    size_t offset, char separator = UI_FIELD_SEPARATOR, size_t* length = NULL) const override;


 private:
  /** the @a DataFieldSet containing the ident message @a SingleDataField instances, or NULL. */
  static DataFieldSet* s_identFields;

  /** the @a vector of @a SingleDataField instances part of this set. */
  const vector<const SingleDataField*> m_fields;

  /** whether all fields have a unique name. */
  bool m_uniqueNames;
};


/**
 * A map of template @a DataField instances.
 */
class DataFieldTemplates : public MappedFileReader {
 public:
  /**
   * Constructs a new instance.
   */
  DataFieldTemplates() : MappedFileReader::MappedFileReader(false) {}

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
  result_t add(const DataField* field, string name = "", bool replace = false);

  // @copydoc
  result_t getFieldMap(vector<string>& row, string& errorDescription) const override;

  // @copydoc
  result_t addFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
    string& errorDescription, const string filename, unsigned int lineNo) override;

  /**
   * Gets the template @a DataField instance with the specified name.
   * @param name the name of the template to get.
   * @return the template @a DataField instance, or NULL.
   * Note: the caller may not free the returned instance.
   */
  const DataField* get(string name) const;


 private:
  /** the known template @a DataField instances by name. */
  map<string, const DataField*> m_fieldsByName;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DATA_H_

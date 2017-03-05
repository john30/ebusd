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

#ifndef LIB_EBUS_DATATYPE_H_
#define LIB_EBUS_DATATYPE_H_

#include <stdint.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/filereader.h"

namespace ebusd {

/** @file lib/ebus/datatype.h
 * Classes, functions, and constants related to decoding/encoding of symbols
 * on the eBUS to/from readable values and registry of data types.
 *
 * A @a DataType is one of @a StringDataType, @a DateTimeDataType,
 * @a NumberDataType, or @a BitsDataType.
 *
 * The particular eBUS specification types like e.g. @a D1C are defined by
 * using one of these base data types with certain flags, such as #BCD, #FIX,
 * #REQ, see @a DataType.
 *
 * Each @a DataType can be converted from a @a SymbolString to an
 * @a ostringstream (see @a DataType#readSymbols() methods) or vice versa from
 * an @a istringstream to a @a SymbolString (see @a DataType#writeSymbols()).
 */

using std::map;
using std::list;

/** the separator character used between base type name and length (in CSV only). */
#define LENGTH_SEPARATOR ':'

/** the replacement string for undefined values (in UI and CSV). */
#define NULL_VALUE "-"

/** the separator character used between fields (in UI only). */
#define UI_FIELD_SEPARATOR ';'

/** the maximum allowed position within master or slave data. */
#define MAX_POS 24

/** the maximum allowed field length. */
#define MAX_LEN 31

/** the field length indicating remainder of input. */
#define REMAIN_LEN 255

/** the maximum divisor value. */
#define MAX_DIVISOR 1000000000

/** the maximum value for value lists. */
#define MAX_VALUE (0xFFFFFFFFu)


/** the type for data output format options. */
typedef int OutputFormat;

/** bit flag for @a OutputFormat: include names. */
#define OF_NAMES 0x01

/** bit flag for @a OutputFormat: include units. */
#define OF_UNITS 0x02

/** bit flag for @a OutputFormat: include comments. */
#define OF_COMMENTS 0x04

/** bit flag for @a OutputFormat: numeric format (keep numeric value of value=name pairs). */
#define OF_NUMERIC 0x08

/** bit flag for @a OutputFormat: JSON format. */
#define OF_JSON 0x10

/** bit flag for @a OutputFormat: short format (only name and value, no indentation). */
#define OF_SHORT 0x20

/** the message part in which a data field is stored. */
enum PartType {
  pt_any,          //!< stored in any data (master or slave)
  pt_masterData,   //!< stored in master data
  pt_slaveData,    //!< stored in slave data
};

/** bit flag for @a DataType: adjustable length, bitCount is maximum length. */
#define ADJ 0x01

/** bit flag for @a DataType: binary representation is BCD. */
#define BCD 0x02

/** bit flag for @a DataType: reverted binary representation (most significant byte first). */
#define REV 0x04

/** bit flag for @a DataType: signed value. */
#define SIG 0x08

/** bit flag for @a DataType: ignore value during read and write. */
#define IGN 0x10

/** bit flag for @a DataType: fixed width formatting. */
#define FIX 0x20

/** bit flag for @a DataType: value may not be NULL. */
#define REQ 0x40

/** bit flag for @a DataType: binary representation is hex converted to decimal and interpreted as 2 digits
 * (also requires #BCD). */
#define HCD 0x80

/** bit flag for @a DataType: exponential numeric representation. */
#define EXP 0x100

/** bit flag for @a DataType: forced value list defaulting to week days. */
#define DAY 0x200

/** bit flag for @a DataType: numeric type with base class @a NumberDataType. */
#define NUM 0x400

/** bit flag for @a DataType: special marker for certain types. */
#define SPE 0x800

/** bit flag for @a DataType: marker for a constant value. */
#define CON 0x1000


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
void printErrorPos(ostream& out, vector<string>::iterator begin, const vector<string>::iterator end,
    vector<string>::iterator pos, string filename, size_t lineNo, result_t result);


/**
 * Base class for all kinds of data types.
 */
class DataType {
 public:
  /**
   * Constructs a new instance.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set, must be multiple of 8 with flag #BCD).
   * @param flags the combination of flags (like #BCD).
   * @param replacement the replacement value (fill-up value for @a StringDataType, no replacement if equal to
   * @a NumberDataType#minValue).
   */
  DataType(const string id, const size_t bitCount, const uint16_t flags, const unsigned int replacement)
    : m_id(id), m_bitCount(bitCount), m_flags(flags), m_replacement(replacement) {}

  /**
   * Destructor.
   */
  virtual ~DataType() { }

  /**
   * @return the type identifier.
   */
  string getId() const { return m_id; }

  /**
   * @return the number of bits (maximum length if #ADJ flag is set).
   */
  size_t getBitCount() const { return m_bitCount; }

  /**
   * Check whether a flag is set.
   * @param flag the flag to check (like #BCD).
   * @return whether the flag is set.
   */
  bool hasFlag(const unsigned int flag) const { return (m_flags & flag) != 0; }

  /**
   * @return whether this type is ignored.
   */
  bool isIgnored() const { return hasFlag(IGN); }

  /**
   * @return whether this type has an adjustable length.
   */
  bool isAdjustableLength() const { return hasFlag(ADJ); }

  /**
   * @return whether this field is derived from @a NumberDataType.
   */
  bool isNumeric() const { return hasFlag(NUM); }

  /**
   * @return the replacement value (fill-up value for @a StringDataType, no replacement if equal to
   * @a NumberDataType#minValue).
   */
  unsigned int getReplacement() const { return m_replacement; }

  /**
   * Dump the type identifier with the specified length and optionally the
   * divisor to the output.
   * @param output the @a ostream to dump to.
   * @param length the number of symbols to read/write.
   * @param appendSeparatorDivisor whether to append a @a FIELD_SEPARATOR followed by the divisor (if available).
   * @return true when a non-default divisor was written to the output.
   */
  virtual bool dump(ostream& output, const size_t length, const bool appendSeparatorDivisor = true) const;

  /**
   * Internal method for reading the numeric raw value from a @a SymbolString.
   * @param input the @a SymbolString to read the binary value from.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to read.
   * @param value the variable in which to store the numeric raw value.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readRawValue(SymbolString& input,
    const size_t offset, const size_t length,
    unsigned int& value) = 0;

  /**
   * Internal method for reading the field from a @a SymbolString.
   * @param input the @a SymbolString to read the binary value from.
   * @param offset the offset in the data of the @a SymbolString.
   * @param length the number of symbols to read.
   * @param output the ostringstream to append the formatted value to.
   * @param outputFormat the @a OutputFormat options to use.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readSymbols(SymbolString& input,
    const size_t offset, const size_t length,
    ostringstream& output, OutputFormat outputFormat) = 0;

  /**
   * Internal method for writing the field to a @a SymbolString.
   * @param input the @a istringstream to parse the formatted value from.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to write, or @a REMAIN_LEN.
   * @param output the @a SymbolString to write the binary value to.
   * @param usedLength the variable in which to store the used length in bytes, or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t writeSymbols(istringstream& input,
    const size_t offset, const size_t length,
    SymbolString& output, size_t* usedLength) = 0;


 protected:
  /** the type identifier. */
  const string m_id;

  /** the number of bits (maximum length if #ADJ flag is set, must be multiple of 8 with flag #BCD). */
  const size_t m_bitCount;

  /** the combination of flags (like #BCD). */
  const uint16_t m_flags;

  /** the replacement value (fill-up value for @a StringDataType, no replacement if equal to
   * @a NumberDataType#m_minValue). */
  const unsigned int m_replacement;
};


/**
 * A string based @a DataType.
 */
class StringDataType : public DataType {
 public:
  /**
   * Constructs a new instance.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set, must be multiple of 8 with flag #BCD).
   * @param flags the combination of flags (like #BCD).
   * @param replacement the replacement value (fill-up value).
   * @param isHex true for hex digits instead of characters.
   */
  StringDataType(const string id, const size_t bitCount, const uint16_t flags,
    const unsigned int replacement, bool isHex = false)
    : DataType(id, bitCount, flags, replacement), m_isHex(isHex) {}

  /**
   * Destructor.
   */
  virtual ~StringDataType() {}

  // @copydoc
  result_t readRawValue(SymbolString& input,
    const size_t offset, const size_t length,
    unsigned int& value) override;

  // @copydoc
  result_t readSymbols(SymbolString& input,
    const size_t offset, const size_t length,
    ostringstream& output, OutputFormat outputFormat) override;

  // @copydoc
  result_t writeSymbols(istringstream& input,
    const size_t offset, const size_t length,
    SymbolString& output, size_t* usedLength) override;


 private:
  /** true for hex digits instead of characters. */
  const bool m_isHex;
};


/**
 * A date/time based @a DataType.
 */
class DateTimeDataType : public DataType {
 public:
  /**
   * Constructs a new instance.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set, must be multiple of 8 with flag #BCD).
   * @param flags the combination of flags (like #BCD).
   * @param replacement the replacement value.
   * @param hasDate true if date part is present.
   * @param hasTime true if time part is present.
   * @param resolution the the resolution in minutes for time types, or 1.
   */
  DateTimeDataType(const string id, const size_t bitCount, const uint16_t flags, const unsigned int replacement,
      const bool hasDate, const bool hasTime, const int16_t resolution)
    : DataType(id, bitCount, flags, replacement), m_hasDate(hasDate), m_hasTime(hasTime),
      m_resolution(resolution == 0 ? 1 : resolution) {}

  /**
   * Destructor.
   */
  virtual ~DateTimeDataType() {}

  /**
   * @return true if date part is present.
   */
  bool hasDate() const { return m_hasDate; }

  /**
   * @return true if time part is present.
   */
  bool hasTime() const { return m_hasTime; }

  /**
   * @return the resolution in minutes for time types, or 1.
   */
  int16_t getResolution() const { return m_resolution; }

  // @copydoc
  result_t readRawValue(SymbolString& input,
    const size_t offset, const size_t length,
    unsigned int& value) override;

  // @copydoc
  result_t readSymbols(SymbolString& input,
    const size_t offset, const size_t length,
    ostringstream& output, OutputFormat outputFormat) override;

  // @copydoc
  result_t writeSymbols(istringstream& input,
    const size_t offset, const size_t length,
    SymbolString& output, size_t* usedLength) override;


 private:
  /** true if date part is present. */
  const bool m_hasDate;

  /** true if time part is present. */
  const bool m_hasTime;

  /** the resolution in minutes for time types, or 1. */
  const int16_t m_resolution;
};


/**
 * A number based @a DataType.
 */
class NumberDataType : public DataType {
 public:
  /**
   * Constructs a new instance for multiple of 8 bits.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set).
   * @param flags the combination of flags (like #BCD).
   * @param replacement the replacement value (no replacement if equal to minValue).
   * @param minValue the minimum raw value.
   * @param maxValue the maximum raw value.
   * @param divisor the divisor (negative for reciprocal).
   */
  NumberDataType(const string id, const size_t bitCount, const uint16_t flags, const unsigned int replacement,
      const unsigned int minValue, const unsigned int maxValue, const int divisor)
    : DataType(id, bitCount, flags|NUM, replacement), m_minValue(minValue), m_maxValue(maxValue), m_divisor(divisor),
      m_precision(calcPrecision(divisor)), m_firstBit(0), m_baseType(NULL) {}

  /**
   * Constructs a new instance for less than 8 bits.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set).
   * @param flags the combination of flags (like #ADJ, may not include flag #BCD).
   * @param replacement the replacement value (no replacement if zero).
   * @param firstBit the offset to the first bit.
   * @param divisor the divisor (negative for reciprocal).
   */
  NumberDataType(const string id, const size_t bitCount, const uint16_t flags, const unsigned int replacement,
      const int16_t firstBit, const int divisor)
    : DataType(id, bitCount, flags|NUM, replacement), m_minValue(0), m_maxValue((1 << bitCount)-1), m_divisor(divisor),
      m_precision(0), m_firstBit(firstBit), m_baseType(NULL) {}

  /**
   * Destructor.
   */
  virtual ~NumberDataType() {}

  /**
   * Calculate the precision from the divisor.
   *
   * @param divisor the divisor (negative for reciprocal).
   * @return the precision for formatting the value.
   */
  static size_t calcPrecision(const int divisor);

  // @copydoc
  bool dump(ostream& output, const size_t length, const bool appendSeparatorDivisor = true) const override;

  /**
   * Derive a new @a NumberDataType from this.
   * @param divisor the extra divisor (negative for reciprocal) to apply, or
   * 1 for none (if applicable), or 0 to keep the current value.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set,
   * must be multiple of 8 with flag #BCD), or 0 to keep the current value.
   * @param derived the derived @a NumberDataType, or this if derivation is
   * not necessary.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t derive(int divisor, size_t bitCount, NumberDataType* &derived);

  /**
   * @return the minimum raw value.
   */
  unsigned int getMinValue() const { return m_minValue; }

  /**
   * @return the maximum raw value.
   */
  unsigned int getMaxValue() const { return m_maxValue; }

  /**
   * @return the divisor (negative for reciprocal).
   */
  int getDivisor() const { return m_divisor; }

  /**
   * @return the precision for formatting the value.
   */
  size_t getPrecision() const { return m_precision; }

  /**
   * @return the offset to the first bit.
   */
  int16_t getFirstBit() const { return m_firstBit; }

  // @copydoc
  result_t readRawValue(SymbolString& input,
    const size_t offset, const size_t length,
    unsigned int& value) override;

  // @copydoc
  result_t readSymbols(SymbolString& input,
    const size_t offset, const size_t length,
    ostringstream& output, OutputFormat outputFormat) override;

  /**
   * Internal method for writing the numeric raw value to a @a SymbolString.
   * @param value the numeric raw value to write.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to write, or @a REMAIN_LEN.
   * @param output the @a SymbolString to write the binary value to.
   * @param usedLength the variable in which to store the used length in bytes,
   * or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t writeRawValue(unsigned int value,
    const size_t offset, const size_t length,
    SymbolString& output, size_t* usedLength = NULL);

  // @copydoc
  result_t writeSymbols(istringstream& input,
    const size_t offset, const size_t length,
    SymbolString& output, size_t* usedLength) override;


 private:
  /** the minimum raw value. */
  const unsigned int m_minValue;

  /** the maximum raw value. */
  const unsigned int m_maxValue;

  /** the divisor (negative for reciprocal). */
  const int m_divisor;

  /** the precision for formatting the value. */
  const size_t m_precision;

  /** the offset to the first bit. */
  const int16_t m_firstBit;

  /** the base @a NumberDataType for derived instances. */
  NumberDataType* m_baseType;
};


/**
 * A map of base @a DataType instances.
 */
class DataTypeList {
 public:
  /**
   * Constructs a new instance and registers the known base data types.
   */
  DataTypeList();

  /**
   * Destructor.
   */
  virtual ~DataTypeList() {
    clear();
  }

  /**
   * Returns the singleton instance.
   * @return the singleton @a DataTypeList instance.
   */
  static DataTypeList* getInstance();

  /**
   * Removes all @a DataType instances.
   */
  void clear();

  /**
   * Adds a @a DataType instance to this map.
   * @param dataType the @a DataType instance to add.
   * @return @a RESULT_OK on success, or an error code.
   * Note: the caller may not free the added instance on success.
   */
  result_t add(DataType* dataType);

  /**
   * Adds a @a DataType instance for later cleanup.
   * @param dataType the @a DataType instance to add.
   */
  void addCleanup(DataType* dataType) { m_cleanupTypes.push_back(dataType); }

  /**
   * Gets the @a DataType instance with the specified ID.
   * @param id the ID string (excluding optional length suffix).
   * @param length the length in bytes, or 0 for default.
   * @return the @a DataType instance, or NULL if not available.
   * Note: the caller may not free the instance.
   */
  DataType* get(const string id, const size_t length = 0);

  /**
   * Returns an iterator pointing to the first ID/@a DataType pair.
   * @return an iterator pointing to the first ID/@a DataType pair.
   */
  map<string, DataType*>::const_iterator begin() const { return m_typesById.begin(); }

  /**
   * Returns an iterator pointing one past the last ID/@a DataType pair.
   * @return an iterator pointing one past the last ID/@a DataType pair.
   */
  map<string, DataType*>::const_iterator end() const { return m_typesById.end(); }

 private:
  /** the known @a DataType instances by ID only. */
  map<string, DataType*> m_typesById;

  /** the known @a DataType instances by ID and length (i.e. "ID:BITS").
   * Note: adjustable length types are stored by ID only. */
  map<string, DataType*> m_typesByIdLength;

  /** the @a DataType instances to cleanup. */
  list<DataType*> m_cleanupTypes;

  /** the singleton instance. */
  static DataTypeList s_instance;

#ifdef HAVE_CONTRIB
  /** true when contributed datatypes were successfully initialized. */
  static bool s_contrib_initialized;
#endif
};

}  // namespace ebusd

#endif  // LIB_EBUS_DATATYPE_H_

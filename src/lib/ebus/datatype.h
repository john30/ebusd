/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2026 John Baier <ebusd@ebusd.eu>
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
using std::istringstream;
using std::ostringstream;

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

typedef unsigned int OutputFormatBaseType;

enum OutputFormat : OutputFormatBaseType {
  /** no bit set at all. */
  OF_NONE = 0,

  /** bit flag for @a OutputFormat: include names. */
  OF_NAMES = 1 << 0,

  /** bit flag for @a OutputFormat: include units. */
  OF_UNITS = 1 << 1,

  /** bit flag for @a OutputFormat: include comments. */
  OF_COMMENTS = 1 << 2,

  /** bit flag for @a OutputFormat: numeric format (keep numeric value of value=name pairs). */
  OF_NUMERIC = 1 << 3,

  /** bit flag for @a OutputFormat: value=name format for such pairs. */
  OF_VALUENAME = 1 << 4,

  /** bit flag for @a OutputFormat: JSON format. */
  OF_JSON = 1 << 5,

  /** bit flag for @a OutputFormat: short format (only name and value for fields). */
  OF_SHORT = 1 << 6,

  /** bit flag for @a OutputFormat: include all attributes. */
  OF_ALL_ATTRS = 1 << 7,

  /** bit flag for @a OutputFormat: include message/field definition. */
  OF_DEFINITION = 1 << 8,

  /** bit flag for @a OutputFormat: include raw data. */
  OF_RAWDATA = 1 << 9,
};

constexpr inline enum OutputFormat operator| (enum OutputFormat self, enum OutputFormat other) {
  return (enum OutputFormat)((OutputFormatBaseType)self | (OutputFormatBaseType)other);
}

inline enum OutputFormat& operator|= (enum OutputFormat& self, enum OutputFormat other) {
  self = self | other;
  return self;
}

constexpr inline enum OutputFormat operator& (enum OutputFormat self, enum OutputFormat other) {
  return (enum OutputFormat)((OutputFormatBaseType)self & (OutputFormatBaseType)other);
}

inline enum OutputFormat& operator&= (enum OutputFormat& self, enum OutputFormat other) {
  self = self & other;
  return self;
}

constexpr inline enum OutputFormat operator~ (enum OutputFormat self) {
  return (enum OutputFormat)(~(OutputFormatBaseType)self);
}

/** whether divisor should be appended to a dump. */
enum AppendDivisor {
  ad_none,      //!< no dump of divisor
  ad_normal,    //!< regular dump of divisor (i.e. not for base types)
  ad_full,  //!< full dump of divisor (i.e. also for base types)
};

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

/** bit flag for @a DataType: numeric type with base class @a DateTimeDataType. */
#define DAT 0x800

/** bit flag for @a DataType: special marker for certain types. */
#define SPE 0x1000

/** bit flag for @a DataType: stored duplicate for backwards compatibility, not to be traversed in lists any more. */
#define DUP 0x2000

/** bit flag for @a DataType: special marker for non-tolerated secondary replacement value of zero (date only). */
#define REZ 0x4000

/**
 * Parse a float value from the 32 bit representation (IEEE 754).
 * @param value the 32 bit representation of the float value.
 * @param negative true if the value is negative.
 * @return the float value.
 */
float uintToFloat(unsigned int value, bool negative);

/**
 * Format a float value to the 32 bit representation (IEEE 754).
 * @param value the float value.
 * @return the 32 bit representation of the float value, or 0xffffffff if NaN.
 */
uint32_t floatToUint(float value);

/**
 * Parse a float value with precision of 2 decimal from 16 bit format with
 * sign, 11 bit mantissa, 4 bit exponent as (0.01*m)(2^e).
 * @param value the 16 bit representation of the float value.
 * @return the float value.
 */
float uint16ToFloat(uint16_t value);

/**
 * Format a float value with precision of 2 decimal from 16 bit format with
 * sign, 11 bit mantissa, 4 bit exponent as (0.01*m)(2^e).
 * @param value the float value.
 * @return the 16 bit representation of the float value, or 0xffff if NaN.
 */
uint16_t floatToUint16(float value);

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
  DataType(const string& id, size_t bitCount, uint16_t flags, unsigned int replacement)
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
  bool hasFlag(unsigned int flag) const { return (m_flags & flag) != 0; }

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
   * @param outputFormat the @a OutputFormat options.
   * @param length the number of symbols to read/write.
   * @param appendDivisor whether to append the divisor (if available).
   * @param output the @a ostream to dump to.
   * @return true when a non-default divisor was written to the output.
   */
  virtual bool dump(OutputFormat outputFormat, size_t length, AppendDivisor appendDivisor, ostream* output) const;

  /**
   * Internal method for reading the numeric raw value from a @a SymbolString.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to read.
   * @param input the @a SymbolString to read the binary value from.
   * @param value the variable in which to store the numeric raw value.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readRawValue(size_t offset, size_t length, const SymbolString& input,
      unsigned int* value) const = 0;

  /**
   * Internal method for reading the field from a @a SymbolString.
   * @param offset the offset in the data of the @a SymbolString.
   * @param length the number of symbols to read.
   * @param input the @a SymbolString to read the binary value from.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the ostream to append the formatted value to.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readSymbols(size_t offset, size_t length, const SymbolString& input,
      OutputFormat outputFormat, ostream* output) const = 0;

  /**
   * Internal method for writing the field to a @a SymbolString.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to write, or @a REMAIN_LEN.
   * @param input the @a istringstream to parse the formatted value from.
   * @param output the @a SymbolString to write the binary value to.
   * @param usedLength the variable in which to store the used length in bytes, or nullptr.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t writeSymbols(size_t offset, size_t length, istringstream* input,
      SymbolString* output, size_t* usedLength) const = 0;


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
  StringDataType(const string& id, size_t bitCount, uint16_t flags,
    unsigned int replacement, bool isHex = false)
    : DataType(id, bitCount, flags, replacement), m_isHex(isHex) {}

  /**
   * Destructor.
   */
  virtual ~StringDataType() {}

  // @copydoc
  bool dump(OutputFormat outputFormat, size_t length, AppendDivisor appendDivisor, ostream* output) const override;

  // @copydoc
  result_t readRawValue(size_t offset, size_t length, const SymbolString& input,
      unsigned int* value) const override;

  // @copydoc
  result_t readSymbols(size_t offset, size_t length, const SymbolString& input,
      OutputFormat outputFormat, ostream* output) const override;

  // @copydoc
  result_t writeSymbols(size_t offset, size_t length, istringstream* input,
      SymbolString* output, size_t* usedLength) const override;


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
  DateTimeDataType(const string& id, size_t bitCount, uint16_t flags, unsigned int replacement,
      bool hasDate, bool hasTime, int16_t resolution)
    : DataType(id, bitCount, flags|DAT, replacement), m_hasDate(hasDate), m_hasTime(hasTime),
      m_resolution(resolution == 0 ? 1 : resolution) {}

  /**
   * Destructor.
   */
  virtual ~DateTimeDataType() {}

  // @copydoc
  bool dump(OutputFormat outputFormat, size_t length, AppendDivisor appendDivisor, ostream* output) const override;

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
  result_t readRawValue(size_t offset, size_t length, const SymbolString& input,
      unsigned int* value) const override;

  // @copydoc
  result_t readSymbols(size_t offset, size_t length, const SymbolString& input,
      OutputFormat outputFormat, ostream* output) const override;

  // @copydoc
  result_t writeSymbols(const size_t offset, size_t length, istringstream* input,
      SymbolString* output, size_t* usedLength) const override;


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
   * @param baseType the base @a NumberDataType for derived instances, or nullptr.
   */
  NumberDataType(const string& id, size_t bitCount, uint16_t flags, unsigned int replacement,
      unsigned int minValue, unsigned int maxValue, int divisor,
      const NumberDataType* baseType = nullptr)
    : DataType(id, bitCount, flags|NUM, replacement), m_minValue(minValue), m_maxValue(maxValue), m_incValue(0),
      m_divisor(divisor == 0 ? 1 : divisor), m_precision(calcPrecision(divisor)), m_firstBit(0), m_baseType(baseType) {}

  /**
   * Constructs a new instance for multiple of 8 bits with increment value.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set).
   * @param flags the combination of flags (like #BCD).
   * @param replacement the replacement value (no replacement if equal to minValue).
   * @param minValue the minimum raw value.
   * @param maxValue the maximum raw value.
   * @param incValue the smallest step value for increment/decrement, or 0 for auto.
   * @param divisor the divisor (negative for reciprocal).
   * @param baseType the base @a NumberDataType for derived instances, or nullptr.
   */
  NumberDataType(const string& id, size_t bitCount, uint16_t flags, unsigned int replacement,
      unsigned int minValue, unsigned int maxValue, unsigned int incValue, int divisor,
      const NumberDataType* baseType = nullptr)
    : DataType(id, bitCount, flags|NUM, replacement), m_minValue(minValue), m_maxValue(maxValue), m_incValue(incValue),
      m_divisor(divisor == 0 ? 1 : divisor), m_precision(calcPrecision(divisor)), m_firstBit(0), m_baseType(baseType) {}

  /**
   * Constructs a new instance for less than 8 bits.
   * @param id the type identifier.
   * @param bitCount the number of bits (maximum length if #ADJ flag is set).
   * @param flags the combination of flags (like #ADJ, may not include flag #BCD).
   * @param replacement the replacement value (no replacement if zero).
   * @param firstBit the offset to the first bit.
   * @param divisor the divisor (negative for reciprocal).
   * @param baseType the base @a NumberDataType for derived instances, or nullptr.
   */
  NumberDataType(const string& id, size_t bitCount, uint16_t flags, unsigned int replacement,
      int16_t firstBit, int divisor, const NumberDataType* baseType = nullptr)
    : DataType(id, bitCount, flags|NUM, replacement), m_minValue(0), m_maxValue((1 << bitCount)-1), m_incValue(0),
      m_divisor(divisor == 0 ? 1 : divisor), m_precision(0), m_firstBit(firstBit), m_baseType(baseType) {}

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
  static size_t calcPrecision(int divisor);

  // @copydoc
  bool dump(OutputFormat outputFormat, size_t length, AppendDivisor appendDivisor, ostream* output) const override;

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
  virtual result_t derive(int divisor, size_t bitCount, const NumberDataType** derived) const;

  /**
   * Derive a new @a NumberDataType from this.
   * @param min the minimum raw value.
   * @param max the minimum raw value.
   * @param inc the smallest step value for increment/decrement, or 0 to keep the current increment (or calculate
   * automatically).
   * @param derived the derived @a NumberDataType, or this if derivation is
   * not necessary.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t derive(unsigned int min, unsigned int max, unsigned int inc, const NumberDataType** derived) const;

  /**
   * @return the minimum raw value.
   */
  unsigned int getMinValue() const { return m_minValue; }

  /**
   * @return the maximum raw value.
   */
  unsigned int getMaxValue() const { return m_maxValue; }

  /**
   * Get the minimum or maximum value.
   * @param getMax true for the maximum, false for the minimum.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the ostream to append the formatted value to.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t getMinMax(bool getMax, const OutputFormat outputFormat, ostream* output) const;

  /**
   * Check the value against the minimum and maximum value.
   * @param value the raw value.
   * @param negative optional variable in which to store the negative flag.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t checkValueRange(unsigned int value, bool* negative = nullptr) const;

  /**
   * Get the smallest step value for increment/decrement.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the ostream to append the formatted value to.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t getStep(const OutputFormat outputFormat, ostream* output) const;

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
  result_t readRawValue(size_t offset, size_t length, const SymbolString& input,
      unsigned int* value) const override;

  // @copydoc
  result_t readSymbols(size_t offset, size_t length, const SymbolString& input,
      const OutputFormat outputFormat, ostream* output) const override;

  /**
   * Convert the numeric raw value to its float representation (including optional divisor).
   * @param value the numeric raw value.
   * @param output the float variable to write the value to.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t getFloatFromRawValue(unsigned int value, float* output) const;

  /**
   * Convert the float value to the numeric raw value (including optional divisor).
   * @param value the float value.
   * @param output the variable to write the numeric raw value to.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t getRawValueFromFloat(float value, unsigned int* output) const;

  /**
   * Internal method for interpreting a numeric raw value.
   * @param value the numeric raw value.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the ostream to append the formatted value to.
   * @param skipRangeCheck whether to skip the value range check.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t readFromRawValue(unsigned int value,
                            OutputFormat outputFormat, ostream* output, bool skipRangeCheck = false) const;

  /**
   * Internal method for parsing an input string to the coorresponding raw value.
   * @param inputStr the input string to parse the formatted value from.
   * @param parsedValue the variable in which to store the parsed raw value.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t parseInput(const string inputStr, unsigned int* parsedValue) const;

  /**
   * Internal method for writing the numeric raw value to a @a SymbolString.
   * @param value the numeric raw value to write.
   * @param offset the offset in the @a SymbolString.
   * @param length the number of symbols to write, or @a REMAIN_LEN.
   * @param output the @a SymbolString to write the binary value to.
   * @param usedLength the variable in which to store the used length in bytes,
   * or nullptr.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t writeRawValue(unsigned int value, size_t offset, size_t length,
      SymbolString* output, size_t* usedLength) const;

  // @copydoc
  result_t writeSymbols(size_t offset, size_t length, istringstream* input,
      SymbolString* output, size_t* usedLength) const override;


 private:
  /** the minimum raw value. */
  const unsigned int m_minValue;

  /** the maximum raw value. */
  const unsigned int m_maxValue;

  /** the smallest step value for increment/decrement, or 0 for auto. */
  const unsigned int m_incValue;

  /** the divisor (negative for reciprocal). */
  const int m_divisor;

  /** the precision for formatting the value. */
  const size_t m_precision;

  /** the offset to the first bit. */
  const int16_t m_firstBit;

  /** the base @a NumberDataType for derived instances. */
  const NumberDataType* m_baseType;
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
   * Dump the type list optionally including the divisor to the output.
   * @param outputFormat the @a OutputFormat options.
   * @param output the @a ostream to dump to.
   */
  void dump(OutputFormat outputFormat, ostream* output) const;

  /**
   * Removes all @a DataType instances.
   */
  void clear();

  /**
   * Adds a @a DataType instance to this map.
   * @param dataType the @a DataType instance to add.
   * @param derivedKey optional speicla key for derived instances.
   * @return @a RESULT_OK on success, or an error code.
   * Note: the caller may not free the added instance on success.
   */
  result_t add(const DataType* dataType, const string derivedKey = "");

  /**
   * Adds a @a DataType instance for later cleanup.
   * @param dataType the @a DataType instance to add.
   */
  void addCleanup(const DataType* dataType) { m_cleanupTypes.push_back(dataType); }

  /**
   * Gets the @a DataType instance with the specified ID.
   * @param id the ID string (excluding optional length suffix).
   * @param length the length in bytes, or 0 for default.
   * @return the @a DataType instance, or nullptr if not available.
   * Note: the caller may not free the instance.
   */
  const DataType* get(const string& id, size_t length = 0) const;

  /**
   * Returns an iterator pointing to the first ID/@a DataType pair.
   * @return an iterator pointing to the first ID/@a DataType pair.
   */
  map<string, const DataType*>::const_iterator begin() const { return m_typesById.cbegin(); }

  /**
   * Returns an iterator pointing one past the last ID/@a DataType pair.
   * @return an iterator pointing one past the last ID/@a DataType pair.
   */
  map<string, const DataType*>::const_iterator end() const { return m_typesById.cend(); }

 private:
  /** the known @a DataType instances by ID (e.g. "ID:BITS" or just "ID").
   * Note: adjustable length types are stored by ID only. */
  map<string, const DataType*> m_typesById;

  /** the @a DataType instances to cleanup. */
  list<const DataType*> m_cleanupTypes;

  /** the singleton instance. */
  static DataTypeList s_instance;

#ifdef HAVE_CONTRIB
  /** true when contributed datatypes were successfully initialized. */
  static bool s_contrib_initialized;
#endif
};

}  // namespace ebusd

#endif  // LIB_EBUS_DATATYPE_H_

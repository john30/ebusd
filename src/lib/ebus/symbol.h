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

#ifndef LIB_EBUS_SYMBOL_H_
#define LIB_EBUS_SYMBOL_H_

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <queue>
#include <string>
#include <vector>
#include "lib/ebus/result.h"

namespace ebusd {

/** @file lib/ebus/symbol.h
 * Classes, functions, and constants related to symbols on the eBUS.
 *
 * The @a SymbolString class is used for holding a sequence of bytes received
 * from or sent to the bus, as well as calculating and verifying the CRC of a
 * message part.
 *
 * A message on the bus always consists of a command part, i.e. the data sent
 * from a master to the bus. The command part starts with the sending master
 * address followed by the destination address. Both addresses are not allowed
 * to be escaped and whenever a #SYN symbol appears, the sending has to be
 * treated as timed out, as only the auto-SYN generator will do so when there
 * was no symbol on the bus for a certain period of time.
 *
 * The remaining bytes of the command part are the primary and secondary
 * command byte, the number of data bytes, the data bytes themselves, and the
 * final CRC.
 *
 * When the destination is the #BROADCAST address, then the messages consists
 * of the command part only.
 *
 * When the destination address is a master (see @a isMaster()), the receiving
 * master has to acknowledge the correct reception of the command with either
 * the #ACK (if the CRC was valid) or the #NAK symbol (if the received CRC did
 * not match the calculated one). In case of a non-acknowledge #NAK symbol, the
 * command part has to be repeated once (and once only) by the sender.
 *
 * When the destination address is a slave, the receiving slave has to
 * acknowledge the reception of the command as described above. After a
 * positive #ACK symbol, the receiving slave has to send its response data.
 * The response data consists of the number of data bytes, the data bytes
 * themselves, and the final CRC. The sending master has to acknowledge the
 * correct reception of the response as described above and in case of a
 * non-acknowledge, the receiving slave has to repeat its data once.
 */

using std::string;
using std::vector;

/** the base type for symbols sent to/from the eBUS. */
typedef unsigned char symbol_t;

/** escape symbol, either followed by 0x00 for the value 0xA9, or 0x01 for the value 0xAA. */
#define ESC 0xA9

/** synchronization symbol. */
#define SYN 0xAA

/** positive acknowledge symbol. */
#define ACK 0x00

/** negative acknowledge symbol. */
#define NAK 0xFF

/** the broadcast destination address. */
#define BROADCAST 0xFE

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
unsigned int parseInt(const char* str, int base, unsigned int minValue, unsigned int maxValue,
    result_t* result, size_t* length = NULL);

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
int parseSignedInt(const char* str, int base, int minValue, int maxValue,
    result_t* result, size_t* length = NULL);

/**
 * A string of unescaped bus symbols.
 */
class SymbolString {
 protected:
  /**
   * Creates a new empty instance.
   * @param isMaster whether this instance if for the master part.
   */
  explicit SymbolString(bool isMaster = false) { m_isMaster = isMaster; }

 public:
  /**
   * Update the CRC by adding a value.
   * @param value the escaped value to add to the current CRC.
   * @param crc the current CRC to update.
   */
  static void updateCrc(symbol_t value, symbol_t* crc);

  /**
   * Return whether this instance if for the master part.
   * @return whether this instance if for the master part.
   */
  bool isMaster() const { return m_isMaster; }

  /**
   * Parse the hex @a string and add all symbols.
   * @param str the hex @a string.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t parseHex(const string& str);

  /**
   * Parse the escaped hex @a string and add all symbols.
   * @param str the hex @a string.
   * @return @a RESULT_OK on success, or an error code.
   */
  result_t parseHexEscaped(const string& str);

  /**
   * Return the symbols as hex string.
   * @param skipFirstSymbols the number of first symbols to skip.
   * @return the symbols as hex string.
   */
  const string getStr(size_t skipFirstSymbols = 0) const;

  /**
   * Return a reference to the symbol at the specified index.
   * @param index the index of the symbol to return.
   * @return the reference to the symbol at the specified index.
   */
  symbol_t& operator[](const size_t index) {
    if (index >= m_data.size()) {
      m_data.resize(index+1, 0);
    }
    return m_data[index];
  }

  /**
   * Return a reference to the symbol at the specified index.
   * @param index the index of the symbol to return.
   * @return the reference to the symbol at the specified index, or SYN if not available.
   */
  symbol_t operator[](size_t index) const {
    if (index >= m_data.size()) {
      return SYN;
    }
    return m_data[index];
  }

  /**
   * Return whether this instance is equal to the other instance.
   * @param other the other instance.
   * @return true if this instance is equal to the other instance.
   */
  bool operator == (const SymbolString& other) {
    return m_isMaster == other.m_isMaster && m_data == other.m_data;
  }

  /**
   * Return whether this instance is different from the other instance.
   * @param other the other instance.
   * @return true if this instance is different from the other instance.
   */
  bool operator != (const SymbolString& other) {
    return m_isMaster != other.m_isMaster || m_data != other.m_data;
  }

  /**
   * Compare the data in this instance to that of the other instance.
   * @param other the other instance.
   * @return 0 if the data is equal,
   * 1 if the data is completely different,
   * 2 if both instances are a master part and the data only differs in the first byte (the master address).
   */
  int compareTo(const SymbolString& other) const {
    if (m_data.size() != other.m_data.size() || m_isMaster != other.m_isMaster) {
      return 1;
    }
    if (m_data == other.m_data) {
      return 0;
    }
    if (!m_isMaster) {
      return 1;
    }
    if (m_data.size() == 1) {
      return 2;
    }
    if (equal(m_data.begin()+1, m_data.end(), other.m_data.begin()+1)) {
      return 2;
    }
    return 1;
  }

  /**
   * Append a symbol to the end of the symbol string.
   * @param value the symbol to append.
   */
  void push_back(symbol_t value) { m_data.push_back(value); }

  /**
   * Return the number of symbols in this symbol string.
   * @return the number of available symbols.
   */
  size_t size() const { return m_data.size(); }

  /**
   * Adjust the header NN field to the number of data bytes DD.
   * @return true on success, false if the number of data bytes DD is too big.
   */
  bool adjustHeader() {
    size_t lengthOffset = (m_isMaster ? 4 : 0);
    if (m_data.size() <= lengthOffset) {
      m_data.resize(lengthOffset+1);
    } else if (m_data.size() >= lengthOffset+255) {
      return false;
    }
    m_data[lengthOffset] = (symbol_t)(m_data.size() - 1 - lengthOffset);
    return true;
  }

  /**
   * Return the offset to the first data byte DD.
   * @return the offset to the first data byte DD.
   */
  size_t getDataOffset() const { return m_isMaster ? 5 : 1; }

  /**
   * Return the number of effectively available data bytes DD.
   * @return the number of effectively available data bytes DD.
   */
  size_t getDataSize() const {
    size_t lengthOffset = (m_isMaster ? 4 : 0);
    if (m_data.size() <= lengthOffset) {
      return 0;
    }
    size_t ret = m_data[lengthOffset];
    return m_data.size() < lengthOffset + 1 + ret ? m_data.size() - lengthOffset - 1 : ret;
  }

  /**
   * Return the data byte at the specified index (within DD).
   * @param index the index of the data byte (within DD) to return.
   * @return the data byte at the specified index, or 0 if not available.
   */
  symbol_t dataAt(size_t index) const {
    size_t offset = (m_isMaster ? 5 : 1) + index;
    if (offset < m_data.size()) {
      return m_data[offset];
    }
    return 0;
  }

  /**
   * Return a reference to the data byte at the specified index (within DD).
   * @param index the index of the data byte (within DD) to return.
   * @return the reference to the data byte at the specified index.
   */
  symbol_t& dataAt(size_t index) {
    size_t offset = (m_isMaster ? 5 : 1) + index;
    if (offset >= m_data.size()) {
      m_data.resize(offset+1, 0);
    }
    return m_data[offset];
  }

  /**
   * Return whether the byte sequence is complete with regard to the header and length field.
   * @return true if the sequence is complete.
   */
  bool isComplete() {
    size_t lengthOffset = (m_isMaster ? 4 : 0);
    if (m_data.size() < lengthOffset + 1) {
      return false;
    }
    return m_data.size() >= lengthOffset + 1 + m_data[lengthOffset];
  }

  /**
   * Calculate the CRC.
   * @return the calculated CRC.
   */
  symbol_t calcCrc() const;

  /**
   * Clear the symbols.
   */
  void clear() { m_data.clear(); }


 private:
  /**
   * Hidden copy constructor.
   * @param str the @a SymbolString to copy from.
   */
  SymbolString(const SymbolString& str)
    : m_data(str.m_data), m_isMaster(str.m_isMaster) {}

  /** the string of unescaped symbols. */
  vector<symbol_t> m_data;

  /** whether this instance is for the master part. */
  bool m_isMaster;
};


/**
 * A string of unescaped master bus symbols.
 */
class MasterSymbolString : public SymbolString {
 public:
  /**
   * Creates a new empty instance.
   */
  MasterSymbolString() : SymbolString(true) {}
};


/**
 * A string of unescaped slave bus symbols.
 */
class SlaveSymbolString : public SymbolString {
 public:
  /**
   * Creates a new empty instance.
   */
  SlaveSymbolString() : SymbolString(false) {}
};


/**
 * Return whether the address is one of the 25 master addresses.
 * @param addr the address to check.
 * @return <code>true</code> if the specified address is a master address.
 */
bool isMaster(symbol_t addr);

/**
 * Return whether the address is a slave address of one of the 25 masters.
 * @param addr the address to check.
 * @return <code>true</code> if the specified address is a slave address of a master.
 */
bool isSlaveMaster(symbol_t addr);

/**
 * Return the slave address associated with the specified address (master or slave).
 * @param addr the address to check.
 * @return the slave address, or SYN if the specified address is neither a master address nor a slave address of a
 * master.
 */
symbol_t getSlaveAddress(symbol_t addr);

/**
 * Return the master address associated with the specified address (master or slave).
 * @param addr the address to check.
 * @return the master address, or SYN if the specified address is neither a master address nor a slave address of a
 * master.
 */
symbol_t getMasterAddress(symbol_t addr);

/**
 * Return the number of the master if the address is a valid bus address.
 * @param addr the bus address.
 * @return the number of the master if the address is a valid bus address (1 to 25), or 0.
 */
unsigned int getMasterNumber(symbol_t addr);

/**
 * Return whether the address is a valid bus address.
 * @param addr the address to check.
 * @param allowBroadcast whether to also allow @a addr to be the broadcast address (default true).
 * @return <code>true</code> if the specified address is a valid bus address.
 */
bool isValidAddress(symbol_t addr, bool allowBroadcast = true);

}  // namespace ebusd

#endif  // LIB_EBUS_SYMBOL_H_

/*
 * Copyright (C) John Baier 2014 <john@johnm.de>
 *
 * This file is part of libebus.
 *
 * libebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libebus. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBEBUS_SYMBOL_H_
#define LIBEBUS_SYMBOL_H_

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <queue>

namespace libebus
{


static const unsigned char ESC = 0xA9;       // escape symbol, either followed by 0x00 for the value 0xA9, or 0x01 for the value 0xAA
static const unsigned char SYN = 0xAA;       // synchronization symbol
static const unsigned char ACK = 0x00;       // positive acknowledge
static const unsigned char NAK = 0xFF;       // negative acknowledge
static const unsigned char BROADCAST = 0xFE; // the broadcast destination address


/**
 * @brief A string of bus symbols.
 */
class SymbolString
{
public:
	/**
	 * @brief Creates a new empty SymbolString.
	 */
	SymbolString() : m_crc(0) {}
	/**
	 * @brief Creates a new escaped SymbolString from an unescaped hex string and adds the calculated CRC.
	 * @param str the unescaped hex string.
	 */
	SymbolString(const std::string str);
	/**
	 * @brief Creates a new unescaped SymbolString from a hex string.
	 * @param escaped whether the hex string is escaped and shall be unescaped.
	 * @param str the hex string.
	 */
	SymbolString(const std::string str, bool escaped);
	/**
	 * @brief Returns the symbols as hex string.
	 * @param escaped whether to unescape the symbols.
	 * @return the symbols as hex string.
	 */
	const std::string getDataStr(bool unescape=false);
	/**
	 * @brief Returns the symbol at the specified index.
	 * @param index the index of the symbol to return.
	 * @return the symbol at the specified index.
	 * @throw std::out_of_range if @a index is invalid.
	 */
	unsigned char at(const size_t index) { return m_data.at(index); }
	/**
	 * @brief Returns the symbol at the specified index.
	 * @param index the index of the symbol to return.
	 * @return the symbol at the specified index.
	 */
	unsigned char operator[](const size_t index) { return m_data[index]; }
	/**
	 * @brief Returns the symbol at the specified index.
	 * @param index the index of the symbol to return.
	 * @return the symbol at the specified index.
	 */
	unsigned char operator[](const size_t index) const { return m_data[index]; }
	/**
	 * @brief Inserts a the symbol at the specified index.
	 * @param index the index at which to insert the symbol.
	 * @param value the symbol to insert.
	 */
	void insert(const size_t index, const unsigned char value) { m_data.insert(m_data.begin()+index, value); }
	/**
	 * @brief Appends a the symbol to the end of the symbol string and escapes it if necessary.
	 * @param value the symbol to append.
	 * @param updateCrc whether to update the calculated CRC in @a m_crc.
	 */
	void push_back_escape(const unsigned char value, bool updateCRC=true);
	/**
	 * @brief Appends a the symbol to the end of the symbol string and unescapes it.
	 * @param value the symbol to append.
	 * @param previousEscape whether the previous value was the escape symbol (set to false for the initial call).
	 * @param updateCrc whether to update the calculated CRC in @a m_crc.
	 * @return if previousEscape is false on return: the unescaped symbol. otherwise: zero if the escape sequence was invalid, one if the escape sequence is not yet finished.
	 */
	unsigned char push_back_unescape(const unsigned char value, bool& previousEscape, bool updateCRC=true);
	/**
	 * @brief Returns the number of symbols in this symbol string.
	 * @return the number of available symbols.
	 */
	size_t size() const { return m_data.size(); }
	/**
	 * @brief Returns the calculated CRC.
	 * @return the calculated CRC.
	 */
	unsigned char getCRC() const { return m_crc; }
	/**
	 * @brief Clears the symbols.
	 */
	void clear() { m_crc=0; m_data.clear(); }

private:
	/**
	 * @brief Updates the calculated CRC in @a m_crc by adding a value.
	 * @param value the (escaped) value to add to the calculated CRC in @a m_crc.
	 */
	void addCRC(const unsigned char value);

	/**
	 * @brief the string of bus symbols.
	 */
	std::vector<unsigned char> m_data;
	/**
	 * @brief the calculated CRC.
	 */
	unsigned char m_crc;
};


/**
 * Returns whether the address is one of the 25 master addresses.
 * @param addr the address to check.
 * @return <code>true</code> if the specified address is a master address.
 */
bool isMaster(unsigned char addr);


} //namespace

#endif // LIBEBUS_SYMBOL_H_

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
#include <string>
#include <vector>

namespace libebus
{


/** the message part in which a data field is stored. */
enum PartType {
			pt_masterData, // stored in master data
			//pt_slaveAck,   // stored in slave acknowledge // TODO implement if reasonable
			pt_slaveData,  // stored in slave data
			//pt_masterAck   // stored in master acknowledge // TODO implement if reasonable
			};

/** the available data types. */
enum DataType {
			dt_string, // string of characters with fixed length>0, filled up with space
			dt_hex,    // string of hex digits with fixed length>0, separated by space
			dt_bcd,    // length: 1 byte, binary coded number, string format: "%d"
			dt_uchar,
			dt_schar,
			dt_uint,
			dt_sint,
			dt_ulong,
			dt_slong,
			dt_float,
			dt_d1b,
			dt_d1c,
			dt_d2b,
			dt_d2c,
			dt_date,  // length: 4 bytes (with skipped weekday) or 3 bytes (without weekday), string format: "dd.mm.yyyy"
			dt_day,   // length: 1 byte, string format: "www" (3 character weekday name)
			dt_time,  // length: 3 bytes, string format: "hh:mm"
			dt_tTime  // length: 1 byte, string format: "hh:mm"
			};


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
	 * @param dataType the data type.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 */
	DataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const DataType dataType, const std::string unit,
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
	 * @param isSetMessage whther the field is part of a set message.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 */
	static DataField* create(const unsigned char dstAddress, const bool isSetMessage,
			std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end);

	/**
	 * @brief Parses the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString to parse from.
	 * @param slaveData the unescaped slave data @a SymbolString to parse from.
	 * @return the parsed value as string.
	 */
	const std::string parseSymbols(SymbolString& masterData, SymbolString& slaveData, bool verbose=false);
	/**
	 * @brief Formats the value to the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString to format to.
	 * @param slaveData the unescaped slave data @a SymbolString to format to.
	 * @param value the value as string.
	 */
	bool formatSymbols(const std::string& value, SymbolString& masterData, SymbolString& slaveData);

protected:
	/**
	 * @brief Internal method doing the actual parse for the individual data type.
	 * @param data the unescaped data SymbolString to parse from.
	 * @param output the ostringstream to append to.
	 * @return true if the value was parsed successfully.
	 */
	virtual bool parse(SymbolString& data, std::ostringstream& output) = 0;
	/**
	 * @brief Internal method doing the actual format for the individual data type.
	 * @param data the unescaped data SymbolString to format to.
	 * @param input the istringstream to interpret.
	 * @return true if the value was formatted successfully.
	 */
	virtual bool format(SymbolString& data, std::istringstream& input) = 0;

	/** the field name. */
	const std::string m_name;
	/** the message part in which the field is stored. */
	const PartType m_partType;
	/** the offset to the first symbol in the message part in which the field is stored. */
	const unsigned char m_offset;
	/** the number of symbols in the message part in which the field is stored. */
	const unsigned char m_length;
	/** the data type. */
	const DataType m_dataType;
	//std::vector<std::string> m_valid;//TODO add list of possible values
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
	 * @param dataType the data type.
	 * @param unit the value unit.
	 * @param comment the field comment.
	 */
	StringDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const DataType dataType, const std::string unit,
			const std::string comment)
		: DataField(name, partType, offset, length, dataType, unit, comment) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~StringDataField() {}

protected:
	virtual bool parse(SymbolString& data, std::ostringstream& output);
	virtual bool format(SymbolString& data, std::istringstream& input);

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
	 * @param dataType the data type.
	 * @param comment the field comment.
	 * @param unit the value unit.
	 * @param factor the factor to apply on the value.
	 * @param replacement the (binary) replacement value to use if the value is not set.
	 */
	NumericDataField(const std::string name, const PartType partType,
			const unsigned char offset, const unsigned char length,
			const DataType dataType, const std::string unit,
			const std::string comment, const float factor,
			const unsigned int replacement)
		: DataField(name, partType, offset, length, dataType, unit, comment),
		  m_factor(factor), m_replacement(replacement) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~NumericDataField() {}

protected:
	virtual bool parse(SymbolString& data, std::ostringstream& output);
	virtual bool format(SymbolString& data, std::istringstream& input);

	/** the factor to apply on the value. */
	const float m_factor;
	/** the (binary) replacement value to use if the value is not set. */
	const unsigned int m_replacement;

};


} //namespace

#endif // LIBEBUS_DATA_H_

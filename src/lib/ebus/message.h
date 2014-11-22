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

#ifndef LIBEBUS_MESSAGE_H_
#define LIBEBUS_MESSAGE_H_

#include "data.h"
#include "result.h"
#include "symbol.h"
#include <string>
#include <vector>

/**
 * @brief Base class for all kinds of bus messages.
 */
class Message
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param class the optional device class.
	 * @param name the message name (unique within the same class and type).
	 * @param isSetMessage whether this is a set message.
	 * @param isActiveMessage true if message can be initiated by the daemon
	 * itself any any other participant, false if message can only be initiated
	 * by a participant other than the daemon.
	 * @param comment the comment.
	 * @param srcAddress the source address (optional if passive), or @a SYN for any.
	 * @param dstAddress the destination address.
	 * @param id the primary, secondary, and optional further ID bytes.
	 * @param data the @a DataField for encoding/decoding the message.
	 * @param pollPriority the priority for polling, or 0 for no polling at all.
	 */
	Message(const std::string clazz, const std::string name, const bool isSetMessage,
			const bool isActiveMessage, const std::string comment,
			const unsigned char srcAddress, const unsigned char dstAddress,
			const std::vector<unsigned char> id, DataField* data,
			const unsigned int pollPriority)
		: m_class(clazz), m_name(name), m_isSetMessage(isSetMessage),
		  m_isActiveMessage(isActiveMessage), m_comment(comment),
		  m_srcAddress(srcAddress), m_dstAddress(dstAddress),
		  m_id(id), m_data(data), m_pollPriority(pollPriority) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~Message() { delete m_data; }
	/**
	 * @brief Factory method for creating a new instance.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param templates a map of @a DataField templates to be referenced by name.
	 * @param returnValue the variable in which to store the created instance.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(std::vector<std::string>::iterator& it, const std::vector<std::string>::iterator end,
			const std::map<std::string, DataField*> templates, Message*& returnValue);
	/**
	 * @brief Get the optional device class.
	 * @return the optional device class.
	 */
	std::string getClass() const { return m_class; }
	/**
	 * @brief Get the message name (unique within the same class and type).
	 * @return the message name (unique within the same class and type).
	 */
	std::string getName() const { return m_name; }
	/**
	 * @brief Get whether this is a set message.
	 * @return whether this is a set message.
	 */
	bool isSetMessage() const { return m_isSetMessage; }
	/**
	 * @brief Get whether message can be initiated by the daemon itself and any other
	 * participant.
	 * @return true if message can be initiated by the daemon itself and any other
	 * participant, false if message can only be initiated by a participant
	 * other than the daemon.
	 */
	bool isActiveMessage() const { return m_isActiveMessage; }
	/**
	 * @brief Get the comment.
	 * @return the comment.
	 */
	std::string getComment() const { return m_comment; }
	/**
	 * @brief Get the source address.
	 * @return the source address, or @a SYN for any.
	 */
	unsigned char getSrcAddress() const { return m_srcAddress; }
	/**
	 * @brief Get the destination address.
	 * @return the destination address.
	 */
	unsigned char getDstAddress() const { return m_dstAddress; }
	/**
	 * @brief Get the command ID bytes.
	 * @return the primary, secondary, and optionally further command ID bytes.
	 */
	std::vector<unsigned char> getId() const { return m_id; }
	/**
	 * @brief Reads the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the @a std::ostringstream to append the formatted value to.
	 * @param verbose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	//result_t read(SymbolString& masterData, SymbolString& slaveData, std::ostringstream& output,
	//		bool verbose=false, char separator=';') = 0;
	/**
	 * @brief Writes the value to the master or slave @a SymbolString.
	 * @param input the @a std::istringstream to parse the formatted value from.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t prepare(const unsigned char srcAddress, SymbolString& masterData,
			std::istringstream& input, char separator=';');
	result_t handle(SymbolString& masterData, SymbolString& slaveData,
			std::ostringstream& output, char separator=';', bool answer=false);


private:

	 /** the optional device class. */
	const std::string m_class;
	/** the message name (unique within the same class and type). */
	const std::string m_name;
	/** whether this is a set message. */
	const bool m_isSetMessage;
	/** true if message can be initiated by the daemon itself and any other
	 * participant, false if message can only be initiated by a participant
	 * other than the daemon. */
	const bool m_isActiveMessage;
	/** the comment. */
	const std::string m_comment;
	/** the source address (optional if passive), or @a SYN for any. */
	const unsigned char m_srcAddress;
	/** the destination address. */
	const unsigned char m_dstAddress;
	/** the primary, secondary, and optionally further command ID bytes. */
	const std::vector<unsigned char> m_id;
	/** the @a DataField for encoding/decoding the message. */
	DataField* m_data;
	/** the priority for polling, or 0 for no polling at all. */
	const unsigned char m_pollPriority;

};

#endif // LIBEBUS_MESSAGE_H_

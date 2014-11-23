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
#include <map>

using namespace std;

/**
 * @brief Defines parameters of a message sent or received on the bus.
 */
class Message
{
public:

	/**
	 * @brief Constructs a new instance.
	 * @param class the optional device class.
	 * @param name the message name (unique within the same class and type).
	 * @param isSet whether this is a set message.
	 * @param isActive true if message can be initiated by the daemon
	 * itself any any other participant, false if message can only be initiated
	 * by a participant other than the daemon.
	 * @param comment the comment.
	 * @param srcAddress the source address (optional if passive), or @a SYN for any.
	 * @param dstAddress the destination address.
	 * @param id the primary, secondary, and optional further ID bytes.
	 * @param data the @a DataField for encoding/decoding the message.
	 * @param pollPriority the priority for polling, or 0 for no polling at all.
	 */
	Message(const string clazz, const string name, const bool isSet,
			const bool isActive, const string comment,
			const unsigned char srcAddress, const unsigned char dstAddress,
			const vector<unsigned char> id, DataField* data,
			const unsigned int pollPriority);
	/**
	 * @brief Destructor.
	 */
	virtual ~Message() { delete m_data; }
	/**
	 * @brief Factory method for creating a new instance.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param templates the @a DataFieldTemplates to be referenced by name, or NULL.
	 * @param returnValue the variable in which to store the created instance.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(vector<string>::iterator& it, const vector<string>::iterator end,
			DataFieldTemplates*, Message*& returnValue);
	/**
	 * @brief Get the optional device class.
	 * @return the optional device class.
	 */
	string getClass() const { return m_class; }
	/**
	 * @brief Get the message name (unique within the same class and type).
	 * @return the message name (unique within the same class and type).
	 */
	string getName() const { return m_name; }
	/**
	 * @brief Get whether this is a set message.
	 * @return whether this is a set message.
	 */
	bool isSet() const { return m_isSet; }
	/**
	 * @brief Get whether message can be initiated by the daemon itself and any other
	 * participant.
	 * @return true if message can be initiated by the daemon itself and any other
	 * participant, false if message can only be initiated by a participant
	 * other than the daemon.
	 */
	bool isActive() const { return m_isActive; }
	/**
	 * @brief Get the comment.
	 * @return the comment.
	 */
	string getComment() const { return m_comment; }
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
	vector<unsigned char> getId() const { return m_id; }
	/**
	 * @brief Returns the key for storing in @a MessageSet.
	 * @return the key for storing in @a MessageSet.
	 */
	unsigned long long getKey() { return m_key; }
	/**
	 * @brief Reads the value from the master or slave @a SymbolString.
	 * @param masterData the unescaped master data @a SymbolString for reading binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for reading binary data.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param verbose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	//result_t read(SymbolString& masterData, SymbolString& slaveData, ostringstream& output,
	//		bool verbose=false, char separator=';') = 0;
	/**
	 * @brief Writes the value to the master or slave @a SymbolString.
	 * @param input the @a istringstream to parse the formatted value from.
	 * @param masterData the unescaped master data @a SymbolString for writing binary data.
	 * @param slaveData the unescaped slave data @a SymbolString for writing binary data.
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t prepare(const unsigned char srcAddress, SymbolString& masterData,
			istringstream& input, char separator=';');
	result_t handle(SymbolString& masterData, SymbolString& slaveData,
			ostringstream& output, char separator=';', bool answer=false);

private:

	 /** the optional device class. */
	const string m_class;
	/** the message name (unique within the same class and type). */
	const string m_name;
	/** whether this is a set message. */
	const bool m_isSet;
	/** true if message can be initiated by the daemon itself and any other
	 * participant, false if message can only be initiated by a participant
	 * other than the daemon. */
	const bool m_isActive;
	/** the comment. */
	const string m_comment;
	/** the source address (optional if passive), or @a SYN for any. */
	const unsigned char m_srcAddress;
	/** the destination address. */
	const unsigned char m_dstAddress;
	/** the primary, secondary, and optionally further command ID bytes. */
	const vector<unsigned char> m_id;
	/** the key for storing in @a MessageSet. */
	unsigned long long m_key;
	/** the @a DataField for encoding/decoding the message. */
	DataField* m_data;
	/** the priority for polling, or 0 for no polling at all. */
	const unsigned char m_pollPriority;

};

/**
 * @brief Holds a map of all known @a Message instances.
 */
class MessageMap
{
public:

	/**
	 * @brief Constructs a new instance.
	 */
	MessageMap() : m_maxIdLength(0) {}
	/**
	 * @brief Destructor.
	 */
	virtual ~MessageMap() { clear(); }
	/**
	 * @brief Adds a @a Message instance to this set.
	 * @param message the @a Message instance to add.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller may not free the added instance on success.
	 */
	result_t add(Message* message);
	/**
	 * @brief Finds the @a Message instance for the specified class and name.
	 * @param master the master @a SymbolString for identifying the @a Message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(const string clazz, const string name, const bool isActive, const bool isSet);
	/**
	 * @brief Finds the @a Message instance for the specified master data.
	 * @param master the master @a SymbolString for identifying the @a Message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(SymbolString master);
	/**
	 * @brief Removes all @a Message instances.
	 */
	void clear();

private:

	/** the maximum ID length used by any of the known @a Message instances. */
	unsigned char m_maxIdLength;

	/** the known @a Message instances by class and name. */
	map<string, Message*> m_messagesByName;

	/** the known passive @a Message instances by key. */
	map<unsigned long long, Message*> m_passiveMessagesByKey;

};

#endif // LIBEBUS_MESSAGE_H_

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
#include <deque>
#include <map>

/** \file message.h */

using namespace std;

class MessageMap;

/**
 * @brief Defines parameters of a message sent or received on the bus.
 */
class Message
{
	friend class MessageMap;
public:

	/**
	 * @brief Construct a new instance.
	 * @param clazz the optional device class.
	 * @param name the message name (unique within the same class and type).
	 * @param isSet whether this is a set message.
	 * @param isPassive true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 * @param comment the comment.
	 * @param srcAddress the source address, or @a SYN for any (only relevant if passive).
	 * @param dstAddress the destination address, or @a SYN for any (set later).
	 * @param id the primary, secondary, and optional further ID bytes.
	 * @param data the @a DataField for encoding/decoding the message.
	 * @param pollPriority the priority for polling, or 0 for no polling at all.
	 */
	Message(const string clazz, const string name, const bool isSet,
			const bool isPassive, const string comment,
			const unsigned char srcAddress, const unsigned char dstAddress,
			const vector<unsigned char> id, DataField* data,
			const unsigned int pollPriority);

	/**
	 * @brief Construct a new temporary instance.
	 * @param isSet whether this is a set message.
	 * @param isPassive true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 * @param pb the primary ID byte.
	 * @param sb the secondary ID byte.
	 * @param data the @a DataField for encoding/decoding the message.
	 */
	Message(const bool isSet, const bool isPassive,
			const unsigned char pb, const unsigned char sb,
			DataField* data);

	/**
	 * @brief Destructor.
	 */
	virtual ~Message() { delete m_data; }

	/**
	 * @brief Factory method for creating a new instance.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param defaultsRows a @a vector with rows containing defaults, or NULL.
	 * @param templates the @a DataFieldTemplates to be referenced by name, or NULL.
	 * @param returnValue the variable in which to store the created instance.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instance.
	 */
	static result_t create(vector<string>::iterator& it, const vector<string>::iterator end,
			vector< vector<string> >* defaultsRows,
			DataFieldTemplates* templates, Message*& returnValue);

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
	 * @brief Get whether message can be initiated only by a participant other than us.
	 * @return true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 */
	bool isPassive() const { return m_isPassive; }

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
	 * @return the destination address, or @a SYN for any.
	 */
	unsigned char getDstAddress() const { return m_dstAddress; }

	/**
	 * @brief Get the command ID bytes.
	 * @return the primary, secondary, and optionally further command ID bytes.
	 */
	vector<unsigned char> getId() const { return m_id; }

	/**
	 * @brief Return the key for storing in @a MessageMap.
	 * @return the key for storing in @a MessageMap.
	 */
	unsigned long long getKey() { return m_key; }

	/**
	 * @brief Get the polling priority, or 0 for no polling at all.
	 * @return the polling priority, or 0 for no polling at all.
	 */
	unsigned char getPollPriority() const { return m_pollPriority; }

	/**
	 * @brief Prepare the master @a SymbolString for sending a query or command to the bus.
	 * @param srcAddress the source address to set.
	 * @param masterData the master data @a SymbolString for writing symbols to.
	 * @param input the @a istringstream to parse the formatted value(s) from.
	 * @param separator the separator character between multiple fields.
	 * @param dstAddress the destination address to set, or @a SYN to keep the address defined during construction.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t prepareMaster(const unsigned char srcAddress, SymbolString& masterData,
			istringstream& input, char separator=UI_FIELD_SEPARATOR,
			const unsigned char dstAddress=SYN);

	/**
	 * @brief Prepare the slave @a SymbolString for sending an answer to the bus.
	 * @param slaveData the slave data @a SymbolString for writing symbols to.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t prepareSlave(SymbolString& slaveData);

	/**
	 * @brief Decode a received message.
	 * @param partType the @a PartType of the data.
	 * @param data the unescaped data @a SymbolString for reading binary data.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param leadingSeparator whether to prepend a separator before the formatted value.
	 * @param verbose whether to prepend the name, append the unit (if present), and append
	 * the comment in square brackets (if present).
	 * @param filterName the optional name of a field to limit the output to.
	 * @param separator the separator character between multiple fields.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t decode(const PartType partType, SymbolString& data,
			ostringstream& output, bool leadingSeparator=false,
			bool verbose=false, const char* filterName=NULL,
			char separator=UI_FIELD_SEPARATOR);

	/**
	 * @brief Get the last decoded value.
	 * @return the last decoded value, or the empty string if it was not successful.
	 */
	string getLastValue() { return m_lastValue; }

	/**
	 * @brief Get the time when @a m_lastValue was updated.
	 * @return the time when @a m_lastValue was updated, or 0 if this message was not decoded yet.
	 */
	time_t getLastUpdateTime() { return m_lastUpdateTime; }

	/**
	 * @brief Get the time when this message was last polled for.
	 * @return the time when this message was last polled for, or 0 for never.
	 */
	time_t getLastPollTime() { return m_lastPollTime; }

	/**
	 * @brief Return whether this @a Message needs to be polled before the other one.
	 * @param other the other @a Message to compare with.
	 * @return true if this @a Message needs to be polled before the other one.
	 */
	bool isLessPollWeight(Message* other);

private:

	/** the optional device class. */
	const string m_class;

	/** the message name (unique within the same class and type). */
	const string m_name;

	/** whether this is a set message. */
	const bool m_isSet;

	/** true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant. */
	const bool m_isPassive;

	/** the comment. */
	const string m_comment;

	/** the source address, or @a SYN for any (only relevant if passive). */
	const unsigned char m_srcAddress;

	/** the destination address. */
	const unsigned char m_dstAddress;

	/** the primary, secondary, and optionally further command ID bytes. */
	vector<unsigned char> m_id;

	/** the key for storing in @a MessageMap. */
	unsigned long long m_key;

	/** the @a DataField for encoding/decoding the message. */
	DataField* m_data;

	/** the priority for polling, or 0 for no polling at all. */
	const unsigned char m_pollPriority;

	/** the last decoded value. */
	string m_lastValue;

	/** the system time when @a m_lastValue was updated, 0 for never. */
	time_t m_lastUpdateTime;

	/** the number of times this messages was already polled for. */
	unsigned int m_pollCount;

	/** the system time when this message was last polled for, 0 for never. */
	time_t m_lastPollTime;

};


/**
 * @brief A function that compares the weighted poll priority of two @a Message instances.
 */
struct compareMessagePriority : binary_function <Message*,Message*,bool> {
	/**
	 * @brief Compare the weighted poll priority of the two @a Message instances.
	 * @param x the first @a Message.
	 * @param y the second @a Message.
	 * @return whether @a x is bigger than or equal to @a y with regard to their weighted poll priority.
	 */
	bool operator() (Message* x, Message* y) const { return x->isLessPollWeight(y) == false; };
};


/**
 * @brief Holds a map of all known @a Message instances.
 */
class MessageMap : public FileReader<DataFieldTemplates*>
{
public:

	/**
	 * @brief Construct a new instance.
	 */
	MessageMap() : FileReader<DataFieldTemplates*>::FileReader(true),
		m_minIdLength(4), m_maxIdLength(0), m_messageCount(0), m_passiveMessageCount(0) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~MessageMap() { clear(); }

	/**
	 * @brief Add a @a Message instance to this set.
	 * @param message the @a Message instance to add.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller may not free the added instance on success.
	 */
	result_t add(Message* message);

	// @copydoc
	virtual result_t addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end, DataFieldTemplates* arg, vector< vector<string> >* defaults, const string& filename, unsigned int lineNo);

	/**
	 * @brief Find the @a Message instance for the specified class and name.
	 * @param clazz the optional device class.
	 * @param name the message name.
	 * @param isSet whether this is a set message.
	 * @param isPassive whether this is a passive message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(const string& clazz, const string& name, const bool isSet, const bool isPassive=false);

	/**
	 * @brief Find all active get @a Message instances for the specified class and name.
	 * @param clazz the device class, or empty for any.
	 * @param name the message name, or empty for any.
	 * @param pb the primary ID byte, or -1 for any.
	 * @param completeMatch false to also include messages where the class and name matches only a part of the given class and name.
	 * @return the found @a Message instances.
	 * Note: the caller may not free the returned instances.
	 */
	deque<Message*> findAll(const string& clazz, const string& name, const short pb, const bool completeMatch=true);

	/**
	 * @brief Find the @a Message instance for the specified master data.
	 * @param master the master @a SymbolString for identifying the @a Message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(SymbolString& master);

	/**
	 * @brief Removes all @a Message instances.
	 */
	void clear();

	/**
	 * @brief Get the number of stored @a Message instances.
	 * @param passiveOnly true to count only passive messages, false to count all messages.
	 * @return the the number of stored @a Message instances.
	 */
	size_t size(const bool passiveOnly=false) { return passiveOnly ? m_passiveMessageCount : m_messageCount; }

	/**
	 * @brief Get the number of stored @a Message instances with a poll priority.
	 * @return the the number of stored @a Message instances with a poll priority.
	 */
	size_t sizePoll() { return m_pollMessages.size(); }

	/**
	 * @brief Get the next @a Message to poll.
	 * @return the next @a Message to poll, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* getNextPoll();

private:

	/** the minimum ID length used by any of the known @a Message instances. */
	unsigned char m_minIdLength;

	/** the maximum ID length used by any of the known @a Message instances. */
	unsigned char m_maxIdLength;

	/** the number of distinct @a Message instances stored in @a m_messagesByName. */
	size_t m_messageCount;

	/** the number of distinct passive @a Message instances stored in @a m_messagesByKey. */
	size_t m_passiveMessageCount;

	/** the known @a Message instances by class and name. */
	map<string, Message*> m_messagesByName;

	/** the known @a Message instances by key. */
	map<unsigned long long, Message*> m_messagesByKey;

	/** the known @a Message instances to poll, by priority. */
	priority_queue<Message*, vector<Message*>, compareMessagePriority> m_pollMessages;

};

#endif // LIBEBUS_MESSAGE_H_

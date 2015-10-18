/*
 * Copyright (C) John Baier 2014-2015 <ebusd@ebusd.eu>
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

class Condition;
class SimpleCondition;
class CombinedCondition;
class MessageMap;

/**
 * Defines parameters of a message sent or received on the bus.
 */
class Message
{
	friend class MessageMap;
public:

	/**
	 * Construct a new instance.
	 * @param circuit the optional circuit name.
	 * @param name the message name (unique within the same circuit and type).
	 * @param isWrite whether this is a write message.
	 * @param isPassive true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 * @param comment the comment.
	 * @param srcAddress the source address, or @a SYN for any (only relevant if passive).
	 * @param dstAddress the destination address, or @a SYN for any (set later).
	 * @param id the primary, secondary, and optional further ID bytes.
	 * @param data the @a DataField for encoding/decoding the message.
	 * @param deleteData whether to delete the @a DataField during destruction.
	 * @param pollPriority the priority for polling, or 0 for no polling at all.
	 * @param condition the @a Condition for this message, or NULL.
	 */
	Message(const string circuit, const string name, const bool isWrite,
			const bool isPassive, const string comment,
			const unsigned char srcAddress, const unsigned char dstAddress,
			const vector<unsigned char> id,
			DataField* data, const bool deleteData,
			const unsigned char pollPriority,
			Condition* condition=NULL);

	/**
	 * Construct a new temporary instance.
	 * @param isWrite whether this is a write message.
	 * @param isPassive true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 * @param pb the primary ID byte.
	 * @param sb the secondary ID byte.
	 * @param data the @a DataField for encoding/decoding the message.
	 * @param condition the @a Condition for this message, or NULL.
	 */
	Message(const bool isWrite, const bool isPassive,
			const unsigned char pb, const unsigned char sb,
			DataField* data,
			Condition* condition=NULL);

	/**
	 * Destructor.
	 */
	virtual ~Message() { if (m_deleteData) delete m_data; }

	/**
	 * Factory method for creating new instances.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param defaultsRows a @a vector with rows containing defaults, or NULL.
	 * @param condition the @a Condition instance for the message, or NULL.
	 * @param filename the name of the file being read.
	 * @param templates the @a DataFieldTemplates to be referenced by name, or NULL.
	 * @param messages the @a vector to which to add created instances.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller needs to free the created instances.
	 */
	static result_t create(vector<string>::iterator& it, const vector<string>::iterator end,
			vector< vector<string> >* defaultsRows, Condition* condition, const string& filename,
			DataFieldTemplates* templates, vector<Message*>& messages);

	/**
	 * Derive a new @a Message from this message.
	 * @param dstAddress the new destination address.
	 * @return the derived @a Message instance.
	 */
	Message* derive(const unsigned char dstAddress);

	/**
	 * Get the optional circuit name.
	 * @return the optional circuit name.
	 */
	string getCircuit() const { return m_circuit; }

	/**
	 * Get the message name (unique within the same circuit and type).
	 * @return the message name (unique within the same circuit and type).
	 */
	string getName() const { return m_name; }

	/**
	 * Get whether this is a write message.
	 * @return whether this is a write message.
	 */
	bool isWrite() const { return m_isWrite; }

	/**
	 * Get whether message can be initiated only by a participant other than us.
	 * @return true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant.
	 */
	bool isPassive() const { return m_isPassive; }

	/**
	 * Get the comment.
	 * @return the comment.
	 */
	string getComment() const { return m_comment; }

	/**
	 * Get the source address.
	 * @return the source address, or @a SYN for any.
	 */
	unsigned char getSrcAddress() const { return m_srcAddress; }

	/**
	 * Get the destination address.
	 * @return the destination address, or @a SYN for any.
	 */
	unsigned char getDstAddress() const { return m_dstAddress; }

	/**
	 * Get the command ID bytes.
	 * @return the primary, secondary, and optionally further command ID bytes.
	 */
	vector<unsigned char> getId() const { return m_id; }

	/**
	 * Return the key for storing in @a MessageMap.
	 * @return the key for storing in @a MessageMap.
	 */
	unsigned long long getKey() { return m_key; }

	/**
	 * Get the polling priority, or 0 for no polling at all.
	 * @return the polling priority, or 0 for no polling at all.
	 */
	unsigned char getPollPriority() const { return m_pollPriority; }

	/**
	 * Set the polling priority.
	 * @param priority the polling priority, or 0 for no polling at all.
	 * @return true when the priority was changed and polling was not enabled before, false otherwise.
	 */
	bool setPollPriority(unsigned char priority);

	/**
	 * Set the poll priority suitable for resolving a @a Condition.
	 */
	void setUsedByCondition();

	/**
	 * Return whether this @a Message depends on a @a Condition.
	 * @return true when this @a Message depends on a @a Condition.
	 */
	bool isConditional() const { return m_condition!=NULL; }

	/**
	 * Return whether this @a Message is available (optionally depending on a @a Condition evaluation).
	 * @return true when this @a Message is available.
	 */
	bool isAvailable();

	/**
	 * Return whether the field is available.
	 * @param fieldName the name of the field to find, or NULL for any.
	 * @param numeric true for a numeric field, false for a string field.
	 * @return true if the field is available.
	 */
	bool hasField(const char* fieldName, bool numeric=true);

	/**
	 * Prepare the master @a SymbolString for sending a query or command to the bus.
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
	 * Prepare the slave @a SymbolString for sending an answer to the bus.
	 * @param slaveData the slave data @a SymbolString for writing symbols to.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t prepareSlave(SymbolString& slaveData);

	/**
	 * Decode a singular part of a received message.
	 * @param partType the @a PartType of the data.
	 * @param data the unescaped data @a SymbolString for reading binary data.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @param leadingSeparator whether to prepend a separator before the formatted value.
	 * @param fieldName the optional name of a field to limit the output to.
	 * @param fieldIndex the optional index of the named field to limit the output to, or -1.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t decode(const PartType partType, SymbolString& data,
			ostringstream& output, OutputFormat outputFormat=0,
			bool leadingSeparator=false, const char* fieldName=NULL, signed char fieldIndex=-1);

	/**
	 * Decode all parts of a received message.
	 * @param masterData the unescaped master data @a SymbolString to decode.
	 * @param slaveData the unescaped slave data @a SymbolString to decode.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @param leadingSeparator whether to prepend a separator before the formatted value.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t decode(SymbolString& masterData, SymbolString& slaveData,
			ostringstream& output, OutputFormat outputFormat=0,
			bool leadingSeparator=false);

	/**
	 * Decode the value from the last stored data.
	 * @param output the @a ostringstream to append the formatted value to.
	 * @param outputFormat the @a OutputFormat options to use.
	 * @param leadingSeparator whether to prepend a separator before the formatted value.
	 * @param fieldName the optional name of a field to limit the output to.
	 * @param fieldIndex the optional index of the named field to limit the output to, or -1.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t decodeLastData(ostringstream& output, OutputFormat outputFormat=0,
			bool leadingSeparator=false, const char* fieldName=NULL, signed char fieldIndex=-1);

	/**
	 * Decode a particular field value from the last stored data.
	 * @param output the variable in which to store the value.
	 * @param fieldName the name of the field to decode, or NULL for the first field.
	 * @param fieldIndex the optional index of the named field, or -1.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t decodeLastDataField(unsigned int& output, const char* fieldName, signed char fieldIndex=-1);

	/**
	 * Get the last seen master data.
	 * @return the last seen master @a SymbolString.
	 */
	SymbolString& getLastMasterData() { return m_lastMasterData; }

	/**
	 * Get the last seen slave data.
	 * @return the last seen slave @a SymbolString.
	 */
	SymbolString& getLastSlaveData() { return m_lastSlaveData; }

	/**
	 * Get the time when @a m_lastValue was last stored.
	 * @return the time when @a m_lastValue was last stored, or 0 if this message was not decoded yet.
	 */
	time_t getLastUpdateTime() { return m_lastUpdateTime; }

	/**
	 * Get the time when @a m_lastValue was last changed.
	 * @return the time when @a m_lastValue was last changed, or 0 if this message was not decoded yet.
	 */
	time_t getLastChangeTime() { return m_lastChangeTime; }

	/**
	 * Get the time when this message was last polled for.
	 * @return the time when this message was last polled for, or 0 for never.
	 */
	time_t getLastPollTime() { return m_lastPollTime; }

	/**
	 * Return whether this @a Message needs to be polled after the other one.
	 * @param other the other @a Message to compare with.
	 * @return true if this @a Message needs to be polled after the other one.
	 */
	bool isLessPollWeight(const Message* other);

	/**
	 * Write the message definition to the @a ostream.
	 * @param output the @a ostream to append the formatted value to.
	 */
	void dump(ostream& output);

	/**
	 * Write parts of the message definition to the @a ostream.
	 * @param output the @a ostream to append the formatted value to.
	 * @param columns the list of column indexes to write.
	 */
	void dump(ostream& output, vector<size_t>& columns);

private:

	/** the optional circuit name. */
	const string m_circuit;

	/** the message name (unique within the same circuit and type). */
	const string m_name;

	/** whether this is a write message. */
	const bool m_isWrite;

	/** true if message can only be initiated by a participant other than us,
	 * false if message can be initiated by any participant. */
	const bool m_isPassive;

	/** the comment. */
	const string m_comment;

	/** the source address, or @a SYN for any (only relevant if passive). */
	const unsigned char m_srcAddress;

	/** the destination address, or @a SYN for any (only for temporary scan messages). */
	const unsigned char m_dstAddress;

	/** the primary, secondary, and optionally further command ID bytes. */
	vector<unsigned char> m_id;

	/** the key for storing in @a MessageMap. */
	unsigned long long m_key;

	/** the @a DataField for encoding/decoding the message. */
	DataField* m_data;

	/** whether to delete the @a DataField during destruction. */
	const bool m_deleteData;

	/** the priority for polling, or 0 for no polling at all. */
	unsigned char m_pollPriority;

	/** whether this message is used by a @a Condition. */
	bool m_usedByCondition;

	/** the @a Condition for this message, or NULL. */
	Condition* m_condition;

	/** the last seen master data. */
	SymbolString m_lastMasterData;

	/** the last seen slave data. */
	SymbolString m_lastSlaveData;

	/** the system time when the message was last updated, 0 for never. */
	time_t m_lastUpdateTime;

	/** the system time when the message content was last changed, 0 for never. */
	time_t m_lastChangeTime;

	/** the number of times this messages was already polled for. */
	unsigned int m_pollCount;

	/** the system time when this message was last polled for, 0 for never. */
	time_t m_lastPollTime;

};


/**
 * A function that compares the weighted poll priority of two @a Message instances.
 */
struct compareMessagePriority : binary_function <Message*,Message*,bool> {
	/**
	 * Compare the weighted poll priority of the two @a Message instances.
	 * @param x the first @a Message.
	 * @param y the second @a Message.
	 * @return whether @a x is smaller than @a y with regard to their weighted poll priority.
	 */
	bool operator() (Message* x, Message* y) const { return x->isLessPollWeight(y); };
};


/**
 * Helper class extending @a priority_queue to hold distinct values only.
 */
class MessagePriorityQueue
	: public priority_queue<Message*, vector<Message*>, compareMessagePriority>
{
public:
	/**
	 * Add data to the queue and ensure it is contained only once.
	 * @param __x the element to add.
	 */
	void push(const value_type& __x)
	{
		for (vector<Message*>::iterator it = c.begin(); it != c.end(); it++) {
			if (*it==__x) {
				c.erase(it);
				break;
			}
		}
		priority_queue<Message*, vector<Message*>, compareMessagePriority>::push(__x);
	}
};


/**
 * An abstract condition based on the value of one or more @a Message instances.
 */
class Condition
{
public:

	/**
	 * Construct a new instance.
	 */
	Condition()
		: m_lastCheckTime(0), m_isTrue(false) { }

	/**
	 * Destructor.
	 */
	virtual ~Condition() { }

	/**
	 * Factory method for creating a new instance.
	 * @param it the iterator to traverse for the definition parts.
	 * @param end the iterator pointing to the end of the definition parts.
	 * @param returnValue the variable in which to store the created instance.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	static result_t create(vector<string>::iterator& it, const vector<string>::iterator end, SimpleCondition*& returnValue);

	/**
	 * Combine this condition with another instance using a logical and.
	 * @param other the @a Condition to combine with.
	 */
	virtual CombinedCondition* combineAnd(Condition* other) = 0;

	/**
	 * Resolve the referred @a Message instance(s) and field index(es).
	 * @param messages the @a MessageMap instance for resolving.
	 * @param errorMessage a @a ostringstream to which to add optional error messages.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t resolve(MessageMap* messages, ostringstream& errorMessage) = 0;

	/**
	 * Check and return whether this condition is fulfilled.
	 * @return whether this condition is fulfilled.
	 */
	virtual bool isTrue() = 0;

protected:

	/** the system time when the condition was last checked, 0 for never. */
	time_t m_lastCheckTime;

	/** whether the condition was @a true during the last check. */
	bool m_isTrue;

};


/**
 * A simple condition based on the value of one @a Message.
 */
class SimpleCondition : public Condition
{
public:

	/**
	 * Construct a new instance.
	 * @param circuit the circuit name.
	 * @param name the message name.
	 * @param dstAddress the override destination address, or @a SYN (only for @a Message without specific destination).
	 * @param field the field name.
	 * @param valueRanges the valid value ranges (pairs of from/to inclusive), empty for @a m_message seen check.
	 */
	SimpleCondition(const string circuit, const string name, const unsigned char dstAddress, const string field, const vector<unsigned int> valueRanges)
		: Condition(),
		  m_circuit(circuit), m_name(name), m_dstAddress(dstAddress), m_field(field), m_valueRanges(valueRanges),
		  m_message(NULL) { }

	/**
	 * Destructor.
	 */
	virtual ~SimpleCondition() {}

	// @copydoc
	virtual CombinedCondition* combineAnd(Condition* other);

	/**
	 * Resolve the referred @a Message instance(s) and field index(es).
	 * @param messages the @a MessageMap instance for resolving.
	 * @param errorMessage a @a ostringstream to which to add optional error messages.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t resolve(MessageMap* messages, ostringstream& errorMessage);

	/**
	 * Check and return whether this condition is fulfilled.
	 * @return whether this condition is fulfilled.
	 */
	virtual bool isTrue();

private:

	/** the circuit name. */
	const string m_circuit;

	/** the message name. */
	const string m_name;

	/** the override destination address, or @a SYN (only for @a Message without specific destination). */
	const unsigned char m_dstAddress;

	/** the field name, or empty for first field. */
	const string m_field;

	/** the valid value ranges (pairs of from/to inclusive), empty for @a m_message seen check. */
	const vector<unsigned int> m_valueRanges;

	/** the resolved @a Message instance, or NULL. */
	Message* m_message;

};


/**
 * A condition combining two or more @a SimpleCondition instances with a logical and.
 */
class CombinedCondition : public Condition
{
public:

	/**
	 * Construct a new instance.
	 */
	CombinedCondition()
		: Condition() { }

	/**
	 * Destructor.
	 */
	virtual ~CombinedCondition() {}

	// @copydoc
	virtual CombinedCondition* combineAnd(Condition* other) { m_conditions.push_back(other); return this; }

	// @copydoc
	virtual result_t resolve(MessageMap* messages, ostringstream& errorMessage);

	// @copydoc
	virtual bool isTrue();

private:

	/** the @a Condition instances used. */
	vector<Condition*> m_conditions;

};


/**
 * Holds a map of all known @a Message instances.
 */
class MessageMap : public FileReader<DataFieldTemplates*>
{
public:

	/**
	 * Construct a new instance.
	 */
	MessageMap() : FileReader<DataFieldTemplates*>::FileReader(true),
		m_minIdLength(4), m_maxIdLength(0), m_messageCount(0), m_conditionalMessageCount(0), m_passiveMessageCount(0) {}

	/**
	 * Destructor.
	 */
	virtual ~MessageMap() { clear(); }

	/**
	 * Add a @a Message instance to this set.
	 * @param message the @a Message instance to add.
	 * @return @a RESULT_OK on success, or an error code.
	 * Note: the caller may not free the added instance on success.
	 */
	result_t add(Message* message);

	// @copydoc
	virtual result_t addDefaultFromFile(vector< vector<string> >& defaults, vector<string>& row,
		vector<string>::iterator& begin, string defaultDest, string defaultCircuit,
		const string& filename, unsigned int lineNo);

	/**
	 * Read the @a Condition instance(s) from the types field.
	 * @param types the field from which to read the @a Condition instance(s).
	 * @param filename the name of the file being read.
	 * @param returnValue the variable in which to store the read instance.
	 */
	result_t readConditions(string& types, const string& filename, Condition*& condition);

	// @copydoc
	virtual result_t addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
		DataFieldTemplates* arg, vector< vector<string> >* defaults,
		const string& filename, unsigned int lineNo);

	/**
	 * Resolve all @a Condition instances.
	 * @param verbose whether to verbosely add all problems to the error message.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	result_t resolveConditions(bool verbose=false);

	/**
	 * Find the @a Message instance for the specified circuit and name.
	 * @param circuit the optional circuit name.
	 * @param name the message name.
	 * @param isWrite whether this is a write message.
	 * @param isPassive whether this is a passive message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(const string& circuit, const string& name, const bool isWrite, const bool isPassive=false);

	/**
	 * Find all active get @a Message instances for the specified circuit and name.
	 * @param circuit the circuit name, or empty for any.
	 * @param name the message name, or empty for any.
	 * @param pb the primary ID byte, or -1 for any (default any).
	 * @param completeMatch false to also include messages where the circuit and name matches only a part of the given circuit and name (default true).
	 * @param withRead true to include read messages (default true).
	 * @param withWrite true to include write messages (default false).
	 * @param withPassive true to include passive messages (default false).
	 * @return the found @a Message instances.
	 * Note: the caller may not free the returned instances.
	 */
	deque<Message*> findAll(const string& circuit, const string& name, const short pb=-1, const bool completeMatch=true,
		const bool withRead=true, const bool withWrite=false, const bool withPassive=false);

	/**
	 * Find the @a Message instance for the specified master data.
	 * @param master the master @a SymbolString for identifying the @a Message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* find(SymbolString& master);

	/**
	 * Find all @a Message instances for the specified master data.
	 * @param master the master @a SymbolString for identifying the @a Message.
	 * @return the @a Message instance, or NULL.
	 * Note: the caller may not free the returned instances.
	 */
	deque<Message*> findAll(SymbolString& master);

	/**
	 * Invalidate cached data of the @a Message and all other instances with a matching name key.
	 * @param message the @a Message to invalidate.
	 */
	void invalidateCache(Message* message);

	/**
	 * Add a @a Message to the list of instances to poll.
	 * @param message the @a Message to poll.
	 * @param toFront whether to add the @a Message to the very front of the poll queue.
	 */
	void addPollMessage(Message* message, bool toFront=false);

	/**
	 * Removes all @a Message instances.
	 */
	void clear();

	/**
	 * Get the number of all stored @a Message instances.
	 * @return the the number of all stored @a Message instances.
	 */
	size_t size() { return m_messageCount; }

	/**
	 * Get the number of stored conditional @a Message instances.
	 * @return the the number of stored conditional @a Message instances.
	 */
	size_t sizeConditional() { return m_conditionalMessageCount; }

	/**
	 * Get the number of stored passive @a Message instances.
	 * @return the the number of stored passive @a Message instances.
	 */
	size_t sizePassive() { return m_passiveMessageCount; }

	/**
	 * Get the number of stored @a Message instances with a poll priority.
	 * @return the the number of stored @a Message instances with a poll priority.
	 */
	size_t sizePoll() { return m_pollMessages.size(); }

	/**
	 * Get the next @a Message to poll.
	 * @return the next @a Message to poll, or NULL.
	 * Note: the caller may not free the returned instance.
	 */
	Message* getNextPoll();

	/**
	 * Get the number of stored @a Condition instances.
	 * @return the number of stored @a Condition instances.
	 */
	size_t sizeConditions() { return m_conditions.size(); }

	/**
	 * Get the stored @a Condition instances.
	 * @return the @a Condition instances by filename and condition name.
	 */
	map<string, Condition*>& getConditions() { return m_conditions; }

	/**
	 * Write the message definitions to the @a ostream.
	 * @param output the @a ostream to append the formatted messages to.
	 */
	void dump(ostream& output);

private:

	/** the minimum ID length used by any of the known @a Message instances. */
	unsigned char m_minIdLength;

	/** the maximum ID length used by any of the known @a Message instances. */
	unsigned char m_maxIdLength;

	/** the number of distinct @a Message instances stored in @a m_messagesByName. */
	size_t m_messageCount;

	/** the number of conditional @a Message instances part of @a m_messageCount. */
	size_t m_conditionalMessageCount;

	/** the number of distinct passive @a Message instances stored in @a m_messagesByKey. */
	size_t m_passiveMessageCount;

	/** the known @a Message instances by lowercase circuit and name. */
	map<string, vector<Message*> > m_messagesByName;

	/** the known @a Message instances by key. */
	map<unsigned long long, vector<Message*> > m_messagesByKey;

	/** the known @a Message instances to poll, by priority. */
	MessagePriorityQueue m_pollMessages;

	/** the @a Condition instances by filename and condition name. */
	map<string, Condition*> m_conditions;

};

#endif // LIBEBUS_MESSAGE_H_

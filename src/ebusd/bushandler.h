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

#ifndef BUSHANDLER_H_
#define BUSHANDLER_H_

#include "message.h"
#include "data.h"
#include "symbol.h"
#include "result.h"
#include "port.h"
#include "wqueue.h"
#include "thread.h"
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <typeinfo>

using namespace std;

/** the maximum allowed time [us] for retrieving a symbol from an addressed slave. */
//#define SLAVE_RECV_TIMEOUT 10000
/** the maximum allowed time [us] for retrieving the AUTO-SYN symbol (45ms + 2*1,2% + 1 Symbol). */
#define SYN_TIMEOUT 50800
/** the maximum duration [us] of a single symbol (Start+8Bit+Stop+Extra @ 2400Bd-2*1,2%). */
#define SYMBOL_DURATION 4700
/** the maximum allowed time [us] for retrieving back a sent symbol (2x symbol duration). */
#define SEND_TIMEOUT (2*SYMBOL_DURATION)

/** the possible bus states. */
enum BusState {
	bs_skip,        // skip all symbols until next @a SYN
	bs_ready,       // ready for next master (after @a SYN symbol, send/receive QQ)
	bs_recvCmd,     // receive command (ZZ, PBSB, master data) [passive set]
	bs_recvCmdAck,  // receive command ACK/NACK [passive set + active set+get]
	bs_recvRes,     // receive response (slave data) [passive set + active get]
	bs_recvResAck,  // receive response ACK/NACK [passive set]
	bs_sendCmd,     // send command (ZZ, PBSB, master data) [active set+get]
	bs_sendResAck,  // send response ACK/NACK [active get]
	bs_sendCmdAck,  // send command ACK/NACK [passive get]
	bs_sendRes,     // send response (slave data) [passive get]
	bs_sendSyn,     // send SYN for completed transfer [active set+get]
};

class BusHandler;

/**
 * @brief Generic request for sending to and receiving from the bus.
 */
class BusRequest
{
	friend class BusHandler;
public:

	/**
	 * @brief Constructor.
	 * @param master the master data @a SymbolString to send.
	 * @param slave the slave data @a SymbolString received.
	 * @param deleteOnFinish whether to automatically delete this @a BusRequest when finished.
	 */
	BusRequest(SymbolString& master, SymbolString& slave, bool deleteOnFinish)
		: m_master(master), m_slave(slave), m_busLostRetries(0),
		  m_deleteOnFinish(deleteOnFinish) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~BusRequest() {}

	/**
	 * @brief Notify the request of the specified result.
	 * @param result the result of the request.
	 */
	virtual void notify(result_t result) = 0;

protected:

	/** the master data @a SymbolString to send. */
	SymbolString& m_master;

	/** the slave data @a SymbolString received. */
	SymbolString& m_slave;

	/** the number of times a send is repeated due to lost arbitration. */
	unsigned int m_busLostRetries;

	/** whether to automatically delete this @a BusRequest when finished. */
	bool m_deleteOnFinish;

};


/**
 * @brief A poll @a BusRequest handled by @a BusHandler itself.
 */
class PollRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * @brief Constructor.
	 * @param slave the slave data @a SymbolString received.
	 * @param message the associated @a Message.
	 */
	PollRequest(SymbolString& slave, Message* message)
		: BusRequest(m_master, slave, true), m_message(message) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~PollRequest() {}

	/**
	 * @brief Prepare the master data.
	 * @param masterAddress the master bus address to use.
	 * @return the result code.
	 */
	result_t prepare(unsigned char masterAddress);

	// @copydoc
	virtual void notify(result_t result);

private:

	/** the master data @a SymbolString. */
	SymbolString m_master;

	/** the associated @a Message. */
	Message* m_message;

};


/**
 * @brief A scan @a BusRequest handled by @a BusHandler itself.
 */
class ScanRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * @brief Constructor.
	 * @param slave the slave data @a SymbolString received.
	 * @param message the associated @a Message.
	 */
	ScanRequest(SymbolString& slave, Message* message)
		: BusRequest(m_master, slave, true), m_message(message) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~ScanRequest() {}

	/**
	 * @brief Prepare the master data.
	 * @param masterAddress the master bus address to use.
	 * @param dstAddress the destination address to set.
	 * @return the result code.
	 */
	result_t prepare(unsigned char masterAddress, unsigned char dstAddress);

	// @copydoc
	virtual void notify(result_t result);

private:

	/** the master data @a SymbolString. */
	SymbolString m_master;

	/** the associated @a Message. */
	Message* m_message;

	/** the formatted scan result. */
	ostringstream m_scanResult;

};


/**
 * @brief An active @a BusRequest that can be waited for.
 */
class ActiveBusRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * @brief Constructor.
	 * @param master the master data @a SymbolString to send.
	 * @param slave the slave data @a SymbolString received.
	 */
	ActiveBusRequest(SymbolString& master, SymbolString& slave);

	/**
	 * @brief Destructor.
	 */
	virtual ~ActiveBusRequest();

	/**
	 * @brief Wait for notification.
	 * @param timeout the maximum time to wait in seconds.
	 * @return the result code.
	 */
	bool wait(int timeout);

	// @copydoc
	virtual void notify(result_t result);

private:

	/** true once the request is finished. */
	bool m_finished;

	/** the result of handling the request. */
	result_t m_result;

	/** a mutex for wait/notify. */
	pthread_mutex_t m_mutex;

	/** a mutex condition for wait/notify. */
	pthread_cond_t m_cond;

};


/**
 * @brief Handles input from and output to the bus with respect to the ebus protocol.
 */
class BusHandler : public Thread
{
public:

	/**
	 * @brief Construct a new instance.
	 * @param port the @a Port instance for accessing the bus.
	 * @param messages the @a MessageMap instance with all known @a Message instances.
	 * @param ownAddress the own master address.
	 * @param answer whether to answer queries for the own master/slave address.
	 * @param busLostRetries the number of times a send is repeated due to lost arbitration.
	 * @param failedSendRetries the number of times a failed send is repeated (other than lost arbitration).
	 * @param slaveRecvTimeout the maximum time in microseconds an addressed slave is expected to acknowledge.
	 * @param busAcquireTimeout the maximum time in microseconds for bus acquisition.
	 * @param lockCount the number of AUTO-SYN symbols before sending is allowed after lost arbitration.
	 * @param pollInterval the interval in seconds in which poll messages are cycled, or 0 if disabled.
	 */
	BusHandler(Port* port, MessageMap* messages,
			const unsigned char ownAddress, const bool answer,
			const unsigned int busLostRetries, const unsigned int failedSendRetries,
			const unsigned int busAcquireTimeout, const unsigned int slaveRecvTimeout,
			const unsigned int lockCount, const unsigned int pollInterval)
		: m_port(port), m_messages(messages),
		  m_ownMasterAddress(ownAddress), m_ownSlaveAddress((ownAddress+5)&0xff), m_answer(answer),
		  m_busLostRetries(busLostRetries), m_failedSendRetries(failedSendRetries),
		  m_busAcquireTimeout(busAcquireTimeout), m_slaveRecvTimeout(slaveRecvTimeout),
		  m_lockCount(lockCount), m_remainLockCount(lockCount),
		  m_pollInterval(pollInterval), m_lastPoll(0),
		  m_request(NULL), m_nextSendPos(0),
		  m_state(bs_skip), m_repeat(false),
		  m_commandCrcValid(false), m_responseCrcValid(false),
		  m_scanMessage(NULL) {
		memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
	}

	/**
	 * @brief Destructor.
	 */
	virtual ~BusHandler() {
		if (m_scanMessage != NULL)
			delete m_scanMessage;
	}

	/**
	 * @brief Send a message on the bus and wait for the answer.
	 * @param master the @a SymbolString with the master data to send.
	 * @param slave the @a SymbolString that will be filled with retrieved slave data.
	 */
	result_t sendAndWait(SymbolString& master, SymbolString& slave);

	/**
	 * @brief Main thread entry.
	 */
	virtual void run();

	/**
	 * @brief Get the last received data for the @a Message.
	 * @param message the @a Message instance.
	 * @return the last received data for the @a Message, or the empty string if not available.
	 */
	string getReceivedData(Message* message);

	/**
	 * @brief Initiate a scan of the slave addresses.
	 * @param full true for a full scan (all slaves), false for scanning only already seen slaves.
	 */
	result_t startScan(bool full=false);

	/**
	 * @brief Format the scan result to the @a ostringstream.
	 * @param output the @a ostringstream to format the scan result to.
	 */
	void formatScanResult(ostringstream& output);

private:

	/**
	 * @brief Handle the next symbol on the bus.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t handleSymbol();

	/**
	 * @brief Set a new @a BusState and add a log message if necessary.
	 * @param state the new @a BusState.
	 * @param result the result code.
	 * @param firstRepetition true if the first repetition of a message part is being started.
	 * @return the result code.
	 */
	result_t setState(BusState state, result_t result, bool firstRepetition=false);

	/**
	 * @brief Called when a passive reception was successfully completed.
	 */
	void receiveCompleted();

	/** the @a Port instance for accessing the bus. */
	Port* m_port;

	/** the @a MessageMap instance with all known @a Message instances. */
	MessageMap* m_messages;

	/** the own master address. */
	const unsigned char m_ownMasterAddress;

	/** the own slave address. */
	const unsigned char m_ownSlaveAddress;

	/** whether to answer queries for the own master/slave address. */
	const bool m_answer;

	/** the number of times a send is repeated due to lost arbitration. */
	const unsigned int m_busLostRetries;

	/** the number of times a failed send is repeated (other than lost arbitration). */
	const unsigned int m_failedSendRetries;

	/** the maximum time in microseconds for bus acquisition. */
	const unsigned int m_busAcquireTimeout;

	/** the maximum time in microseconds an addressed slave is expected to acknowledge. */
	const unsigned int m_slaveRecvTimeout;

	/** the number of AUTO-SYN symbols before sending is allowed after lost arbitration. */
	const unsigned int m_lockCount;

	/** the remaining number of AUTO-SYN symbols before sending is allowed again. */
	unsigned int m_remainLockCount;

	/** the interval in seconds in which poll messages are cycled, or 0 if disabled. */
	const unsigned int m_pollInterval;

	/** the time of the last poll, or 0 for never. */
	time_t m_lastPoll;

	/** the queue of @a BusRequests that shall be handled. */
	WQueue<BusRequest*> m_requests;

	/** the currently handled BusRequest, or NULL. */
	BusRequest* m_request;

	/** the offset of the next symbol that needs to be sent from the command or response,
	 * (only relevant if m_request is set and state is bs_command or bs_response). */
	unsigned char m_nextSendPos;

	/** the current @a BusState. */
	BusState m_state;

	/** whether the current message part is being repeated. */
	bool m_repeat;

	/** the received/sent command. */
	SymbolString m_command;

	/** whether the command CRC is valid. */
	bool m_commandCrcValid;

	/** the received/sent response. */
	SymbolString m_response;

	/** whether the response CRC is valid. */
	bool m_responseCrcValid;

	/** the participating bus addresses seen so far. */
	bool m_seenAddresses[256];

	/** the @a Message instance used for scanning. */
	Message* m_scanMessage;

	/** the scan results by slave address. */
	map<unsigned char, string> m_scanResults;

};


#endif // BUSHANDLER_H_

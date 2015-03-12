/*
 * Copyright (C) John Baier 2014-2015 <ebusd@johnm.de>
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
#include "device.h"
#include "wqueue.h"
#include "thread.h"
#include <string>
#include <vector>
#include <map>
#include <pthread.h>

/** \file bushandler.h */

using namespace std;

/** the default time [us] for retrieving a symbol from an addressed slave. */
#define SLAVE_RECV_TIMEOUT 15000

/** the maximum allowed time [us] for retrieving the AUTO-SYN symbol (45ms + 2*1,2% + 1 Symbol). */
#define SYN_TIMEOUT 50800

/** the time [us] for determining bus signal availability (AUTO-SYN timeout * 5). */
#define SIGNAL_TIMEOUT 250000

/** the maximum duration [us] of a single symbol (Start+8Bit+Stop+Extra @ 2400Bd-2*1,2%). */
#define SYMBOL_DURATION 4700

/** the maximum allowed time [us] for retrieving back a sent symbol (2x symbol duration). */
#define SEND_TIMEOUT (2*SYMBOL_DURATION)

/** the possible bus states. */
enum BusState {
	bs_noSignal,	//!< no signal on the bus
	bs_skip,        //!< skip all symbols until next @a SYN
	bs_ready,       //!< ready for next master (after @a SYN symbol, send/receive QQ)
	bs_recvCmd,     //!< receive command (ZZ, PBSB, master data) [passive set]
	bs_recvCmdAck,  //!< receive command ACK/NACK [passive set + active set+get]
	bs_recvRes,     //!< receive response (slave data) [passive set + active get]
	bs_recvResAck,  //!< receive response ACK/NACK [passive set]
	bs_sendCmd,     //!< send command (ZZ, PBSB, master data) [active set+get]
	bs_sendResAck,  //!< send response ACK/NACK [active get]
	bs_sendCmdAck,  //!< send command ACK/NACK [passive get]
	bs_sendRes,     //!< send response (slave data) [passive get]
	bs_sendSyn,     //!< send SYN for completed transfer [active set+get]
};

class BusHandler;

/**
 * Generic request for sending to and receiving from the bus.
 */
class BusRequest
{
	friend class BusHandler;
public:

	/**
	 * Constructor.
	 * @param master the escaped master data @a SymbolString to send.
	 * @param deleteOnFinish whether to automatically delete this @a BusRequest when finished.
	 */
	BusRequest(SymbolString& master, const bool deleteOnFinish)
		: m_master(master), m_busLostRetries(0),
		  m_deleteOnFinish(deleteOnFinish) {}

	/**
	 * Destructor.
	 */
	virtual ~BusRequest() {}

	/**
	 * Notify the request of the specified result.
	 * @param result the result of the request.
	 * @param slave the slave data @a SymbolString received.
	 * @return true if the request needs to be restarted.
	 */
	virtual bool notify(result_t result, SymbolString& slave) = 0;

protected:

	/** the escaped master data @a SymbolString to send. */
	SymbolString& m_master;

	/** the number of times a send is repeated due to lost arbitration. */
	unsigned int m_busLostRetries;

	/** whether to automatically delete this @a BusRequest when finished. */
	const bool m_deleteOnFinish;

};


/**
 * A poll @a BusRequest handled by @a BusHandler itself.
 */
class PollRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * Constructor.
	 * @param message the associated @a Message.
	 */
	PollRequest(Message* message)
		: BusRequest(m_master, true), m_message(message) {}

	/**
	 * Destructor.
	 */
	virtual ~PollRequest() {}

	/**
	 * Prepare the master data.
	 * @param masterAddress the master bus address to use.
	 * @return the result code.
	 */
	result_t prepare(unsigned char masterAddress);

	// @copydoc
	virtual bool notify(result_t result, SymbolString& slave);

private:

	/** the escaped master data @a SymbolString. */
	SymbolString m_master;

	/** the associated @a Message. */
	Message* m_message;

};


/**
 * A scan @a BusRequest handled by @a BusHandler itself.
 */
class ScanRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * Constructor.
	 * @param message the primary query @a Message.
	 * @param messages the optional secondary query @a Message instances (to be queried only when the primary was successful).
	 * @param scanResults the map in which to store the formatted scan result by slave address.
	 */
	ScanRequest(Message* message, deque<Message*> messages,
		map<unsigned char, string>* scanResults)
		: BusRequest(m_master, true), m_message(message), m_messages(messages),
		  m_scanResults(scanResults) {}

	/**
	 * Destructor.
	 */
	virtual ~ScanRequest() {}

	/**
	 * Prepare the master data.
	 * @param masterAddress the master bus address to use.
	 * @param dstAddress the destination address to set.
	 * @return the result code.
	 */
	result_t prepare(unsigned char masterAddress, unsigned char dstAddress);

	// @copydoc
	virtual bool notify(result_t result, SymbolString& slave);

private:

	/** the escaped master data @a SymbolString. */
	SymbolString m_master;

	/** the currently queried @a Message. */
	Message* m_message;

	/** the remaining secondary @a Message instances. */
	deque<Message*> m_messages;

	/** the map in which to store the formatted scan result by slave address. */
	map<unsigned char, string>* m_scanResults;

};


/**
 * An active @a BusRequest that can be waited for.
 */
class ActiveBusRequest : public BusRequest
{
	friend class BusHandler;
public:

	/**
	 * Constructor.
	 * @param master the escaped master data @a SymbolString to send.
	 * @param slave reference to @a SymbolString for filling in the received slave data.
	 */
	ActiveBusRequest(SymbolString& master, SymbolString& slave)
		: BusRequest(master, false), m_result(RESULT_ERR_NO_SIGNAL), m_slave(slave) {}

	/**
	 * Destructor.
	 */
	virtual ~ActiveBusRequest() {}

	// @copydoc
	virtual bool notify(result_t result, SymbolString& slave);

private:

	/** the result of handling the request. */
	result_t m_result;

	/** reference to @a SymbolString for filling in the received slave data. */
	SymbolString& m_slave;

};


/**
 * Handles input from and output to the bus with respect to the eBUS protocol.
 */
class BusHandler : public WaitThread
{
public:

	/**
	 * Construct a new instance.
	 * @param device the @a Device instance for accessing the bus.
	 * @param messages the @a MessageMap instance with all known @a Message instances.
	 * @param ownAddress the own master address.
	 * @param answer whether to answer queries for the own master/slave address.
	 * @param busLostRetries the number of times a send is repeated due to lost arbitration.
	 * @param failedSendRetries the number of times a failed send is repeated (other than lost arbitration).
	 * @param slaveRecvTimeout the maximum time in microseconds an addressed slave is expected to acknowledge.
	 * @param busAcquireTimeout the maximum time in microseconds for bus acquisition.
	 * @param lockCount the number of AUTO-SYN symbols before sending is allowed after lost arbitration, or 0 for auto detection.
	 * @param pollInterval the interval in seconds in which poll messages are cycled, or 0 if disabled.
	 */
	BusHandler(Device* device, MessageMap* messages,
			const unsigned char ownAddress, const bool answer,
			const unsigned int busLostRetries, const unsigned int failedSendRetries,
			const unsigned int busAcquireTimeout, const unsigned int slaveRecvTimeout,
			const unsigned int lockCount, const unsigned int pollInterval)
		: m_device(device), m_messages(messages),
		  m_ownMasterAddress(ownAddress), m_ownSlaveAddress((unsigned char)(ownAddress+5)), m_answer(answer),
		  m_busLostRetries(busLostRetries), m_failedSendRetries(failedSendRetries),
		  m_busAcquireTimeout(busAcquireTimeout), m_slaveRecvTimeout(slaveRecvTimeout),
		  m_masterCount(1), m_autoLockCount(lockCount==0), m_lockCount(lockCount<=3 ? 3 : lockCount), m_remainLockCount(m_autoLockCount),
		  m_pollInterval(pollInterval), m_lastReceive(0), m_lastPoll(0),
		  m_currentRequest(NULL), m_nextSendPos(0),
		  m_symPerSec(0), m_maxSymPerSec(0),
		  m_state(bs_noSignal), m_repeat(false),
		  m_command(false), m_commandCrcValid(false), m_response(false), m_responseCrcValid(false),
		  m_scanMessage(NULL) {
		memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
	}

	/**
	 * Destructor.
	 */
	virtual ~BusHandler() {
		stop();
		if (m_scanMessage != NULL)
			delete m_scanMessage;
	}

	/**
	 * Send a message on the bus and wait for the answer.
	 * @param master the escaped @a SymbolString with the master data to send.
	 * @param slave the @a SymbolString that will be filled with retrieved slave data.
	 * @return the result code.
	 */
	result_t sendAndWait(SymbolString& master, SymbolString& slave);

	/**
	 * Main thread entry.
	 */
	virtual void run();

	/**
	 * Initiate a scan of the slave addresses.
	 * @param full true for a full scan (all slaves), false for scanning only already seen slaves.
	 * @return the result code.
	 */
	result_t startScan(bool full=false);

	/**
	 * Format the scan result to the @a ostringstream.
	 * @param output the @a ostringstream to format the scan result to.
	 */
	void formatScanResult(ostringstream& output);

	/**
	 * Return true when a signal on the bus is available.
	 * @return true when a signal on the bus is available.
	 */
	bool hasSignal() { return m_state != bs_noSignal; }

	/**
	 * Return the current symbol rate.
	 * @return the number of received symbols in the last second.
	 */
	unsigned int getSymbolRate() { return m_symPerSec; }

	/**
	 * Return the maximum seen symbol rate.
	 * @return the maximum number of received symbols per second ever seen.
	 */
	unsigned int getMaxSymbolRate() { return m_maxSymPerSec; }

	/**
	 * Return the number of masters already seen.
	 * @return the number of masters already seen (including ebusd itself).
	 */
	unsigned int getMasterCount() { return m_masterCount; }

private:

	/**
	 * Handle the next symbol on the bus.
	 * @return RESULT_OK on success, or an error code.
	 */
	result_t handleSymbol();

	/**
	 * Set a new @a BusState and add a log message if necessary.
	 * @param state the new @a BusState.
	 * @param result the result code.
	 * @param firstRepetition true if the first repetition of a message part is being started.
	 * @return the result code.
	 */
	result_t setState(BusState state, result_t result, bool firstRepetition=false);

	/**
	 * Called when a passive reception was successfully completed.
	 */
	void receiveCompleted();

	/** the @a Device instance for accessing the bus. */
	Device* m_device;

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

	/** the number of masters already seen. */
	unsigned int m_masterCount;

	/** whether m_lockCount shall be detected automatically. */
	const bool m_autoLockCount;

	/** the number of AUTO-SYN symbols before sending is allowed after lost arbitration. */
	unsigned int m_lockCount;

	/** the remaining number of AUTO-SYN symbols before sending is allowed again. */
	unsigned int m_remainLockCount;

	/** the interval in seconds in which poll messages are cycled, or 0 if disabled. */
	const unsigned int m_pollInterval;

	/** the time of the last received symbol, or 0 for never. */
	time_t m_lastReceive;

	/** the time of the last poll, or 0 for never. */
	time_t m_lastPoll;

	/** the queue of @a BusRequests that shall be handled. */
	WQueue<BusRequest*> m_nextRequests;

	/** the currently handled BusRequest, or NULL. */
	BusRequest* m_currentRequest;

	/** the queue of @a BusRequests that are already finished. */
	WQueue<BusRequest*> m_finishedRequests;

	/** the offset of the next symbol that needs to be sent from the command or response,
	 * (only relevant if m_request is set and state is @a bs_command or @a bs_response). */
	unsigned char m_nextSendPos;

	/** the number of received symbols in the last second. */
	unsigned int m_symPerSec;

	/** the maximum number of received symbols per second ever seen. */
	unsigned int m_maxSymPerSec;

	/** the current @a BusState. */
	BusState m_state;

	/** whether the current message part is being repeated. */
	bool m_repeat;

	/** the unescaped received command. */
	SymbolString m_command;

	/** whether the command CRC is valid. */
	bool m_commandCrcValid;

	/** the unescaped received response or escaped response to send. */
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

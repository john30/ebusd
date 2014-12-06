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

using namespace std;

/** the maximum allowed time [us] for retrieval of a single symbol from an addressed slave. */
#define SLAVE_RECV_TIMEOUT 10000
/** the maximum allowed time [us] for retrieval of an AUTO-SYN symbol (should be generated in <45ms). */
#define SYN_TIMEOUT 50000

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
//	bs_sendRes,     // send response (slave data) [passive get] // TODO implement
//	bs_sendCmdAck,  // send command ACK/NACK [passive get] // TODO implement
	bs_sendSyn,     // send SYN for completed transfer [active set+get]
};

/** the possible message transfer types. */
enum TransferType {
	tt_broadcast,    // broadcast transfer
	tt_masterMaster, // master to master transfer
	tt_masterSlave   // master to slave transfer
};

/** the possible combinations of participants in a single message exchange. */
enum MessageDirection {
	md_thisToAll,         // message from us to all (broadcast)
	md_thisToMaster,      // message from us to another master
	md_thisToSlave,       // message from us to another slave
	md_otherToAll,        // message from a master (other than us) to all (broadcast)
	md_otherToMaster,     // message from a master (other than us) to another master (other than us)
	md_otherToSlave,      // message from a master (other than us) to another slave (other than us)
	md_otherToThisMaster, // message from a master (other than us) to us (as master)
	md_otherToThisSlave,  // message from a master (other than us) to us (as slave)
	md_undefined,
};

class BusHandler;

/**
 * @brief Handles input from and output to the bus with respect to the ebus protocol.
 */
class BusRequest
{
	friend class BusHandler;
public:

	/**
	 * @brief Constructor.
	 * @param master the master data @a SymbolString to send.
	 * @param slave the slave data @a SymbolString received.
	 */
	BusRequest(SymbolString& master, SymbolString& slave);

	/**
	 * @brief Destructor.
	 */
	virtual ~BusRequest();

	/**
	 * @brief Wait for notification.
	 * @return the result code.
	 */
	bool wait(int timeout);

	/**
	 * @brief Notify all waiting threads.
	 */
	void notify(result_t result);

private:

	/** the master data @a SymbolString to send. */
	SymbolString& m_master;

	/** the slave data @a SymbolString received. */
	SymbolString& m_slave;

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
	 * @param ownMasterAddress the own master address to react on master-master messages, or @a SYN to ignore.
	 * @param ownSlaveAddress the own slave address to react on master-slave messages, or @a SYN to ignore.
	 */
	BusHandler(Port* port, MessageMap* messages, unsigned char ownMasterAddress,
			unsigned char ownSlaveAddress)
		: m_port(port), m_messages(messages), m_ownMasterAddress(ownMasterAddress),
		  m_ownSlaveAddress(ownSlaveAddress), m_request(NULL), m_nextSendPos(0),
		  m_state(bs_skip), m_repeat(false),
		  m_commandCrcValid(false), m_responseCrcValid(false) {}

	/**
	 * @brief Destructor.
	 */
	virtual ~BusHandler() {}

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
	 * @return the result code.
	 */
	result_t setState(BusState state, result_t result);

	/**
	 * @brief Called when a transfer was successfully completed.
	 * @param type the @a TransferType.
	 */
	void transferCompleted(TransferType type);

	/** the @a Port instance for accessing the bus. */
	Port* m_port;

	/** the @a MessageMap instance with all known @a Message instances. */
	MessageMap* m_messages;

	/** the own master address to react on master-master messages, or @a SYN to ignore. */
	unsigned char m_ownMasterAddress;

	/** the own slave address to react on master-slave messages, or @a SYN to ignore. */
	unsigned char m_ownSlaveAddress;

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

};


#endif // BUSHANDLER_H_

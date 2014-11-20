/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
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

#ifndef BUSLOOP_H_
#define BUSLOOP_H_

#include "commands.h"
#include "port.h"
#include "dump.h"
#include "wqueue.h"
#include "thread.h"
#include "symbol.h"
#include "result.h"

/** the maximum time [us] allowed for retrieving a byte from an addressed slave */
#define RECV_TIMEOUT 10000

enum MessageType { invalid, broadcast, masterMaster, masterSlave };

class BusMessage
{

public:
	BusMessage(const std::string command, const bool poll, const bool scan);
	~BusMessage();

	MessageType getType() const { return m_type; }
	bool isPoll() const { return m_poll; }
	bool isScan() const { return m_scan; }

	SymbolString getCommand() const { return m_command; }
	SymbolString getResult() const { return m_result; }

	bool isErrorResult() const { return m_resultCode < 0; }
	const char* getResultCodeCStr() const { return getResultCode(m_resultCode); }
	void setResult(const SymbolString result, const int resultCode)
		{ m_result = result; m_resultCode = resultCode; }

	const std::string getMessageStr();

	void waitSignal() { pthread_cond_wait(&m_cond, &m_mutex); } // TODO timeout
	void sendSignal() { pthread_cond_signal(&m_cond); }

private:
	MessageType m_type;
	bool m_poll;
	bool m_scan;
	SymbolString m_command;
	SymbolString m_result;
	int m_resultCode;

	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
};


class BusLoop : public Thread
{

public:
	BusLoop(Commands* commands);
	~BusLoop();

	void* run();
	void stop() { m_stop = true; }

	void addMessage(BusMessage* message) { m_busQueue.add(message); }

	void dump() { m_dumpState == true ? m_dumpState = false :  m_dumpState = true ; }
	void raw() { m_logRawData == true ? m_logRawData = false :  m_logRawData = true ; }

	void reload(Commands* commands) { m_commands = commands; }

	void scan(const bool full=false) { m_scan = true; m_scanFull = full; m_scanIndex = 0; }

private:
	Commands* m_commands;
	Port* m_port;

	Dump* m_dump;
	bool m_dumpState;

	bool m_logRawData;

	bool m_stop;

	int m_lockCounter;
	bool m_priorRetry;

	WQueue<BusMessage*> m_busQueue;
	SymbolString m_sstr;

	double m_pollInterval;
	long m_recvTimeout;
	int m_sendRetries;
	int m_lockRetries;
	long m_acquireTime;

	std::vector<unsigned char> m_slave;

	bool m_scan;
	bool m_scanFull;
	size_t m_scanIndex;

	unsigned char fetchByte();
	void collectCycData(const int numRecv);
	void analyseCycData();
	void addPollMessage();
	int acquireBus();
	BusMessage* sendCommand();
	int sendByte(const unsigned char sendByte);
	int recvSlaveAck(unsigned char& recvByte);
	int recvSlaveData(SymbolString& result);
	void collectSlave();
	void addScanMessage();

};

#endif // BUSLOOP_H_

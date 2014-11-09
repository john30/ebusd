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

#ifndef EBUSLOOP_H_
#define EBUSLOOP_H_

#include "commands.h"
#include "port.h"
#include "dump.h"
#include "buscommand.h"
#include "wqueue.h"
#include "thread.h"

/** the maximum time [us] allowed for retrieving a byte from an addressed slave */
#define RECV_TIMEOUT 10000

using namespace libebus;


class EBusLoop : public Thread
{

public:
	EBusLoop(Commands* commands);
	~EBusLoop();

	void* run();
	void stop() { m_stop = true; }

	void addBusCommand(BusCommand* busCommand) { m_sendBuffer.add(busCommand); }

	void dump() { m_dumpState == true ? m_dumpState = false :  m_dumpState = true ; }
	void raw() { m_logRawData == true ? m_logRawData = false :  m_logRawData = true ; }

	void newCommands(Commands* commands) { m_commands = commands; }

private:
	Commands* m_commands;
	Port* m_port;

	Dump* m_dump;
	bool m_dumpState;

	bool m_logRawData;

	bool m_stop;

	int m_lockCounter;
	bool m_priorRetry;

	WQueue<BusCommand*> m_sendBuffer;
	SymbolString m_sstr;

	double m_pollInterval;
	long m_recvTimeout;
	int m_sendRetries;
	int m_lockRetries;
	long m_acquireTime;

	unsigned char fetchByte();
	void collectCycData(const int numRecv);
	void analyseCycData();
	void addPollCommand();
	int acquireBus();
	BusCommand* sendCommand();
	int sendByte(const unsigned char sendByte);
	int recvSlaveAck(unsigned char& recvByte);
	int recvSlaveData(SymbolString& result);

};

#endif // EBUSLOOP_H_

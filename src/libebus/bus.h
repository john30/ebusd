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

#ifndef LIBEBUS_BUS_H_
#define LIBEBUS_BUS_H_

#include "symbol.h"
#include "result.h"
#include "port.h"
#include "dump.h"
#include "buscommand.h"
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <queue>

namespace libebus
{


// the maximum time allowed for retrieving a byte from an addressed slave
#define RECV_TIMEOUT 10000

class Bus
{

public:
	Bus(const std::string deviceName, const bool noDeviceCheck, const long recvTimeout,
		const std::string dumpFile, const long dumpSize, const bool dumpState);
	~Bus();

	void connect() { m_port->open(); }
	void disconnect() { if (m_port->isOpen() == true) m_port->close(); }
	bool isConnected() { return m_port->isOpen(); }

	void printBytes() const;

	int proceed();
	SymbolString getCycData();

	void addCommand(BusCommand* busCommand) { m_sendBuffer.push(busCommand); }

	int getBus(const unsigned char byte);
	BusCommand* sendCommand();
	BusCommand* delCommand();

	void setDumpState(const bool dumpState) { m_dumpState = dumpState; }

private:
	Port* m_port;
	SymbolString m_sstr;
	std::queue<SymbolString> m_cycBuffer;
	std::queue<BusCommand*> m_sendBuffer;

	const long m_recvTimeout;

	Dump* m_dump;
	bool m_dumpState;

	bool m_busLocked;
	bool m_busPriorRetry;

	int proceedCycData(const unsigned char byte);
	int sendByte(const unsigned char byte_sent);
	unsigned char recvByte();
	int recvSlaveDataAndCRC(SymbolString& result);

};


} //namespace

#endif // LIBEBUS_BUS_H_

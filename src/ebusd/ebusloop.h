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

//~ #include "bus.h"
#include "commands.h"
#include "port.h"
#include "dump.h"
#include "buscommand.h"
#include "wqueue.h"
#include "thread.h"

using namespace libebus;


class EBusLoop : public Thread
{

public:
	EBusLoop(Commands* commands);
	~EBusLoop();

	void* run();
	void stop() { m_stop = true; }

	void addBusCommand(BusCommand* busCommand) { m_sendBuffer.add(busCommand); }

	//~ void dump(const bool dumpState) { m_bus->setDumpState(dumpState); }
	void dump(const bool dumpState) { m_dumpState = dumpState; }

	void newCommands(Commands* commands) { m_commands = commands; }

private:
	Commands* m_commands;
	Port* m_port;

	Dump* m_dump;
	bool m_dumpState;

	bool m_logRawData;

	bool m_stop;

	SymbolString m_sstr;

	//~ std::string m_deviceName;
	//~ bool m_noDeviceCheck;
	//~ Bus* m_bus;

	WQueue<BusCommand*> m_sendBuffer;
	//~ int m_retries;
	//~ int m_lookbusretries;
	//~ double m_pollInterval;

	unsigned char recvByte();
	void analyseCycData(SymbolString data) const;

};

#endif // EBUSLOOP_H_

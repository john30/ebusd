/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
 */

#include "ebusloop.h"
#include "logger.h"
#include "appl.h"
#include <iostream>

extern LogInstance& L;
extern Appl& A;

EBusLoop::EBusLoop() : m_stop(false)
{
	m_deviceName = A.getParam<const char*>("p_device");
	
	m_bus = new Bus(m_deviceName,
			A.getParam<bool>("p_nodevicecheck"),
			A.getParam<const char*>("p_dumpfile"),
		        A.getParam<long>("p_dumpsize"),
		        A.getParam<bool>("p_dump"));
		        
	m_bus->connect();

	if (m_bus->isConnected() == false)
		L.log(bus, error, "can't open %s", m_deviceName.c_str());
}

EBusLoop::~EBusLoop()
{
	m_bus->disconnect();
	
	if (m_bus->isConnected() == true)
		L.log(bus, error, "error during disconnect.");
		
	delete m_bus;
}

void* EBusLoop::run()
{
	int busResult;
	bool busCommandActive = false;
	
	for (;;) {
		if (m_bus->isConnected() == true) {

			// work on bus
			busResult = m_bus->proceed();

			// new cyc message arrived
			if (busResult == 2) {
				std::string data = m_bus->getCycData();
				L.log(bus, debug, "%s", data.c_str());
				m_cycBuffer.add(data);
			}

			// add new bus command to send
			if (busResult == 4 && busCommandActive == false && m_sendBuffer.size() != 0) {
				BusCommand* busCommand = m_sendBuffer.remove();
				m_bus->addCommand(busCommand);
				busCommandActive = true;
			}

			// send bus command
			if (busResult == 1 && busCommandActive == true) {
				L.log(bus, event, " getBus success");
				m_bus->sendCommand();
				BusCommand* busCommand = m_bus->recvCommand();
				L.log(bus, trace, " %s", busCommand->getResult().c_str());
				m_recvBuffer.add(busCommand);
				busCommandActive = false;
			}

			if (busResult == 0)
				L.log(bus, event, " getBus failure");

			if (busResult == -1)
				L.log(bus, event, " getBus error");
	
		} else {
			sleep(10);
			m_bus->connect();
			
			if (m_bus->isConnected() == false)
				L.log(bus, error, "can't open %s", m_deviceName.c_str());
		}

		if (m_stop == true) {
			m_bus->disconnect();	
			return NULL;
		}
	}

	return NULL;
}


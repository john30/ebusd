/*
 * Copyright (C) Roland Jax 2012-2014 <roland.jax@liwest.at>
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

#include "ebusloop.h"
#include "logger.h"
#include "appl.h"

extern LogInstance& L;
extern Appl& A;

EBusLoop::EBusLoop(Commands* commands) : m_commands(commands), m_stop(false)
{
	m_deviceName = A.getParam<const char*>("p_device");

	m_bus = new Bus(m_deviceName,
			A.getParam<bool>("p_nodevicecheck"),
			A.getParam<long>("p_recvtimeout"),
			A.getParam<const char*>("p_dumpfile"),
		        A.getParam<long>("p_dumpsize"),
		        A.getParam<bool>("p_dump"));

	m_retries = A.getParam<int>("p_retries");

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
	int retries = 0;
	bool busCommandActive = false;

	for (;;) {
		if (m_bus->isConnected() == true) {

			// work on bus
			busResult = m_bus->proceed();

			// new cyc message arrived
			if (busResult == 2) {
				std::string data = m_bus->getCycData();
				L.log(bus, trace, "%s", data.c_str());

				int index = m_commands->storeData(data);

				if (index == -1) {
					L.log(bus, debug, " command not found");

				} else if (index == -2) {
					L.log(bus, debug, " no commands defined");

				} else if (index == -3) {
					L.log(bus, debug, " search skipped - string too short");

				} else {
					std::string tmp;
					tmp += (*m_commands)[index][0];
					tmp += " ";
					tmp += (*m_commands)[index][1];
					tmp += " ";
					tmp += (*m_commands)[index][2];
					L.log(bus, event, " [%d] %s", index, tmp.c_str());
				}

			}

			// add new bus command to send
			if (busResult == 4 && busCommandActive == false && m_sendBuffer.size() != 0) {
				BusCommand* busCommand = m_sendBuffer.remove();
				L.log(bus, debug, " type: %s msg: %s",
				      busCommand->getType().c_str(), busCommand->getCommand().c_str());
				m_bus->addCommand(busCommand);
				L.log(bus, debug, " addCommand success");
				busCommandActive = true;
			}

			// send bus command
			if (busResult == 1 && busCommandActive == true) {
				L.log(bus, trace, " getBus success");
				m_bus->sendCommand();
				BusCommand* busCommand = m_bus->recvCommand();
				L.log(bus, trace, " %s", busCommand->getResult().c_str());

				if (busCommand->getResult().c_str()[0] == '-' && retries < m_retries) {
					retries++;
					L.log(bus, trace, " retry number: %d", retries);
					busCommand->setResult(std::string());
					m_bus->addCommand(busCommand);
				} else {
					retries = 0;
					m_recvBuffer.add(busCommand);
					busCommandActive = false;
				}
			}

			if (busResult == 0)
				L.log(bus, trace, " getBus failure");

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


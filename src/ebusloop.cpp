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

	m_lookbusretries = A.getParam<int>("p_lookbusretries");

	m_pollInterval = A.getParam<int>("p_pollinterval");

	m_logAutoSyn = A.getParam<bool>("p_logautosyn");

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
	int lookbusretries = 0;
	bool busCommandActive = false;

	// polling
	time_t pollStart, pollEnd;
	time(&pollStart);
	double pollDelta = 0.0;
	bool pollCommandActive = false;

	for (;;) {
		if (m_bus->isConnected() == true) {

			// work on bus
			busResult = m_bus->proceed();

			// new cyc message arrived
			if (busResult == RESULT_SYN || busResult == RESULT_BUS_LOCKED) {
				std::string data = m_bus->getCycData();

				if (data.size() == 0 && m_logAutoSyn == true)
					L.log(bus, trace, "%s", "aa");

				if (data.size() != 0) {
					L.log(bus, trace, "%s", data.c_str());

					int index = m_commands->storeCycData(data);

					if (index == -1) {
						L.log(bus, debug, " command not found");

					} else if (index == -2) {
						L.log(bus, debug, " no commands defined");

					} else if (index == -3) {
						L.log(bus, debug, " search skipped - string too short");

					} else {
						std::string tmp;
						tmp += (*m_commands)[index][1];
						tmp += " ";
						tmp += (*m_commands)[index][2];
						L.log(bus, event, " cycle   [%d] %s", index, tmp.c_str());
					}
				}

				if (busResult == RESULT_BUS_LOCKED)
					L.log(bus, trace, "bus locked");
			}

			// add new bus command to send
			if (busResult == RESULT_SYN && busCommandActive == false && m_sendBuffer.size() != 0) {
				BusCommand* busCommand = m_sendBuffer.remove();
				L.log(bus, debug, " type: %s msg: %s",
				      busCommand->getTypeCStr(), busCommand->getCommand().c_str());
				m_bus->addCommand(busCommand);
				L.log(bus, debug, " addCommand success");
				busCommandActive = true;
			}

			// add new polling command
			if (m_commands->sizePolDB() > 0) {
				// check polling delta
				time(&pollEnd);
				pollDelta = difftime(pollEnd, pollStart);

				// add new polling command to send
				if (busResult == RESULT_SYN && busCommandActive == false && pollDelta >= m_pollInterval) {
					L.log(bus, trace, "polling Intervall reached");

					int index = m_commands->nextPolCommand();
					if (index < 0) {
						L.log(bus, error, "polling index out of range");
						time(&pollStart);
						continue;
					}

					std::string tmp;
					tmp += (*m_commands)[index][1];
					tmp += " ";
					tmp += (*m_commands)[index][2];
					L.log(bus, event, " polling [%d] %s", index, tmp.c_str());

					std::string ebusCommand(A.getParam<const char*>("p_address"));
					ebusCommand += m_commands->getEbusCommand(index);
					std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

					BusCommand* busCommand = new BusCommand(ebusCommand);
					L.log(bus, trace, " type: %s msg: %s", busCommand->getTypeCStr(), ebusCommand.c_str());

					m_bus->addCommand(busCommand);
					L.log(bus, debug, " addCommand success");
					busCommandActive = true;
					pollCommandActive = true;

					time(&pollStart);
				}

			}

			// send bus command
			if (busResult == RESULT_BUS_ACQUIRED && busCommandActive == true) {
				L.log(bus, trace, " getBus success");
				lookbusretries = 0;
				m_bus->sendCommand();
				BusCommand* busCommand = m_bus->recvCommand();
				L.log(bus, trace, " %s", busCommand->getResult().c_str());

				if (busCommand->isErrorResult() && retries < m_retries) {
					retries++;
					L.log(bus, trace, " retry number: %d", retries);
					busCommand->setResult(std::string(), RESULT_OK);
					m_bus->addCommand(busCommand);
				} else {
					retries = 0;
					if (pollCommandActive == true) {
						// only save correct results
						if (!busCommand->isErrorResult())
							m_commands->storePolData(busCommand->getResult().c_str());

						delete busCommand;
						pollCommandActive = false;
					} else {
						m_recvBuffer.add(busCommand);
					}

					busCommandActive = false;
				}
			}

			// get bus retry
			if (busResult == RESULT_BUS_PRIOR_RETRY)
				L.log(bus, trace, " getBus prior retry");

			if (busResult == RESULT_ERR_BUS_LOST) {
				L.log(bus, trace, " getBus failure");
				if (lookbusretries >= m_lookbusretries) {
					L.log(bus, event, " getBus failed - command deleted");
					m_bus->delCommand();
					lookbusretries = 0;
					busCommandActive = false;
					pollCommandActive = false;
				}else {
					lookbusretries++;
				}
			}

			if (busResult == RESULT_ERR_SEND)
				L.log(bus, event, " getBus send error");

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


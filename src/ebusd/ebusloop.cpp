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

#include "ebusloop.h"
#include "logger.h"
#include "appl.h"

extern LogInstance& L;
extern Appl& A;

EBusLoop::EBusLoop(Commands* commands) : m_commands(commands), m_stop(false)
{
	m_port = new Port(A.getParam<const char*>("p_device"), A.getParam<bool>("p_nodevicecheck"));

	m_port->open();

	if (m_port->isOpen() == false)
		L.log(bus, error, "can't open %s", A.getParam<const char*>("p_device"));


	m_dump = new Dump(A.getParam<const char*>("p_dumpfile"), A.getParam<long>("p_dumpsize"));

	m_dumpState = A.getParam<bool>("p_dump");

	m_logRawData = A.getParam<bool>("p_lograwdata");

	//~ m_deviceName = A.getParam<const char*>("p_device");

	//~ m_bus = new Bus(m_deviceName,
			//~ A.getParam<bool>("p_nodevicecheck"),
			//~ A.getParam<long>("p_recvtimeout"),
			//~ A.getParam<const char*>("p_dumpfile"),
		        //~ A.getParam<long>("p_dumpsize"),
		        //~ A.getParam<bool>("p_dump"));

	//~ m_retries = A.getParam<int>("p_retries");
//~
	//~ m_lookbusretries = A.getParam<int>("p_lookbusretries");
//~
	//~ m_pollInterval = A.getParam<int>("p_pollinterval");
//~


	//~ m_bus->connect();

	//~ if (m_bus->isConnected() == false)
		//~ L.log(bus, error, "can't open %s", m_deviceName.c_str());
}

EBusLoop::~EBusLoop()
{
	if (m_port->isOpen() == true)
		m_port->close();

	delete m_port;
	delete m_dump;
	//~ m_bus->disconnect();
//~
	//~ if (m_bus->isConnected() == true)
		//~ L.log(bus, error, "error during disconnect.");
//~
	//~ delete m_bus;
}

void* EBusLoop::run()
{
	bool busLock = false;

	for (;;) {
		if (m_port->isOpen() == true) {
			unsigned char byte;
			ssize_t numBytes;

			// read device - no timeout needed (AUTO-SYN)
			numBytes = m_port->recv(0);

			if (numBytes < 0) {
				L.log(bus, error, " ERR_DEVICE: generic device error");
				continue;
			}

			for (int i = 0; i < numBytes; i++) {

				// fetch byte
				byte = recvByte();

				// collect cycle data
				if (byte != SYN)
					m_sstr.push_back(byte, true, false);

				// unlock bus
				if (byte == SYN && busLock == true) {
					busLock = false;
					L.log(bus, trace, " bus unlocked");
				}

				// analyse cycle data
				if (byte == SYN && m_sstr.size() > 0) {

					analyseCycData(m_sstr);

					if (m_sstr.size() == 1) {
						busLock = true;
						L.log(bus, trace, " bus locked");
					}

					m_sstr.clear();
				}
			}

			// send command
			if (m_sstr.size() == 0 && busLock == false
			&& m_sendBuffer.size() > 0) {
				// TODO: sendCommand....
			}

			// poll command - timer reached
			if (m_sstr.size() == 0 && busLock == false
			&& m_sendBuffer.size() > 0) {
				// TODO: pollCommand....
			}
		}
		else {
			// TODO: define max reopen
			sleep(10);
			m_port->open();

			if (m_port->isOpen() == false)
				L.log(bus, error, "can't open %s", A.getParam<const char*>("p_device"));

		}

		if (m_stop == true) {
			if (m_port->isOpen() == true)
				m_port->close();

			return NULL;
		}

	}

	return NULL;
}

unsigned char EBusLoop::recvByte()
{
	unsigned char byte;

	// fetch byte
	byte = m_port->byte();

	if (m_dumpState == true)
		m_dump->write((const char*) &byte);

	if (m_logRawData == true)
		L.log(bus, event, "%02x", byte);

	return byte;
}

void EBusLoop::analyseCycData(SymbolString data) const
{
	L.log(bus, trace, "%s", data.getDataStr().c_str());

	int index = m_commands->storeCycData(data.getDataStr());

	if (index == -1) {
		L.log(bus, debug, " command not found");
	}
	else if (index == -2) {
		L.log(bus, debug, " no commands defined");
	}
	else if (index == -3) {
		L.log(bus, debug, " search skipped - string too short");
	}
	else {
		std::string tmp;
		tmp += (*m_commands)[index][1];
		tmp += " ";
		tmp += (*m_commands)[index][2];
		L.log(bus, event, " cycle   [%d] %s", index, tmp.c_str());
	}
}

/*
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

	for (;;) {
		if (m_bus->isConnected() == true) {

			// work on bus
			busResult = m_bus->proceed();

			// new cyc message arrived
			if (busResult == RESULT_SYN || busResult == RESULT_BUS_LOCKED) {
				SymbolString data = m_bus->getCycData();

				if (data.size() == 0 && m_logAutoSyn == true)
					L.log(bus, trace, "aa");

				if (data.size() != 0) {
					L.log(bus, trace, "%s", data.getDataStr().c_str());

					int index = m_commands->storeCycData(data.getDataStr());

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
				L.log(bus, debug, " msg: %s", busCommand->getCommand().getDataStr().c_str());
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

					BusCommand* busCommand = new BusCommand(ebusCommand, true);
					L.log(bus, trace, " msg: %s", ebusCommand.c_str());

					m_bus->addCommand(busCommand);
					L.log(bus, debug, " addCommand success");
					busCommandActive = true;

					time(&pollStart);
				}

			}

			// send bus command
			if (busResult == RESULT_BUS_ACQUIRED && busCommandActive == true) {
				L.log(bus, trace, " getBus success");
				lookbusretries = 0;
				BusCommand* busCommand = m_bus->sendCommand();
				L.log(bus, trace, " %s", busCommand->getMessageStr().c_str());

				if (busCommand->isErrorResult() == true && retries < m_retries) {
					retries++;
					L.log(bus, trace, " retry number: %d", retries);
					busCommand->setResult(std::string(), RESULT_OK);
					m_bus->addCommand(busCommand);
				} else {
					retries = 0;
					if (busCommand->isPoll() == true) {
						// only save correct results
						if (busCommand->isErrorResult() == false)
							m_commands->storePolData(busCommand->getMessageStr().c_str()); // TODO use getResult()

						delete busCommand;
					} else {
						busCommand->sendSignal();
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
					BusCommand* busCommand = m_bus->delCommand();
					if (busCommand->isPoll() == true) {
						delete busCommand;
					} else {
						busCommand->sendSignal();
					}
					lookbusretries = 0;
					busCommandActive = false;
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

*/

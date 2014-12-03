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

#include "bushandler.h"
#include "message.h"
#include "data.h"
#include "result.h"
#include "symbol.h"
#include "logger.h"
#include "appl.h"
#include <string>
#include <vector>
#include <cstring>
#include <time.h>

using namespace std;

extern Logger& L;
extern Appl& A;

/**
 * @brief Return the string corresponding to the @a BusState and send position.
 * @param state the @a BusState.
 * @param sendPos >=0 while sending data, -1 while receiving data.
 * @return the string corresponding to the @a BusState.
 */
const char* getStateCode(BusState state, int sendPos) {
	switch (state)
	{
	case bs_skip: return "skip";
	case bs_ready: return "ready";
	case bs_command: return sendPos < 0 ? "receive command" : "send command";
	case bs_commandAck: return sendPos < 0 ? "receive command ACK" : "send command ACK";
	case bs_response: return sendPos < 0 ? "receive response" : "send response";
	case bs_responseAck: return sendPos < 0 ? "receive response ACK" : "send response ACK";
	//case bs_validTransfer: return sendPos < 0 ? "after complete receive" : "after complete send";
	default: return "unknown state";
	}
}


BusRequest::BusRequest(SymbolString& master, SymbolString& slave)
	: m_master(master), m_slave(slave), m_finished(false)
{
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);
}

BusRequest::~BusRequest()
{
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

bool BusRequest::wait(int timeout)
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	t.tv_sec += timeout;
	int result = 0;

	pthread_mutex_lock(&m_mutex);

	while (m_finished == false && result == 0)
		result = pthread_cond_timedwait(&m_cond, &m_mutex, &t);

	if (result == 0 && m_finished == false)
		result = 1;

	pthread_mutex_unlock(&m_mutex);

	return result == 0;
}

void BusRequest::notify(bool finished)
{
	pthread_mutex_lock(&m_mutex);

	m_finished = finished;

	pthread_mutex_unlock(&m_mutex);
}


result_t BusHandler::sendAndWait(SymbolString& master, SymbolString& slave)
{
	BusRequest* request = new BusRequest(master, slave);

	m_requests.add(request);
	bool result = request->wait(5);
	if (result == false)
		m_requests.remove(request);
	delete request;

	return result == true ? RESULT_OK : RESULT_ERR_TIMEOUT;
}

void BusHandler::run()
{
	result_t result = RESULT_OK;
	do {
		if (m_port->isOpen() == true) {
			result = receiveSymbol();

			if (result != RESULT_OK)
				L.log(bus, error, " %s", getResultCode(result));

		}
		else {
			// TODO: define max reopen
			sleep(10);
			result = m_port->open();

			if (result != RESULT_OK)
				L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

		}

	} while (isRunning() == true);
}

result_t BusHandler::receiveSymbol()
{
	long timeout;
	ssize_t count;
	unsigned char sentSymbol = SYN;
	BusRequest* startRequest = NULL;
	if (m_state == bs_skip)
		timeout = 0;
	else if (m_sendPos >= 0) {
		timeout = SLAVE_RECV_TIMEOUT;
		if (m_sendPos+1 < m_request->m_master.size()) {
			m_sendPos++;
			sentSymbol = m_request->m_master[m_sendPos];
			if (m_port->send(&sentSymbol) != 1) {
				sentSymbol = SYN; // try again later // TODO error: send failed, abort send
				m_request->notify(false);
				m_request = NULL;
				m_sendPos = -1;
			}
		}
	}
	else {
		timeout = SYN_TIMEOUT;
		if (m_state == bs_ready && m_request == NULL) {
			startRequest = m_requests.next(false);
			if (startRequest != NULL) {
				// initiate arbitration
				sentSymbol = startRequest->m_master[0];
				if (m_port->send(&sentSymbol) != 1) {
					sentSymbol = SYN; // try again later // TODO error: send failed
					startRequest = NULL;
				}
			}
		}
	}

	count = m_port->recv(timeout, 1);

	if (count < 0)
		return setState(bs_skip, RESULT_ERR_DEVICE);

	if (count == 0) {
		if (m_state == bs_ready)
			return RESULT_OK; // TODO keep "no signal" within auto-syn state
		return setState(bs_skip, RESULT_ERR_TIMEOUT);
	}

	unsigned char symbol = m_port->byte();
	if (symbol == SYN) {
		m_repeat = false;
		return setState(bs_ready, RESULT_OK);
	}

	unsigned char headerLen, crcPos;
	result_t result;

	switch (m_state)
	{
	case bs_skip:
		return RESULT_OK;

	case bs_ready:
		if (symbol == ESC)
			return setState(bs_skip, RESULT_ERR_ESC);
		if (m_sendPos < 0 && sentSymbol != SYN) {
			// check arbitration
			if (symbol == sentSymbol) { // arbitration successful
				if (m_requests.remove(startRequest) == false) {
					sentSymbol = SYN; // try again later // TODO error: send failed, abort send
				} else {
					m_request = startRequest;
					m_sendPos = 0;
				}
			} else { // arbitration lost
				sentSymbol = SYN; // try again later // TODO error: lost arbitration
			}
		}
		result = m_command.push_back(symbol);
		if (result < RESULT_OK)
			return setState(bs_skip, result);

		return setState(bs_command, RESULT_OK);

	case bs_command:
		headerLen = 4;
		crcPos = m_command.size() > headerLen ? headerLen + 1 + m_command[headerLen] : 0xff;
		result = m_command.push_back(symbol, true, m_command.size() < crcPos);
		if (result < RESULT_OK)
			return setState(bs_skip, result);

		if (result == RESULT_OK && m_command.size() == crcPos + 1) { // CRC received
			m_commandCrcValid = m_command[headerLen + 1 + m_command[headerLen]] == m_command.getCRC();
			if (m_command[1] == BROADCAST) {
				if (m_commandCrcValid) {
					transferCompleted(tt_broadcast);
					return setState(bs_skip, RESULT_OK);
				}

				return setState(bs_skip, RESULT_ERR_CRC);
			}
			/*if (m_command[1] == m_ownSlaveAddress || m_command[1] == m_ownMasterAddress) {
				setState(bs_commandAck, RESULT_OK);
				m_sendPos = 0;
				symbol = m_commandCrcValid ? ACK : NAK;
				if (m_port->send(&symbol) <= 0)
					return setState(bs_skip, RESULT_ERR_SEND);
			}*/
			return setState(bs_commandAck, RESULT_OK);
		}
		return RESULT_OK;

	case bs_commandAck:
		if (symbol == ESC)
			return setState(bs_skip, RESULT_ERR_ESC);
		/*if (m_sendPos >= 0) {
			if (symbol == ACK && m_commandCrcValid == true)
				return setState();

			return setState()
		}*/
		if (symbol == ACK) {
			if (m_commandCrcValid == false)
				return setState(bs_skip, RESULT_ERR_ACK);

			if (isMaster(m_command[1]) == true) {
				transferCompleted(tt_masterMaster);
				return setState(bs_skip, RESULT_OK);
			}

			return setState(bs_response, RESULT_OK);
		}
		if (symbol == NAK) {
			if (m_repeat == false) {
				m_repeat = true;
				return setState(bs_ready, RESULT_ERR_NAK);
			}
			return setState(bs_skip, RESULT_ERR_NAK);
		}
		return setState(bs_skip, RESULT_ERR_ACK);

	case bs_response:
		headerLen = 0;
		crcPos = m_response.size() > headerLen ? headerLen + 1 + m_response[headerLen] : 0xff;
		result = m_response.push_back(symbol, true, m_response.size() < crcPos);
		if (result < RESULT_OK)
			return setState(bs_skip, result);

		if (result == RESULT_OK && m_response.size() == crcPos + 1) { // CRC received
			m_responseCrcValid = m_response[headerLen + 1 + m_response[headerLen]] == m_response.getCRC();
			/*if (m_command[1] == m_ownSlaveAddress || m_command[1] == m_ownMasterAddress) {
				setState(bs_responseAck, RESULT_OK);
				m_sendPos = 0;
				symbol = m_responseCrcValid ? ACK : NAK;
				if (m_port->send(&symbol) <= 0)
					return setState(bs_skip, RESULT_ERR_SEND);
			}*/
			return setState(bs_responseAck, RESULT_OK);
		}
		return RESULT_OK;

	case bs_responseAck:
		if (symbol == ESC)
			return setState(bs_skip, RESULT_ERR_ESC);
		/*if (m_sendPos >= 0) {
			if (symbol == ACK && m_responseCrcValid == true)
				return setState();

			return setState()
		}*/
		if (symbol == ACK) {
			if (m_responseCrcValid == false)
				return setState(bs_skip, RESULT_ERR_ACK);

			transferCompleted(tt_masterSlave);
			return setState(bs_skip, RESULT_OK);
		}
		if (symbol == NAK) {
			if (m_repeat == false) {
				m_repeat = true;
				return setState(bs_response, RESULT_ERR_NAK);
			}
			return setState(bs_skip, RESULT_ERR_NAK);
		}
		return setState(bs_skip, RESULT_ERR_ACK);
	}

	return RESULT_OK;
}

result_t BusHandler::setState(BusState state, result_t result)
{
	if (state == m_state)
		return result;

	if (result < RESULT_OK || (result != RESULT_OK && state == bs_skip))
		L.log(bus, error, " %s during %s, switching to %s", getResultCode(result), getStateCode(m_state, m_sendPos), getStateCode(state, m_sendPos));

	m_state = state;
	if (state == bs_ready || state == bs_skip) {
		m_command.clear();
		m_commandCrcValid = false;
		m_response.clear();
		m_responseCrcValid = false;
		m_sendPos = -1;
	}
	if (state == bs_skip)
		m_repeat = false;

	return result;
}

void BusHandler::transferCompleted(TransferType type)
{
	switch (type)
	{
	case tt_broadcast:
		L.log(bus, trace, "received broadcast %s", m_command.getDataStr().c_str());
		break;
	case tt_masterMaster:
		L.log(bus, trace, "received master %s", m_command.getDataStr().c_str());
		break;
	case tt_masterSlave:
		L.log(bus, trace, "received master %s, slave %s", m_command.getDataStr().c_str(), m_response.getDataStr().c_str());
		break;
	default:
		return;
	}
	Message* msg = m_messages->find(m_command);
	if (msg != NULL) {
		ostringstream output;
		result_t result = msg->decode(m_command, m_response, output);
		if (result != RESULT_OK)
			L.log(bus, error, "unable to parse %s %s: %s", msg->getClass().c_str(), msg->getName().c_str(), getResultCode(result));
		else
			L.log(bus, trace, "%s %s: %s", msg->getClass().c_str(), msg->getName().c_str(), output.str().c_str());
	}
}

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
 * @brief Return the string corresponding to the @a BusState.
 * @param state the @a BusState.
 * @return the string corresponding to the @a BusState.
 */
const char* getStateCode(BusState state) {
	switch (state)
	{
	case bs_skip:       return "skip";
	case bs_ready:      return "ready";
	case bs_sendCmd:    return "send command";
	case bs_recvCmdAck: return "receive command ACK";
	case bs_recvRes:    return "receive response";
	case bs_sendResAck: return "send response ACK";
	case bs_recvCmd:    return "receive command";
	case bs_recvResAck: return "receive response ACK";
//	case bs_sendRes:    return "send response";
//	case bs_sendCmdAck: return "send command ACK";
	case bs_sendSyn:    return "send SYN";
	default:            return "unknown";
	}
}


BusRequest::BusRequest(SymbolString& master, SymbolString& slave)
	: m_master(master), m_slave(slave), m_finished(false), m_result(RESULT_SYN)
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

void BusRequest::notify(result_t result)
{
	pthread_mutex_lock(&m_mutex);

	m_result = result;
	m_finished = true;

	pthread_mutex_unlock(&m_mutex);
}


result_t BusHandler::sendAndWait(SymbolString& master, SymbolString& slave)
{
	BusRequest* request = new BusRequest(master, slave);

	m_requests.add(request);
	bool success = request->wait(5);
	if (success == false)
		m_requests.remove(request);
	result_t result = request->m_result;
	delete request;

	return success == true ? result : RESULT_ERR_TIMEOUT;
}

void BusHandler::run()
{
	do {
		if (m_port->isOpen() == true)
			handleSymbol();
		else {
			// TODO: define max reopen
			sleep(10);
			result_t result = m_port->open();

			if (result != RESULT_OK)
				L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

		}

	} while (isRunning() == true);
}

#define RECV_TIMEOUT 4500

result_t BusHandler::handleSymbol()
{
	long timeout = SYN_TIMEOUT;
	unsigned char sendSymbol = ESC;
	bool sending = false;

	// check if another symbol has to be sent and determine timeout for receive
	switch (m_state)
	{
	case bs_skip:
		timeout = 0; // endless
		break;

	case bs_ready:
		m_request = m_requests.next(false);
		if (m_request != NULL) { // initiate arbitration
			sendSymbol = m_request->m_master[0];
			sending = true;
		}
		break;

	case bs_recvCmd:
	case bs_recvCmdAck:
	case bs_recvRes:
	case bs_recvResAck:
		timeout = SLAVE_RECV_TIMEOUT;
		break;

	case bs_sendCmd:
		if (m_request != NULL) {
			sendSymbol = m_request->m_master[m_nextSendPos];
			sending = true;
		}
		break;

	case bs_sendResAck:
		if (m_request != NULL) {
			sendSymbol = m_responseCrcValid ? ACK : NAK;
			sending = true;
		}
		break;

	case bs_sendSyn:
		sendSymbol = SYN;
		sending = true;
		break;
	}

	// send symbol if necessary
	if (sending == true) {
		if (m_port->send(&sendSymbol, 1) == 1)
			timeout = RECV_TIMEOUT;
		else {
			sending = false;
			timeout = 0;
			setState(bs_skip, RESULT_ERR_SEND);
		}
	}

	// receive next symbol (optionally check reception of sent symbol)
	ssize_t count = m_port->recv(timeout, 1);

	if (count < 0)
		return setState(bs_skip, count);

	if (count == 0) {
		if (m_state == bs_ready)
			return RESULT_OK; // TODO keep "no signal" within auto-syn state
		return setState(bs_skip, RESULT_ERR_TIMEOUT);
	}

	unsigned char recvSymbol = m_port->byte();
	if (recvSymbol == SYN)
		return setState(bs_ready, RESULT_SYN);

	unsigned char headerLen, crcPos;
	result_t result;

	switch (m_state)
	{
	case bs_skip:
		return RESULT_OK;

	case bs_ready:
		if (m_request != NULL && sending == true) {
			// check arbitration
			if (recvSymbol == sendSymbol) { // arbitration successful
				if (m_requests.remove(m_request) == false) {
					// request already timed out
					m_request = NULL;
					return setState(bs_sendSyn, RESULT_ERR_TIMEOUT);
				}
				m_nextSendPos = 1;
				m_repeat = false;
				return setState(bs_sendCmd, RESULT_OK);
			}
			// arbitration lost
			setState(m_state, RESULT_ERR_BUS_LOST); // try again later
		}
		result = m_command.push_back(recvSymbol, false); // expect no escaping for master address
		if (result < RESULT_OK)
			return setState(bs_skip, result);

		m_repeat = false;
		return setState(bs_recvCmd, RESULT_OK);

	case bs_recvCmd:
		headerLen = 4;
		crcPos = m_command.size() > headerLen ? headerLen + 1 + m_command[headerLen] : 0xff;
		result = m_command.push_back(recvSymbol, true, m_command.size() < crcPos);
		if (result < RESULT_OK)
			return setState(bs_skip, result);

		if (result == RESULT_OK && crcPos != 0xff && m_command.size() == crcPos + 1) { // CRC received
			unsigned char dstAddress = m_command[1];
			//if (isValidAddress(dstAddress) == false || isMaster(m_command[0]) == false)
			//	return setState(bs_skip, RESULT_ERR_INVALID_ADDR);

			m_commandCrcValid = m_command[headerLen + 1 + m_command[headerLen]] == m_command.getCRC();
			if (m_commandCrcValid) {
				if (dstAddress == BROADCAST) {
					transferCompleted(tt_broadcast);
					return setState(bs_skip, RESULT_OK);
				}
				//if (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress)
				//	return setState(bs_sendCmdAck, RESULT_OK);

				return setState(bs_recvCmdAck, RESULT_OK);
			}
			if (dstAddress == BROADCAST)
				return setState(bs_skip, RESULT_OK);

			//if (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress)
			//	return setState(bs_sendCmdAck, RESULT_ERR_CRC);
			if (m_repeat == true)
				return setState(bs_skip, RESULT_ERR_CRC);
			return setState(bs_recvCmdAck, RESULT_ERR_CRC);
		}
		return RESULT_OK;

	case bs_recvCmdAck:
		if (recvSymbol == ACK) {
			if (m_commandCrcValid == false)
				return setState(bs_skip, RESULT_ERR_ACK);

			if (m_request != NULL) {
				if (isMaster(m_request->m_master[1]) == true) {
					transferCompleted(tt_masterMaster);
					return setState(bs_sendSyn, RESULT_OK);
				}
			} else if (isMaster(m_command[1]) == true) {
				transferCompleted(tt_masterMaster);
				return setState(bs_skip, RESULT_OK);
			}

			m_repeat = false;
			return setState(bs_recvRes, RESULT_OK);
		}
		if (recvSymbol == NAK) {
			if (m_repeat == false) {
				m_repeat = true;
				m_nextSendPos = 0;
				m_command.clear();
				if (m_request != NULL)
					return setState(bs_sendCmd, RESULT_ERR_NAK);

				return setState(bs_recvCmd, RESULT_ERR_NAK);
			}
			if (m_request != NULL)
				return setState(bs_sendSyn, RESULT_ERR_NAK);

			return setState(bs_skip, RESULT_ERR_NAK);
		}
		if (m_request != NULL)
			return setState(bs_sendSyn, RESULT_ERR_ACK);

		return setState(bs_skip, RESULT_ERR_ACK);

	case bs_recvRes:
		headerLen = 0;
		crcPos = m_response.size() > headerLen ? headerLen + 1 + m_response[headerLen] : 0xff;
		result = m_response.push_back(recvSymbol, true, m_response.size() < crcPos);
		if (result < RESULT_OK) {
			if (m_request != NULL)
				return setState(bs_sendSyn, result);

			return setState(bs_skip, result);
		}
		if (result == RESULT_OK && crcPos != 0xff && m_response.size() == crcPos + 1) { // CRC received
			m_responseCrcValid = m_response[headerLen + 1 + m_response[headerLen]] == m_response.getCRC();
			if (m_responseCrcValid) {
				if (m_request != NULL)
					return setState(bs_sendResAck, RESULT_OK);

				return setState(bs_recvResAck, RESULT_OK);
			}
			if (m_repeat == true) {
				if (m_request != NULL)
					return setState(bs_sendSyn, RESULT_ERR_CRC);

				return setState(bs_skip, RESULT_ERR_CRC);
			}
			if (m_request != NULL)
				return setState(bs_sendResAck, RESULT_ERR_CRC);

			return setState(bs_recvResAck, RESULT_ERR_CRC);
		}
		return RESULT_OK;

	case bs_recvResAck:
		if (recvSymbol == ACK) {
			if (m_responseCrcValid == false)
				return setState(bs_skip, RESULT_ERR_ACK);

			transferCompleted(tt_masterSlave);
			return setState(bs_skip, RESULT_OK);
		}
		if (recvSymbol == NAK) {
			if (m_repeat == false) {
				m_repeat = true;
				m_response.clear();
				return setState(bs_recvRes, RESULT_ERR_NAK);
			}
			return setState(bs_skip, RESULT_ERR_NAK);
		}
		return setState(bs_skip, RESULT_ERR_ACK);

	case bs_sendCmd:
		if (m_request != NULL && sending == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				m_nextSendPos++;
				if (m_nextSendPos >= m_request->m_master.size()) {
					// master data completely sent
					if (m_request->m_master[1] == BROADCAST)
						return setState(bs_sendSyn, RESULT_OK);

					m_commandCrcValid = true;
					return setState(bs_recvCmdAck, RESULT_OK);
				}
				return RESULT_OK;
			}
		}
		return setState(bs_sendSyn, RESULT_ERR_INVALID_ARG);

	case bs_sendResAck:
		if (m_request != NULL && sending == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				return setState(bs_sendSyn, RESULT_OK);
			}
		}
		return setState(bs_sendSyn, RESULT_ERR_INVALID_ARG);

	case bs_sendSyn:
		if (sending == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				return setState(bs_skip, RESULT_OK);
			}
		}
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

	}

	return RESULT_OK;
}

result_t BusHandler::setState(BusState state, result_t result)
{
	if (state == m_state)
		return result;

	if (result < RESULT_OK || (result != RESULT_OK && state == bs_skip))
		L.log(bus, error, " %s during %s, switching to %s", getResultCode(result), getStateCode(m_state), getStateCode(state));

	m_state = state;
	if (m_request != NULL) {
		if (state == bs_sendSyn) {
			m_request->m_slave = m_response; // TODO nicer
			m_request->notify(result);
			m_request = NULL;
		} else if (result != RESULT_OK) {
			m_request->notify(result);
			m_request = NULL;
		}
	}

	if (state == bs_ready || state == bs_skip) {
		m_command.clear();
		m_commandCrcValid = false;
		m_response.clear();
		m_responseCrcValid = false;
		m_nextSendPos = 0;
	}

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
			L.log(bus, error, "unable to parse %s %s from %s / %s: %s", msg->getClass().c_str(), msg->getName().c_str(), m_command.getDataStr().c_str(), m_response.getDataStr().c_str(), getResultCode(result));
		else
			L.log(bus, trace, "%s %s: %s", msg->getClass().c_str(), msg->getName().c_str(), output.str().c_str());
	}
}

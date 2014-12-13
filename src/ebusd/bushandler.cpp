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
#include <iomanip>

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


result_t PollRequest::prepare(unsigned char ownMasterAddress)
{
	istringstream input;
	result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input);
	if (result == RESULT_OK)
		L.log(bus, event, "poll cmd: %s", m_master.getDataStr().c_str());
	return result;
}

void PollRequest::notify(result_t result)
{
	ostringstream output;
	if (result == RESULT_OK) {
		result = m_message->decode(pt_slaveData, m_slave, output); // decode data
	}
	if (result != RESULT_OK)
		L.log(bus, error, "poll %s failed: %s", m_message->getName().c_str(), getResultCode(result));
	else
		L.log(bus, event, "poll %s: %s", m_message->getName().c_str(), output.str().c_str());
}


result_t ScanRequest::prepare(unsigned char ownMasterAddress, unsigned char dstAddress)
{
	istringstream input;
	result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input, UI_FIELD_SEPARATOR, dstAddress);
	if (result == RESULT_OK)
		L.log(bus, event, "scan cmd: %s", m_master.getDataStr().c_str());
	return result;
}

void ScanRequest::notify(result_t result)
{
	if (result == RESULT_OK) {
		m_scanResult << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_master[1]) << UI_FIELD_SEPARATOR;
		result = m_message->decode(pt_slaveData, m_slave, m_scanResult); // decode data
	}
	if (result == RESULT_OK)
		L.log(bus, event, "scan result: %s", m_scanResult.str().c_str());
	else
		L.log(bus, error, "scan %2.2x failed: %s", m_master[1], getResultCode(result));
}


ActiveBusRequest::ActiveBusRequest(SymbolString& master, SymbolString& slave)
	: BusRequest(master, slave, false), m_finished(false), m_result(RESULT_SYN)
{
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);
}

ActiveBusRequest::~ActiveBusRequest()
{
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

bool ActiveBusRequest::wait(int timeout)
{
	m_finished = false;
	m_result = RESULT_SYN;
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

void ActiveBusRequest::notify(result_t result)
{
	if (result == RESULT_OK)
		L.log(bus, trace, "read res: %s", m_slave.getDataStr().c_str());

	pthread_mutex_lock(&m_mutex);

	m_result = result;
	m_finished = true;
	pthread_cond_signal(&m_cond);

	pthread_mutex_unlock(&m_mutex);
}


result_t BusHandler::sendAndWait(SymbolString& master, SymbolString& slave)
{
	result_t result = RESULT_SYN;
	ActiveBusRequest* request = new ActiveBusRequest(master, slave);

	for (int sendRetries=m_failedSendRetries+1; sendRetries>=0; sendRetries--) {
		m_requests.add(request);
		bool success = request->wait(1); // 1 second is still 3 times the theoretical worst-case request duration
		if (success == false)
			m_requests.remove(request);
		result = success == true ? request->m_result : RESULT_ERR_TIMEOUT;

		if (result == RESULT_OK)
			break;

		L.log(bus, error, "%s, %s", getResultCode(result), sendRetries>0 ? "retry send" : "give up");
		request->m_busLostRetries = 0;
	}

	delete request; // TODO may be unsave while run() is using the request

	return result;
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
		if (m_request != NULL)
			setState(bs_ready, RESULT_ERR_TIMEOUT); // just to be sure an old BusRequest is cleaned up
		if (m_remainLockCount == 0) {
			m_request = m_requests.next(false);
			if (m_request == NULL && m_pollInterval > 0) { // check for poll/scan
				time_t now;
				time(&now);
				if (m_lastPoll == 0 || difftime(now, m_lastPoll) > m_pollInterval) {
					Message* message = m_messages->getNextPoll();
					if (message != NULL) {
						m_lastPoll = now;
						PollRequest* request = new PollRequest(m_response, message);
						result_t ret = request->prepare(m_ownMasterAddress);
						if (ret != RESULT_OK) {
							L.log(bus, error, "prepare poll message: %s", getResultCode(ret));
							delete request;
						}
						else {
							m_request = request;
							m_requests.add(request);
						}
					}
				}
			}
			if (m_request != NULL) { // initiate arbitration
				sendSymbol = m_request->m_master[0];
				sending = true;
			}
		}
		break;

	case bs_recvCmd:
	case bs_recvCmdAck:
	case bs_recvRes:
	case bs_recvResAck:
		timeout = m_slaveRecvTimeout;
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

	case bs_sendCmdAck:
		if (m_request != NULL) {
			sendSymbol = m_commandCrcValid ? ACK : NAK;
			sending = true;
		}
		break;

	case bs_sendRes:
		if (m_request != NULL) {
			sendSymbol = m_response[m_nextSendPos];
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
			if (m_state == bs_ready)
				timeout = m_busAcquireTimeout;
			else
				timeout = SEND_TIMEOUT;
		else {
			sending = false;
			timeout = 0;
			setState(bs_skip, RESULT_ERR_SEND);
		}
	}

	// receive next symbol (optionally check reception of sent symbol)
	unsigned char recvSymbol;
	ssize_t count = m_port->recv(timeout, 1, &recvSymbol);

	if (count < 0) // count < 0 is a RESULT_ERR_ code
		return setState(bs_skip, count); // TODO keep "no signal" within auto-syn state

	//unsigned char recvSymbol = m_port->byte(); // TODO remove me
	if (recvSymbol == SYN) {
		if (sending == false && m_remainLockCount > 0)
			m_remainLockCount--;
		return setState(bs_ready, RESULT_SYN);
	}

	unsigned char headerLen, crcPos;
	result_t result;

	switch (m_state)
	{
	case bs_skip:
		return RESULT_OK;

	case bs_ready:
		if (m_request != NULL && sending == true) {
			if (m_requests.remove(m_request) == false) {
				// request already timed out
				return setState(bs_skip, RESULT_ERR_TIMEOUT);
			}
			// check arbitration
			if (recvSymbol == sendSymbol) { // arbitration successful
				m_nextSendPos = 1;
				m_repeat = false;
				return setState(bs_sendCmd, RESULT_OK);
			}
			// arbitration lost. if same priority class found, try again after next AUTO-SYN
			m_remainLockCount = isMaster(recvSymbol) ? 2 : 1;
			if ((recvSymbol & 0x0f) != (sendSymbol & 0x0f)
			        && m_lockCount > m_remainLockCount)
				// if different priority class found, try again after N AUTO-SYN symbols (at least next AUTO-SYN)
				m_remainLockCount = m_lockCount;
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
			m_commandCrcValid = m_command[headerLen + 1 + m_command[headerLen]] == m_command.getCRC();
			if (m_commandCrcValid) {
				if (dstAddress == BROADCAST) {
					receiveCompleted();
					return setState(bs_skip, RESULT_OK);
				}
				if (m_answer == true
				        && (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress))
					return setState(bs_sendCmdAck, RESULT_OK);

				return setState(bs_recvCmdAck, RESULT_OK);
			}
			if (dstAddress == BROADCAST)
				return setState(bs_skip, RESULT_ERR_CRC);

			if (m_answer == true
			        && (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress)) {
				return setState(bs_sendCmdAck, RESULT_ERR_CRC);
			}
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
					return setState(bs_sendSyn, RESULT_OK);
				}
			} else if (isMaster(m_command[1]) == true) {
				receiveCompleted();
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
					return setState(bs_sendCmd, RESULT_ERR_NAK, true);

				return setState(bs_recvCmd, RESULT_ERR_NAK);
			}
			if (m_request != NULL)
				return setState(bs_skip, RESULT_ERR_NAK);

			return setState(bs_skip, RESULT_ERR_NAK);
		}
		if (m_request != NULL)
			return setState(bs_skip, RESULT_ERR_ACK);

		return setState(bs_skip, RESULT_ERR_ACK);

	case bs_recvRes:
		headerLen = 0;
		crcPos = m_response.size() > headerLen ? headerLen + 1 + m_response[headerLen] : 0xff;
		result = m_response.push_back(recvSymbol, true, m_response.size() < crcPos);
		if (result < RESULT_OK) {
			if (m_request != NULL)
				return setState(bs_skip, result);

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

			receiveCompleted();
			return setState(bs_skip, RESULT_OK);
		}
		if (recvSymbol == NAK) {
			if (m_repeat == false) {
				m_repeat = true;
				m_response.clear();
				return setState(bs_recvRes, RESULT_ERR_NAK, true);
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
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

	case bs_sendResAck:
		if (m_request != NULL && sending == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				if (m_responseCrcValid == false) {
					if (m_repeat == false) {
						m_repeat = true;
						m_response.clear();
						return setState(bs_recvRes, RESULT_ERR_NAK, true);
					}
					return setState(bs_sendSyn, RESULT_ERR_ACK);
				}
				return setState(bs_sendSyn, RESULT_OK);
			}
		}
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

	case bs_sendCmdAck:
		if (sending == true && m_answer == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				if (m_commandCrcValid == false) {
					if (m_repeat == false) {
						m_repeat = true;
						m_command.clear();
						return setState(bs_recvCmd, RESULT_ERR_NAK, true);
					}
					return setState(bs_skip, RESULT_ERR_ACK);
				}
				if (isMaster(m_command[1]) == true)
					receiveCompleted(); // decode command and store value
					return setState(bs_skip, RESULT_OK);

				m_nextSendPos = 0;
				m_repeat = false;
				Message* message = m_messages->find(m_command);
				if (message == NULL || message->isPassive() == false || message->isSet() == true)
					return setState(bs_skip, RESULT_ERR_INVALID_ARG); // don't know this request or definition has wrong direction, deny

				// build response and store in m_response for sending back to requesting master
				result = message->prepareSlave(m_response);
				if (result != RESULT_OK)
					return setState(bs_skip, result);
				return setState(bs_sendRes, RESULT_OK);
			}
		}
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

	case bs_sendRes:
		if (sending == true && m_answer == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				m_nextSendPos++;
				if (m_nextSendPos >= m_response.size()) {
					// slave data completely sent
					return setState(bs_recvResAck, RESULT_OK);
				}
				return RESULT_OK;
			}
		}
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

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

result_t BusHandler::setState(BusState state, result_t result, bool firstRepetition)
{
	if (m_request != NULL) {
		if (result == RESULT_ERR_BUS_LOST && m_request->m_busLostRetries < m_busLostRetries) {
			L.log(bus, error, "%s, retry", getResultCode(result));
			m_request->m_busLostRetries++;
			m_requests.add(m_request); // repeat
			m_request = NULL;
		} else if (state == bs_sendSyn || (result != RESULT_OK && firstRepetition == false)) {
			L.log(bus, debug, "notify request: %s", getResultCode(result));
			m_request->m_slave = SymbolString(m_response, false, false);
			unsigned char dstAddress = m_request->m_master[1];
			if (result == RESULT_OK && isValidAddress(dstAddress, false) == true)
				m_seenAddresses[dstAddress] = true;
			m_request->notify(result);
			if (m_request->m_deleteOnFinish == true) {
				if (result == RESULT_OK && typeid(*m_request) == typeid(ScanRequest)) {
					string res = ((ScanRequest*)m_request)->m_scanResult.str();
					m_scanResults[dstAddress] = res;
				}
				delete m_request;
			}
			m_request = NULL;
		}
	}

	if (state == m_state)
		return result;

	if (result < RESULT_OK || (result != RESULT_OK && state == bs_skip))
		L.log(bus, debug, "%s during %s, switching to %s", getResultCode(result), getStateCode(m_state), getStateCode(state));
	else if (m_request != NULL || state == bs_sendCmd || state==bs_sendResAck || state==bs_sendSyn)
		L.log(bus, debug, "switching from %s to %s", getStateCode(m_state), getStateCode(state));
	m_state = state;

	if (state == bs_ready || state == bs_skip) {
		m_command.clear();
		m_commandCrcValid = false;
		m_response.clear();
		m_responseCrcValid = false;
		m_nextSendPos = 0;
	}

	return result;
}

void BusHandler::receiveCompleted()
{
	unsigned char dstAddress = m_command[1];
	bool master = isMaster(dstAddress);

	m_seenAddresses[m_command[0]] = true;
	if (dstAddress == BROADCAST)
		L.log(upd, trace, "update BC cmd: %s", m_command.getDataStr().c_str());
	else if (master == true) {
		L.log(upd, trace, "update MM cmd: %s", m_command.getDataStr().c_str());
		m_seenAddresses[dstAddress] = true;
	} else {
		L.log(upd, trace, "update MS cmd: %s / %s", m_command.getDataStr().c_str(), m_response.getDataStr().c_str());
		m_seenAddresses[dstAddress] = true;
	}

	Message* message = m_messages->find(m_command);
	if (message != NULL) {
		string clazz = message->getClass();
		string name = message->getName();
		ostringstream output;
		result_t result = message->decode(pt_masterData, m_command, output);
		if (result == RESULT_OK && dstAddress != BROADCAST && master == false)
			result = message->decode(pt_slaveData, m_response, output, output.str().empty() == false);
		if (result != RESULT_OK)
			L.log(upd, error, "unable to parse %s %s from %s / %s: %s", clazz.c_str(), name.c_str(), m_command.getDataStr().c_str(), m_response.getDataStr().c_str(), getResultCode(result));
		else {
			string data = output.str();
			L.log(upd, event, "update %s %s: %s", clazz.c_str(), name.c_str(), data.c_str());
		}
	}
}

result_t BusHandler::startScan(bool full)
{
	Message* scanMessage = m_scanMessage;
	if (scanMessage == NULL) {
		scanMessage = m_messages->find("", "scan", false);
	}
	if (scanMessage == NULL) {
		DataFieldSet* identFields = DataFieldSet::createIdentFields();
		scanMessage = m_scanMessage = new Message(false, false, 0x07, 0x04, identFields);
	}
	if (scanMessage == NULL)
		return RESULT_ERR_NOTFOUND;

	if (full == true)
		m_scanResults.clear();

	for (unsigned int slave=0; slave<=255; slave++) {
		if (isValidAddress(slave, false) == false || isMaster(slave) == true)
			continue;
		if (full == false && m_seenAddresses[slave] == false) {
			unsigned int master = slave+(256-5); // check if we saw the corresponding master already
			if (isMaster(master) == false || m_seenAddresses[slave] == false)
				continue;
		}

		ScanRequest* request = new ScanRequest(m_response, scanMessage);
		result_t result = request->prepare(m_ownMasterAddress, slave);
		if (result != RESULT_OK) {
			delete request;
			return result;
		}
		m_requests.add(request);
	}
	return RESULT_OK;
}

void BusHandler::formatScanResult(ostringstream& output)
{
	for (unsigned int slave=0; slave<=255; slave++) {
		map<unsigned char, string>::iterator it = m_scanResults.find(slave);
		if (it != m_scanResults.end())
			output << it->second << endl;
	}
}

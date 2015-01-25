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
#include "log.h"
#include <string>
#include <vector>
#include <deque>
#include <cstring>
#include <time.h>
#include <iomanip>

using namespace std;

/**
 * Return the string corresponding to the @a BusState.
 * @param state the @a BusState.
 * @return the string corresponding to the @a BusState.
 */
const char* getStateCode(BusState state) {
	switch (state)
	{
	case bs_noSignal:	return "no signal";
	case bs_skip:       return "skip";
	case bs_ready:      return "ready";
	case bs_sendCmd:    return "send command";
	case bs_recvCmdAck: return "receive command ACK";
	case bs_recvRes:    return "receive response";
	case bs_sendResAck: return "send response ACK";
	case bs_recvCmd:    return "receive command";
	case bs_recvResAck: return "receive response ACK";
	case bs_sendCmdAck: return "send command ACK";
	case bs_sendRes:    return "send response";
	case bs_sendSyn:    return "send SYN";
	default:            return "unknown";
	}
}


result_t PollRequest::prepare(unsigned char ownMasterAddress)
{
	istringstream input;
	result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input);
	if (result == RESULT_OK)
		logInfo(lf_bus, "poll cmd: %s", m_master.getDataStr().c_str());
	return result;
}

bool PollRequest::notify(result_t result, SymbolString& slave)
{
	ostringstream output;
	if (result == RESULT_OK) {
		result = m_message->decode(pt_slaveData, slave, output); // decode data
	}
	if (result != RESULT_OK)
		logError(lf_bus, "poll %s %s failed: %s", m_message->getClass().c_str(), m_message->getName().c_str(), getResultCode(result));
	else
		logNotice(lf_bus, "poll %s %s: %s", m_message->getClass().c_str(), m_message->getName().c_str(), output.str().c_str());

	return false;
}


result_t ScanRequest::prepare(unsigned char ownMasterAddress, unsigned char dstAddress)
{
	istringstream input;
	result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input, UI_FIELD_SEPARATOR, dstAddress);
	if (result == RESULT_OK)
		logInfo(lf_bus, "scan cmd: %s", m_master.getDataStr().c_str());
	return result;
}

bool ScanRequest::notify(result_t result, SymbolString& slave)
{
	unsigned char dstAddress = m_master[1];
	bool append = m_scanResults != NULL && m_scanResults->find(dstAddress) != m_scanResults->end();
	ostringstream scanResult;
	if (result == RESULT_OK) {
		if (append == false)
			scanResult << hex << setw(2) << setfill('0') << static_cast<unsigned>(dstAddress) << UI_FIELD_SEPARATOR;
		result = m_message->decode(pt_slaveData, slave, scanResult, append); // decode data
	}
	if (result != RESULT_OK) {
		logError(lf_bus, "scan %2.2x failed: %s", dstAddress, getResultCode(result));
		return false;
	}

	string str = scanResult.str();
	logNotice(lf_bus, "scan: %s", str.c_str());
	if (m_scanResults != NULL) {
		if (append == true)
			(*m_scanResults)[dstAddress] += str;
		else
			(*m_scanResults)[dstAddress] = str;
	}

	// check for remaining secondary messages
	if (m_messages.empty() == true)
		return false;

	m_message = m_messages.front();
	m_messages.pop_front();

	result = prepare(m_master[0], dstAddress);
	if (result != RESULT_OK)
		return false; // give up

	return true;
}


bool ActiveBusRequest::notify(result_t result, SymbolString& slave)
{
	if (result == RESULT_OK)
		logNotice(lf_bus, "read res: %s", slave.getDataStr().c_str());

	m_result = result;
	m_slave = SymbolString(slave, false, false);

	return false;
}


result_t BusHandler::sendAndWait(SymbolString& master, SymbolString& slave)
{
	result_t result = RESULT_SYN;
	ActiveBusRequest* request = new ActiveBusRequest(master, slave);

	for (int sendRetries=m_failedSendRetries+1; sendRetries>=0; sendRetries--) {
		m_nextRequests.add(request);
		bool success = m_finishedRequests.waitRemove(request);
		result = success == true ? request->m_result : RESULT_ERR_TIMEOUT;

		if (result == RESULT_OK)
			break;

		if (success == false || result == RESULT_ERR_NO_SIGNAL) {
			logError(lf_bus, "%s, give up", getResultCode(result));
			break;
		}
		logError(lf_bus, "%s, %s", getResultCode(result), sendRetries>0 ? "retry send" : "");

		request->m_busLostRetries = 0;
	}

	delete request;

	return result;
}

void BusHandler::run()
{
	do {
		if (m_port->isOpen() == true)
			handleSymbol();
		else {
			// TODO define max reopen
			sleep(10);
			result_t result = m_port->open();

			if (result != RESULT_OK)
				logError(lf_bus, "can't open %s", m_port->getDeviceName());

		}

	} while (isRunning() == true);
}

result_t BusHandler::handleSymbol()
{
	long timeout = SYN_TIMEOUT;
	unsigned char sendSymbol = ESC;
	bool sending = false;
	BusRequest* startRequest = NULL;

	// check if another symbol has to be sent and determine timeout for receive
	switch (m_state)
	{
	case bs_noSignal:
		timeout = SIGNAL_TIMEOUT;
		break;

	case bs_skip:
		timeout = SYN_TIMEOUT;
		break;

	case bs_ready:
		if (m_currentRequest != NULL)
			setState(bs_ready, RESULT_ERR_TIMEOUT); // just to be sure an old BusRequest is cleaned up
		if (m_remainLockCount == 0 && m_currentRequest == NULL) {
			startRequest = m_nextRequests.next(false);
			if (startRequest == NULL && m_pollInterval > 0) { // check for poll/scan
				time_t now;
				time(&now);
				if (m_lastPoll == 0 || difftime(now, m_lastPoll) > m_pollInterval) {
					Message* message = m_messages->getNextPoll();
					if (message != NULL) {
						m_lastPoll = now;
						PollRequest* request = new PollRequest(message);
						result_t ret = request->prepare(m_ownMasterAddress);
						if (ret != RESULT_OK) {
							logError(lf_bus, "prepare poll message: %s", getResultCode(ret));
							delete request;
						}
						else {
							startRequest = request;
							m_nextRequests.add(request);
						}
					}
				}
			}
			if (startRequest != NULL) { // initiate arbitration
				sendSymbol = m_ownMasterAddress;
				sending = true;
			}
		}
		break;

	case bs_recvCmd:
	case bs_recvCmdAck:
		timeout = m_slaveRecvTimeout;
		break;

	case bs_recvRes:
		if (m_response.size() > 0 || m_slaveRecvTimeout > SYN_TIMEOUT)
			timeout = m_slaveRecvTimeout;
		else
			timeout = SYN_TIMEOUT;
		break;

	case bs_recvResAck:
		timeout = m_slaveRecvTimeout;
		break;

	case bs_sendCmd:
		if (m_currentRequest != NULL) {
			sendSymbol = m_currentRequest->m_master[m_nextSendPos];
			sending = true;
		}
		break;

	case bs_sendResAck:
		if (m_currentRequest != NULL) {
			sendSymbol = m_responseCrcValid ? ACK : NAK;
			sending = true;
		}
		break;

	case bs_sendCmdAck:
		if (m_currentRequest != NULL) {
			sendSymbol = m_commandCrcValid ? ACK : NAK;
			sending = true;
		}
		break;

	case bs_sendRes:
		if (m_currentRequest != NULL) {
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
	result_t result;
	if (sending == true) {
		result = m_port->send(sendSymbol);
		if (result == RESULT_OK)
			if (m_state == bs_ready)
				timeout = m_busAcquireTimeout;
			else
				timeout = SEND_TIMEOUT;
		else {
			sending = false;
			timeout = 0;
			setState(bs_skip, result);
		}
	}

	// receive next symbol (optionally check reception of sent symbol)
	unsigned char recvSymbol;
	result = m_port->recv(timeout, recvSymbol);

	time_t now;
	time(&now);
	if (result != RESULT_OK) {
		if (difftime(now, m_lastReceive) > 1 // at least one full second has passed since last received symbol
			|| m_state == bs_noSignal)
			return setState(bs_noSignal, result);

		return setState(bs_skip, result);
	}

	m_lastReceive = now;
	if (recvSymbol == SYN) {
		if (sending == false && m_remainLockCount > 0 && m_command.size() != 1)
			m_remainLockCount--;
		else if (sending == false && m_remainLockCount == 0 && m_command.size() == 1)
			m_remainLockCount = 1; // wait for next AUTO-SYN after SYN / address / SYN (bus locked for own priority)
		return setState(bs_ready, RESULT_SYN);
	}

	unsigned char headerLen, crcPos;

	switch (m_state)
	{
	case bs_noSignal:
		return setState(bs_skip, RESULT_OK);

	case bs_skip:
		return RESULT_OK;

	case bs_ready:
		if (startRequest != NULL && sending == true) {
			if (m_nextRequests.remove(startRequest) == false) {
				// request already removed (e.g. due to timeout)
				return setState(bs_skip, RESULT_ERR_TIMEOUT);
			}
			m_currentRequest = startRequest;
			// check arbitration
			if (recvSymbol == sendSymbol) { // arbitration successful
				m_nextSendPos = 1;
				m_repeat = false;
				return setState(bs_sendCmd, RESULT_OK);
			}
			// arbitration lost. if same priority class found, try again after next AUTO-SYN
			m_remainLockCount = isMaster(recvSymbol) ? 2 : 1; // number of SYN to wait for before next send try
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

			if (m_currentRequest != NULL) {
				if (isMaster(m_currentRequest->m_master[1]) == true) {
					return setState(bs_sendSyn, RESULT_OK);
				}
			}
			else if (isMaster(m_command[1]) == true) {
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
				if (m_currentRequest != NULL)
					return setState(bs_sendCmd, RESULT_ERR_NAK, true);

				return setState(bs_recvCmd, RESULT_ERR_NAK);
			}
			if (m_currentRequest != NULL)
				return setState(bs_skip, RESULT_ERR_NAK);

			return setState(bs_skip, RESULT_ERR_NAK);
		}
		if (m_currentRequest != NULL)
			return setState(bs_skip, RESULT_ERR_ACK);

		return setState(bs_skip, RESULT_ERR_ACK);

	case bs_recvRes:
		headerLen = 0;
		crcPos = m_response.size() > headerLen ? headerLen + 1 + m_response[headerLen] : 0xff;
		result = m_response.push_back(recvSymbol, true, m_response.size() < crcPos);
		if (result < RESULT_OK) {
			if (m_currentRequest != NULL)
				return setState(bs_skip, result);

			return setState(bs_skip, result);
		}
		if (result == RESULT_OK && crcPos != 0xff && m_response.size() == crcPos + 1) { // CRC received
			m_responseCrcValid = m_response[headerLen + 1 + m_response[headerLen]] == m_response.getCRC();
			if (m_responseCrcValid) {
				if (m_currentRequest != NULL)
					return setState(bs_sendResAck, RESULT_OK);

				return setState(bs_recvResAck, RESULT_OK);
			}
			if (m_repeat == true) {
				if (m_currentRequest != NULL)
					return setState(bs_sendSyn, RESULT_ERR_CRC);

				return setState(bs_skip, RESULT_ERR_CRC);
			}
			if (m_currentRequest != NULL)
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
		if (m_currentRequest != NULL && sending == true) {
			if (recvSymbol == sendSymbol) {
				// successfully sent
				m_nextSendPos++;
				if (m_nextSendPos >= m_currentRequest->m_master.size()) {
					// master data completely sent
					if (m_currentRequest->m_master[1] == BROADCAST)
						return setState(bs_sendSyn, RESULT_OK);

					m_commandCrcValid = true;
					return setState(bs_recvCmdAck, RESULT_OK);
				}
				return RESULT_OK;
			}
		}
		return setState(bs_skip, RESULT_ERR_INVALID_ARG);

	case bs_sendResAck:
		if (m_currentRequest != NULL && sending == true) {
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
				if (message == NULL || message->isPassive() == false || message->isWrite() == true)
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
	if (m_currentRequest != NULL) {
		if (result == RESULT_ERR_BUS_LOST && m_currentRequest->m_busLostRetries < m_busLostRetries) {
			logError(lf_bus, "%s, retry", getResultCode(result));
			m_currentRequest->m_busLostRetries++;
			m_nextRequests.add(m_currentRequest); // repeat
			m_currentRequest = NULL;
		}
		else if (state == bs_sendSyn || (result != RESULT_OK && firstRepetition == false)) {
			logDebug(lf_bus, "notify request: %s", getResultCode(result));
			unsigned char dstAddress = m_currentRequest->m_master[1];
			if (result == RESULT_OK && isValidAddress(dstAddress, false) == true)
				m_seenAddresses[dstAddress] = true;
			bool restart = m_currentRequest->notify(result, m_response);
			if (restart == true) {
				m_currentRequest->m_busLostRetries = 0;
				m_nextRequests.add(m_currentRequest);
			}
			else if (m_currentRequest->m_deleteOnFinish == true)
				delete m_currentRequest;
			else
				m_finishedRequests.add(m_currentRequest);

			m_currentRequest = NULL;
		}
	}

	if (state == bs_noSignal) { // notify all requests
		m_response.clear();
		while ((m_currentRequest = m_nextRequests.remove(false)) != NULL) {
			bool restart = m_currentRequest->notify(RESULT_ERR_NO_SIGNAL, m_response);
			if (restart == true) { // should not occur with no signal
				m_currentRequest->m_busLostRetries = 0;
				m_nextRequests.add(m_currentRequest);
			}
			else if (m_currentRequest->m_deleteOnFinish == true)
				delete m_currentRequest;
			else
				m_finishedRequests.add(m_currentRequest);
		}
	}

	if (state == m_state)
		return result;

	if (result < RESULT_OK || (result != RESULT_OK && state == bs_skip))
		logDebug(lf_bus, "%s during %s, switching to %s", getResultCode(result), getStateCode(m_state), getStateCode(state));
	else if (m_currentRequest != NULL || state == bs_sendCmd || state == bs_sendResAck || state == bs_sendSyn)
		logDebug(lf_bus, "switching from %s to %s", getStateCode(m_state), getStateCode(state));
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
		logInfo(lf_update, "update BC cmd: %s", m_command.getDataStr().c_str());
	else if (master == true) {
		logInfo(lf_update, "update MM cmd: %s", m_command.getDataStr().c_str());
		m_seenAddresses[dstAddress] = true;
	}
	else {
		logInfo(lf_update, "update MS cmd: %s / %s", m_command.getDataStr().c_str(), m_response.getDataStr().c_str());
		m_seenAddresses[dstAddress] = true;
	}

	Message* message = m_messages->find(m_command);
	if (message != NULL) {
		string clazz = message->getClass();
		string name = message->getName();
		ostringstream output;
		result_t result = message->decode(m_command, m_response, output);
		if (result != RESULT_OK)
			logError(lf_update, "unable to parse %s %s from %s / %s: %s", clazz.c_str(), name.c_str(), m_command.getDataStr().c_str(), m_response.getDataStr().c_str(), getResultCode(result));
		else {
			string data = output.str();
			logNotice(lf_update, "update %s %s: %s", clazz.c_str(), name.c_str(), data.c_str());
		}
	}
	else {
		if (dstAddress == BROADCAST)
			logNotice(lf_update, "unknown BC cmd: %s", m_command.getDataStr().c_str());
		else if (master == true)
			logNotice(lf_update, "unknown MM cmd: %s", m_command.getDataStr().c_str());
		else
			logNotice(lf_update, "unknown MS cmd: %s / %s", m_command.getDataStr().c_str(), m_response.getDataStr().c_str());
	}
}

result_t BusHandler::startScan(bool full)
{
	Message* scanMessage = m_scanMessage;
	deque<Message*> messages = m_messages->findAll("scan", "", -1);
	for (deque<Message*>::iterator it = messages.begin(); it < messages.end();) {
		Message* message = *it++;
		if (message->getId()[0] == 0x07 && message->getId()[1] == 0x04) {
			if (scanMessage == NULL)
				scanMessage = message;
			messages.erase(it - 1); // query pb 0x07 / sb 0x04 only once
		}
	}
	if (scanMessage == NULL) {
		DataFieldSet* identFields = DataFieldSet::createIdentFields();
		scanMessage = m_scanMessage = new Message(false, false, 0x07, 0x04, identFields);
	}
	if (scanMessage == NULL)
		return RESULT_ERR_NOTFOUND;

	m_scanResults.clear();

	for (unsigned int slave=0; slave<=255; slave++) {
		if (isValidAddress(slave, false) == false || isMaster(slave) == true)
			continue;
		if (full == false && m_seenAddresses[slave] == false) {
			unsigned int master = slave+(256-5); // check if we saw the corresponding master already
			if (isMaster(master) == false || m_seenAddresses[slave] == false)
				continue;
		}

		ScanRequest* request = new ScanRequest(scanMessage, messages, &m_scanResults);
		result_t result = request->prepare(m_ownMasterAddress, slave);
		if (result != RESULT_OK) {
			delete request;
			return result;
		}
		m_nextRequests.add(request);
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

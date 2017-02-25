/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ebusd/bushandler.h"
#include <iomanip>
#include "lib/utils/log.h"

namespace ebusd {

using std::dec;
using std::hex;
using std::setfill;
using std::setw;


// the string used for answering to a scan request (07h 04h)
#define SCAN_ANSWER ("ebusd.eu;" PACKAGE_NAME ";" SCAN_VERSION ";100")

/**
 * Return the string corresponding to the @a BusState.
 * @param state the @a BusState.
 * @return the string corresponding to the @a BusState.
 */
const char* getStateCode(BusState state) {
  switch (state) {
  case bs_noSignal:   return "no signal";
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

result_t PollRequest::prepare(unsigned char ownMasterAddress) {
  istringstream input;
  result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input, UI_FIELD_SEPARATOR, SYN, m_index);
  if (result == RESULT_OK) {
    logInfo(lf_bus, "poll cmd: %s", m_master.getDataStr().c_str());
  }
  return result;
}

bool PollRequest::notify(result_t result, SymbolString& slave) {
  if (result == RESULT_OK) {
    result = m_message->storeLastData(pt_slaveData, slave, m_index);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
  }
  ostringstream output;
  if (result == RESULT_OK) {
    result = m_message->decodeLastData(output);  // decode data
  }
  if (result < RESULT_OK) {
    logError(lf_bus, "poll %s %s failed: %s", m_message->getCircuit().c_str(), m_message->getName().c_str(),
        getResultCode(result));
  } else {
    logNotice(lf_bus, "poll %s %s: %s", m_message->getCircuit().c_str(), m_message->getName().c_str(),
        output.str().c_str());
  }
  return false;
}


result_t ScanRequest::prepare(unsigned char ownMasterAddress) {
  if (m_slaves.empty()) {
    return RESULT_ERR_EOF;
  }
  unsigned char dstAddress = m_slaves.front();
  if (m_index == 0 && m_messages.size() == m_allMessages.size()) {  // first message for this address
    m_busHandler->setScanResult(dstAddress, "");
  }
  istringstream input;
  result_t result = m_message->prepareMaster(ownMasterAddress, m_master, input, UI_FIELD_SEPARATOR, dstAddress,
      m_index);
  if (result >= RESULT_OK) {
    logInfo(lf_bus, "scan %2.2x cmd: %s", dstAddress, m_master.getDataStr().c_str());
  }
  return result;
}

bool ScanRequest::notify(result_t result, SymbolString& slave) {
  unsigned char dstAddress = m_master[1];
  if (result == RESULT_OK) {
    if (m_message == m_messageMap->getScanMessage()) {
      Message* message = m_messageMap->getScanMessage(dstAddress);
      if (message != NULL) {
        m_message = message;
        m_message->storeLastData(pt_masterData, m_master, m_index);  // expected to work since this is a clone
      }
    } else if (m_message->getDstAddress() == SYN) {
      m_message = m_message->derive(dstAddress, true);
      m_messageMap->add(m_message);
      m_message->storeLastData(pt_masterData, m_master, m_index);  // expected to work since this is a clone
    }
    result = m_message->storeLastData(pt_slaveData, slave, m_index);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
    if (result == RESULT_OK) {
      result = m_message->decodeLastData(m_scanResult, 0, true);  // decode data
    }
  }
  if (result < RESULT_OK) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    if (result == RESULT_ERR_TIMEOUT) {
      logNotice(lf_bus, "scan %2.2x timed out (%d slaves left)", dstAddress, m_slaves.size());
    } else {
      logError(lf_bus, "scan %2.2x failed (%d slaves left): %s", dstAddress, m_slaves.size(), getResultCode(result));
    }
    m_messages.clear();  // skip remaining secondary messages
  } else if (m_messages.empty()) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    logNotice(lf_bus, "scan %2.2x completed (%d slaves left)", dstAddress, m_slaves.size());
  }
  if (m_messages.empty()) {  // last message for this address
    m_busHandler->setScanResult(dstAddress, m_scanResult.str());
  }
  if (m_slaves.empty()) {
    logNotice(lf_bus, "scan finished");
    m_busHandler->setScanFinished();
    return false;
  }
  if (m_messages.empty()) {
    m_messages = m_allMessages;
    m_scanResult.str("");
    m_scanResult.clear();
  }
  m_index = 0;
  m_message = m_messages.front();
  m_messages.pop_front();
  if (prepare(m_master[0]) < RESULT_OK) {
    m_busHandler->setScanFinished();
    return false;  // give up
  }
  return true;
}


bool ActiveBusRequest::notify(result_t result, SymbolString& slave) {
  if (result == RESULT_OK) {
    logDebug(lf_bus, "read res: %s", slave.getDataStr().c_str());
  }
  m_result = result;
  m_slave.addAll(slave, true);
  return false;
}

void GrabbedMessage::setLastData(SymbolString& master, SymbolString& slave) {
  m_lastMaster.clear(false);
  m_lastMaster.addAll(master);
  m_lastSlave.clear(false);
  m_lastSlave.addAll(slave);
  m_count++;
}

/**
 * Decode the input @a SymbolString with the specified @a DataType and length.
 * @param type the @a DataType.
 * @param input the unescaped @a SymbolString to read the binary value from.
 * @param isMaster whether the @a SymbolString is the master part.
 * @param baseOffset the base offset in the @a SymbolString.
 * @param length the number of symbols to read.
 * @param offsets the last offset to the baseOffset to read.
 * @param output the ostringstream to append the formatted value to.
 * @param firstOnly whether to read only the first non-erroneous offset.
 * @return @a RESULT_OK on success, or an error code.
 */
bool decodeType(DataType* type, SymbolString *input, bool isMaster, unsigned char baseOffset, unsigned char length,
    unsigned char offsets, ostringstream& output, bool firstOnly = false) {
  bool first = true;
  string in = input->getDataStr(true, true, baseOffset);
  for (unsigned char offset = 0; offset <= offsets; offset++) {
    ostringstream out;
    result_t result = type->readSymbols(*input, isMaster, (unsigned char)(baseOffset+offset), (unsigned char)length,
        out, 0);
    if (result != RESULT_OK) {
      continue;
    }
    if (type->isNumeric() && type->hasFlag(DAY)) {
      unsigned int value = 0;
      if (type->readRawValue(*input, (unsigned char)(baseOffset+offset), (unsigned char)length, value) == RESULT_OK) {
        out.str("");
        out << DataField::getDayName(reinterpret_cast<NumberDataType*>(type)->getMinValue()+value);
      }
    }
    if (first) {
      first = false;
      output << endl << " ";
      ostringstream::pos_type cnt = output.tellp();
      type->dump(output, length, false);
      cnt = output.tellp() - cnt;
      while (cnt < 5) {
        output << " ";
        cnt += 1;
      }
    } else {
      output << ",";
    }
    output << " " << in.substr(offset*2, length*2);
    if (type->isNumeric()) {
      output << "=" << out.str();
    } else {
      output << "=\"" << out.str() << "\"";
    }
    if (firstOnly) {
      return true;  // only the first offset with maximum length when adjustable maximum size is at least 8 bytes
    }
  }
  return !first;
}

bool GrabbedMessage::dump(const bool unknown, MessageMap* messages, bool first, ostringstream& output,
    const bool decode) {
  Message* message = messages->find(m_lastMaster);
  if (unknown && message) {
    return false;
  }
  if (!first) {
    output << endl;
  }
  unsigned char dstAddress = m_lastMaster[1];
  output << m_lastMaster.getDataStr();
  if (dstAddress != BROADCAST && !isMaster(dstAddress)) {
    output << " / " << m_lastSlave.getDataStr();
  }
  output << " = " << static_cast<unsigned>(m_count);
  if (message) {
    output << ": " << message->getCircuit() << " " << message->getName();
  }
  if (decode) {
    DataTypeList *types = DataTypeList::getInstance();
    if (!types) {
      return true;
    }
    bool master = isMaster(dstAddress) || dstAddress == BROADCAST || m_lastSlave.size() <= 1 || m_lastSlave[0] == 0;
    SymbolString *input = master ? &m_lastMaster : &m_lastSlave;
    unsigned char baseOffset = master ? 5 : 1;
    unsigned char remain = input->size();
    if (remain <= baseOffset) {
      return true;
    }
    remain = (unsigned char)(remain-baseOffset);
    if ((*input)[baseOffset-1] < remain) {
      remain = (*input)[baseOffset-1];
    }
    if (remain == 0) {
      return true;
    }
    for (map<string, DataType*>::const_iterator it = types->begin(); it != types->end(); it++) {
      DataType* baseType = it->second;
      if ((baseType->getBitCount() % 8) != 0 || baseType->isIgnored()) {  // skip bit and ignored types
        continue;
      }
      unsigned char maxLength = baseType->getBitCount()/8;
      bool firstOnly = maxLength >= 8;
      if (maxLength > remain) {
        maxLength = remain;
      }
      if (baseType->isAdjustableLength()) {
        for (unsigned char length = maxLength; length >= 1; length--) {
          DataType* type = types->get(baseType->getId(), length);
          if (decodeType(type, input, master, baseOffset, length, (unsigned char)(remain-length), output, firstOnly)) {
            if (firstOnly) {
              break;  // only a single offset with maximum length when adjustable maximum size is at least 8 bytes
            }
          }
        }
      } else if (maxLength > 0) {
        decodeType(baseType, input, master, baseOffset, maxLength, (unsigned char)(remain-maxLength), output);
      }
    }
  }
  return true;
}


void BusHandler::clear() {
  memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
  m_masterCount = 1;
  m_scanResults.clear();
}

result_t BusHandler::sendAndWait(SymbolString& master, SymbolString& slave) {
  result_t result = RESULT_ERR_NO_SIGNAL;
  slave.clear();
  ActiveBusRequest request(master, slave);
  logInfo(lf_bus, "send message: %s", master.getDataStr().c_str());

  for (int sendRetries = m_failedSendRetries + 1; sendRetries >= 0; sendRetries--) {
    m_nextRequests.push(&request);
    bool success = m_finishedRequests.remove(&request, true);
    result = success ? request.m_result : RESULT_ERR_TIMEOUT;
    if (result == RESULT_OK) {
      Message* message = m_messages->find(master);
      if (message != NULL) {
        m_messages->invalidateCache(message);
      }
      break;
    }
    if (!success || result == RESULT_ERR_NO_SIGNAL || result == RESULT_ERR_SEND || result == RESULT_ERR_DEVICE) {
      logError(lf_bus, "send to %2.2x: %s, give up", master[1], getResultCode(result));
      break;
    }
    logError(lf_bus, "send to %2.2x: %s%s", master[1], getResultCode(result), sendRetries > 0 ? ", retry" : "");
    request.m_busLostRetries = 0;
  }
  return result;
}

result_t BusHandler::readFromBus(Message* message, string inputStr, const unsigned char dstAddress,
    const unsigned char srcAddress) {
  unsigned char masterAddress = srcAddress == SYN ? m_ownMasterAddress : srcAddress;
  result_t ret = RESULT_EMPTY;
  SymbolString master(true);
  SymbolString slave(false);
  for (unsigned char index = 0; index < message->getCount(); index++) {
    istringstream input(inputStr);
    ret = message->prepareMaster(masterAddress, master, input, UI_FIELD_SEPARATOR, dstAddress, index);
    if (ret != RESULT_OK) {
      logError(lf_bus, "prepare message part %d: %s", index, getResultCode(ret));
      break;
    }
    // send message
    ret = sendAndWait(master, slave);
    if (ret != RESULT_OK) {
      logError(lf_bus, "send message part %d: %s", index, getResultCode(ret));
      break;
    }
    ret = message->storeLastData(pt_slaveData, slave, index);
    if (ret < RESULT_OK) {
      logError(lf_bus, "store message part %d: %s", index, getResultCode(ret));
      break;
    }
  }
  return ret;
}

void BusHandler::run() {
  unsigned int symCount = 0;
  time_t lastTime;
  time(&lastTime);
  do {
    if (m_device->isValid() && !m_reconnect) {
      result_t result = handleSymbol();
      if (result != RESULT_ERR_TIMEOUT) {
        symCount++;
      }
      time_t now;
      time(&now);
      if (now > lastTime) {
        m_symPerSec = symCount / (unsigned int)(now-lastTime);
        if (m_symPerSec > m_maxSymPerSec) {
          m_maxSymPerSec = m_symPerSec;
          if (m_maxSymPerSec > 100) {
            logNotice(lf_bus, "max. symbols per second: %d", m_maxSymPerSec);
          }
        }
        lastTime = now;
        symCount = 0;
      }
    } else {
      if (!Wait(10)) {
        break;
      }
      m_reconnect = false;
      result_t result = m_device->open();
      if (result == RESULT_OK) {
        logNotice(lf_bus, "re-opened %s", m_device->getName());
      } else {
        logError(lf_bus, "unable to open %s: %s", m_device->getName(), getResultCode(result));
        setState(bs_noSignal, result);
      }
      symCount = 0;
    }
  } while (isRunning());
}

result_t BusHandler::handleSymbol() {
  unsigned int timeout = SYN_TIMEOUT;
  unsigned char sendSymbol = ESC;
  bool sending = false;
  BusRequest* startRequest = NULL;

  // check if another symbol has to be sent and determine timeout for receive
  switch (m_state) {
  case bs_noSignal:
    timeout = m_generateSynInterval > 0 ? m_generateSynInterval : SIGNAL_TIMEOUT;
    break;

  case bs_skip:
    timeout = SYN_TIMEOUT;
    break;

  case bs_ready:
    if (m_currentRequest != NULL) {
      setState(bs_ready, RESULT_ERR_TIMEOUT);  // just to be sure an old BusRequest is cleaned up
    } else if (m_remainLockCount == 0) {
      startRequest = m_nextRequests.peek();
      if (startRequest == NULL && m_pollInterval > 0) {  // check for poll/scan
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
            } else {
              startRequest = request;
              m_nextRequests.push(request);
            }
          }
        }
      }
      if (startRequest != NULL) {  // initiate arbitration
        sendSymbol = startRequest->m_master[0];
        sending = true;
      }
    }
    break;

  case bs_recvCmd:
  case bs_recvCmdAck:
    timeout = m_slaveRecvTimeout;
    break;

  case bs_recvRes:
    if (m_response.size() > 0 || m_slaveRecvTimeout > SYN_TIMEOUT) {
      timeout = m_slaveRecvTimeout;
    } else {
      timeout = SYN_TIMEOUT;
    }
    break;

  case bs_recvResAck:
    timeout = m_slaveRecvTimeout+m_transferLatency;
    break;

  case bs_sendCmd:
    if (m_currentRequest != NULL) {
      sendSymbol = m_currentRequest->m_master[m_nextSendPos];  // escaped command
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
    if (m_answer) {
      sendSymbol = m_commandCrcValid ? ACK : NAK;
      sending = true;
    }
    break;

  case bs_sendRes:
    if (m_answer) {
      sendSymbol = m_response[m_nextSendPos];  // escaped response
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
  if (sending) {
    result = m_device->send(sendSymbol);
    if (result == RESULT_OK) {
      if (m_state == bs_ready) {
        timeout = m_transferLatency+m_busAcquireTimeout;
      } else {
        timeout = m_transferLatency+SEND_TIMEOUT;
      }
    } else {
      sending = false;
      timeout = SYN_TIMEOUT;
      if (startRequest != NULL && m_nextRequests.remove(startRequest)) {
        m_currentRequest = startRequest;  // force the failed request to be notified
      }
      setState(bs_skip, result);
    }
  }

  // receive next symbol (optionally check reception of sent symbol)
  unsigned char recvSymbol;
  result = m_device->recv(timeout+m_transferLatency, recvSymbol);

  if (!sending && result == RESULT_ERR_TIMEOUT && m_generateSynInterval > 0
  && timeout >= m_generateSynInterval && (m_state == bs_noSignal || m_state == bs_skip)) {
    // check if acting as AUTO-SYN generator is required
    result = m_device->send(SYN);
    if (result == RESULT_OK) {
      recvSymbol = ESC;
      result = m_device->recv(SEND_TIMEOUT, recvSymbol);
      if (result == RESULT_ERR_TIMEOUT) {
        return setState(bs_noSignal, result);
      }
      if (result != RESULT_OK) {
        logError(lf_bus, "unable to receive sent AUTO-SYN symbol: %s", getResultCode(result));
      } else if (recvSymbol != SYN) {
        logError(lf_bus, "received %2.2x instead of AUTO-SYN symbol", recvSymbol);
      } else {
        if (m_generateSynInterval != SYN_TIMEOUT) {
          // received own AUTO-SYN symbol back again: act as AUTO-SYN generator now
          m_generateSynInterval = SYN_TIMEOUT;
          logNotice(lf_bus, "acting as AUTO-SYN generator");
        }
        m_remainLockCount = 0;
        return setState(bs_ready, result);
      }
    }
    return setState(bs_skip, result);
  }
  time_t now;
  time(&now);
  if (result != RESULT_OK) {
    if (sending && startRequest != NULL && m_nextRequests.remove(startRequest)) {
      m_currentRequest = startRequest;  // force the failed request to be notified
    }
    if ((m_generateSynInterval != SYN_TIMEOUT && difftime(now, m_lastReceive) > 1)
      // at least one full second has passed since last received symbol
      || m_state == bs_noSignal) {
      return setState(bs_noSignal, result);
    }
    return setState(bs_skip, result);
  }

  m_lastReceive = now;
  if ((recvSymbol == SYN) && (m_state != bs_sendSyn)) {
    if (!sending && m_remainLockCount > 0 && m_command.size() != 1) {
      m_remainLockCount--;
    } else if (!sending && m_remainLockCount == 0 && m_command.size() == 1) {
      m_remainLockCount = 1;  // wait for next AUTO-SYN after SYN / address / SYN (bus locked for own priority)
    }
    return setState(bs_ready, m_state == bs_skip ? RESULT_OK : RESULT_ERR_SYN);
  }

  unsigned int headerLen, crcPos;

  switch (m_state) {
  case bs_noSignal:
    return setState(bs_skip, RESULT_OK);

  case bs_skip:
    return RESULT_OK;

  case bs_ready:
    if (startRequest != NULL && sending) {
      if (!m_nextRequests.remove(startRequest)) {
        // request already removed (e.g. due to timeout)
        return setState(bs_skip, RESULT_ERR_TIMEOUT);
      }
      m_currentRequest = startRequest;
      // check arbitration
      if (recvSymbol == sendSymbol) {  // arbitration successful
        m_nextSendPos = 1;
        m_repeat = false;
        return setState(bs_sendCmd, RESULT_OK);
      }
      // arbitration lost. if same priority class found, try again after next AUTO-SYN
      m_remainLockCount = isMaster(recvSymbol) ? 2 : 1;  // number of SYN to wait for before next send try
      if ((recvSymbol & 0x0f) != (sendSymbol & 0x0f) && m_lockCount > m_remainLockCount) {
        // if different priority class found, try again after N AUTO-SYN symbols (at least next AUTO-SYN)
        m_remainLockCount = m_lockCount;
      }
      setState(m_state, RESULT_ERR_BUS_LOST);  // try again later
    }
    result = m_command.push_back(recvSymbol, false);  // expect no escaping for master address
    if (result < RESULT_OK) {
      return setState(bs_skip, result);
    }
    m_repeat = false;
    return setState(bs_recvCmd, RESULT_OK);

  case bs_recvCmd:
    headerLen = 4;
    // header symbols are never escaped
    crcPos = m_command.size() > headerLen ? headerLen + 1 + m_command[headerLen] : 0xff;
    result = m_command.push_back(recvSymbol, true, m_command.size() < crcPos);
    if (result < RESULT_OK) {
      return setState(bs_skip, result);
    }
    if (result == RESULT_OK && crcPos != 0xff && m_command.size() == crcPos + 1) {  // CRC received
      unsigned char dstAddress = m_command[1];
      // header symbols are never escaped
      m_commandCrcValid = m_command[headerLen + 1 + m_command[headerLen]] == m_command.getCRC();
      if (m_commandCrcValid) {
        if (dstAddress == BROADCAST) {
          receiveCompleted();
          return setState(bs_skip, RESULT_OK);
        }
        addSeenAddress(m_command[0]);
        if (m_answer && (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress)) {
          return setState(bs_sendCmdAck, RESULT_OK);
        }
        return setState(bs_recvCmdAck, RESULT_OK);
      }
      if (dstAddress == BROADCAST) {
        return setState(bs_skip, RESULT_ERR_CRC);
      }
      if (m_answer && (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress)) {
        return setState(bs_sendCmdAck, RESULT_ERR_CRC);
      }
      if (m_repeat) {
        return setState(bs_skip, RESULT_ERR_CRC);
      }
      return setState(bs_recvCmdAck, RESULT_ERR_CRC);
    }
    return RESULT_OK;

  case bs_recvCmdAck:
    if (recvSymbol == ACK) {
      if (!m_commandCrcValid) {
        return setState(bs_skip, RESULT_ERR_ACK);
      }
      if (m_currentRequest != NULL) {
        if (isMaster(m_currentRequest->m_master[1])) {
          return setState(bs_sendSyn, RESULT_OK);
        }
      } else if (isMaster(m_command[1])) {  // header symbols are never escaped
        receiveCompleted();
        return setState(bs_skip, RESULT_OK);
      }

      m_repeat = false;
      return setState(bs_recvRes, RESULT_OK);
    }
    if (recvSymbol == NAK) {
      if (!m_repeat) {
        m_repeat = true;
        m_nextSendPos = 0;
        m_command.clear();
        if (m_currentRequest != NULL) {
          return setState(bs_sendCmd, RESULT_ERR_NAK, true);
        }
        return setState(bs_recvCmd, RESULT_ERR_NAK);
      }
      return setState(bs_skip, RESULT_ERR_NAK);
    }
    return setState(bs_skip, RESULT_ERR_ACK);

  case bs_recvRes:
    headerLen = 0;
    crcPos = m_response.size() > headerLen ? headerLen + 1 + m_response[headerLen] : 0xff;
    result = m_response.push_back(recvSymbol, true, m_response.size() < crcPos);
    if (result < RESULT_OK) {
      return setState(bs_skip, result);
    }
    if (result == RESULT_OK && crcPos != 0xff && m_response.size() == crcPos + 1) {  // CRC received
      m_responseCrcValid = m_response[headerLen + 1 + m_response[headerLen]] == m_response.getCRC();
      if (m_responseCrcValid) {
        if (m_currentRequest != NULL) {
          return setState(bs_sendResAck, RESULT_OK);
        }
        return setState(bs_recvResAck, RESULT_OK);
      }
      if (m_repeat) {
        if (m_currentRequest != NULL) {
          return setState(bs_sendSyn, RESULT_ERR_CRC);
        }
        return setState(bs_skip, RESULT_ERR_CRC);
      }
      if (m_currentRequest != NULL) {
        return setState(bs_sendResAck, RESULT_ERR_CRC);
      }
      return setState(bs_recvResAck, RESULT_ERR_CRC);
    }
    return RESULT_OK;

  case bs_recvResAck:
    if (recvSymbol == ACK) {
      if (!m_responseCrcValid) {
        return setState(bs_skip, RESULT_ERR_ACK);
      }
      receiveCompleted();
      return setState(bs_skip, RESULT_OK);
    }
    if (recvSymbol == NAK) {
      if (!m_repeat) {
        m_repeat = true;
        m_response.clear();
        return setState(bs_recvRes, RESULT_ERR_NAK, true);
      }
      return setState(bs_skip, RESULT_ERR_NAK);
    }
    return setState(bs_skip, RESULT_ERR_ACK);

  case bs_sendCmd:
    if (m_currentRequest != NULL && sending && recvSymbol == sendSymbol) {
      // successfully sent
      m_nextSendPos++;
      if (m_nextSendPos >= m_currentRequest->m_master.size()) {
        // master data completely sent
        if (m_currentRequest->m_master[1] == BROADCAST) {
          return setState(bs_sendSyn, RESULT_OK);
        }
        m_commandCrcValid = true;
        return setState(bs_recvCmdAck, RESULT_OK);
      }
      return RESULT_OK;
    }
    return setState(bs_skip, RESULT_ERR_INVALID_ARG);

  case bs_sendResAck:
    if (m_currentRequest != NULL && sending && recvSymbol == sendSymbol) {
      // successfully sent
      if (!m_responseCrcValid) {
        if (!m_repeat) {
          m_repeat = true;
          m_response.clear();
          return setState(bs_recvRes, RESULT_ERR_NAK, true);
        }
        return setState(bs_sendSyn, RESULT_ERR_ACK);
      }
      return setState(bs_sendSyn, RESULT_OK);
    }
    return setState(bs_skip, RESULT_ERR_INVALID_ARG);

  case bs_sendCmdAck:
    if (sending && m_answer && recvSymbol == sendSymbol) {
      // successfully sent
      if (!m_commandCrcValid) {
        if (!m_repeat) {
          m_repeat = true;
          m_command.clear();
          return setState(bs_recvCmd, RESULT_ERR_NAK, true);
        }
        return setState(bs_skip, RESULT_ERR_ACK);
      }
      if (isMaster(m_command[1])) {
        receiveCompleted();  // decode command and store value
        return setState(bs_skip, RESULT_OK);
      }

      m_nextSendPos = 0;
      m_repeat = false;
      Message* message;
      istringstream input;  // TODO create input from database of internal variables
      message = m_messages->find(m_command);
      if (message == NULL) {
        message = m_messages->find(m_command, true);
        if (message != NULL && message->getSrcAddress() != SYN) {
          message = NULL;
        }
      }
      if (message == NULL || message->isWrite()) {
        // don't know this request or definition has wrong direction, deny
        return setState(bs_skip, RESULT_ERR_INVALID_ARG);
      }
      if (message == m_messages->getScanMessage(m_ownSlaveAddress)) {
        input.str(SCAN_ANSWER);
      }
      // build response and store in m_response for sending back to requesting master
      m_response.clear(true);  // escape while sending response
      result = message->prepareSlave(input, m_response);
      if (result != RESULT_OK) {
        return setState(bs_skip, result);
      }
      return setState(bs_sendRes, RESULT_OK);
    }
    return setState(bs_skip, RESULT_ERR_INVALID_ARG);

  case bs_sendRes:
    if (sending && m_answer && recvSymbol == sendSymbol) {
      // successfully sent
      m_nextSendPos++;
      if (m_nextSendPos >= m_response.size()) {
        // slave data completely sent
        return setState(bs_recvResAck, RESULT_OK);
      }
      return RESULT_OK;
    }
    return setState(bs_skip, RESULT_ERR_INVALID_ARG);

  case bs_sendSyn:
    if (sending && recvSymbol == sendSymbol) {
      // successfully sent
      return setState(bs_skip, RESULT_OK);
    }
    return setState(bs_skip, RESULT_ERR_INVALID_ARG);
  }
  return RESULT_OK;
}

result_t BusHandler::setState(BusState state, result_t result, bool firstRepetition) {
  if (m_currentRequest != NULL) {
    if (result == RESULT_ERR_BUS_LOST && m_currentRequest->m_busLostRetries < m_busLostRetries) {
      logDebug(lf_bus, "%s during %s, retry", getResultCode(result), getStateCode(m_state));
      m_currentRequest->m_busLostRetries++;
      m_nextRequests.push(m_currentRequest);  // repeat
      m_currentRequest = NULL;
    } else if (state == bs_sendSyn || (result != RESULT_OK && !firstRepetition)) {
      logDebug(lf_bus, "notify request: %s", getResultCode(result));
      unsigned char dstAddress = m_currentRequest->m_master[1];
      if (result == RESULT_OK) {
        addSeenAddress(dstAddress);
      }
      bool restart = m_currentRequest->notify(
        result == RESULT_ERR_SYN && (m_state == bs_recvCmdAck || m_state == bs_recvRes)
        ? RESULT_ERR_TIMEOUT : result, m_response);
      if (restart) {
        m_currentRequest->m_busLostRetries = 0;
        m_nextRequests.push(m_currentRequest);
      } else if (m_currentRequest->m_deleteOnFinish) {
        delete m_currentRequest;
      } else {
        m_finishedRequests.push(m_currentRequest);
      }
      m_currentRequest = NULL;
    }
  }

  if (state == bs_noSignal) {  // notify all requests
    m_response.clear(false);  // notify with empty response
    while ((m_currentRequest = m_nextRequests.pop()) != NULL) {
      bool restart = m_currentRequest->notify(RESULT_ERR_NO_SIGNAL, m_response);
      if (restart) {  // should not occur with no signal
        m_currentRequest->m_busLostRetries = 0;
        m_nextRequests.push(m_currentRequest);
      } else if (m_currentRequest->m_deleteOnFinish) {
        delete m_currentRequest;
      } else {
        m_finishedRequests.push(m_currentRequest);
      }
    }
  }

  if (state == m_state) {
    return result;
  }
  if (result < RESULT_OK || (result != RESULT_OK && state == bs_skip)) {
    logDebug(lf_bus, "%s during %s, switching to %s", getResultCode(result), getStateCode(m_state),
        getStateCode(state));
  } else if (m_currentRequest != NULL || state == bs_sendCmd || state == bs_sendResAck || state == bs_sendSyn) {
    logDebug(lf_bus, "switching from %s to %s", getStateCode(m_state), getStateCode(state));
  }
  if (state == bs_noSignal) {
    logError(lf_bus, "signal lost");
  } else if (m_state == bs_noSignal) {
    logNotice(lf_bus, "signal acquired");
  }
  m_state = state;

  if (state == bs_ready || state == bs_skip) {
    m_command.clear();
    m_commandCrcValid = false;
    m_response.clear(false);  // unescape while receiving response
    m_responseCrcValid = false;
    m_nextSendPos = 0;
  }

  return result;
}

void BusHandler::addSeenAddress(unsigned char address) {
  if (!isValidAddress(address, false)) {
    return;
  }
  if (!isMaster(address)) {
    if (!m_device->isReadOnly() && address == m_ownSlaveAddress) {
      if (!m_addressConflict) {
        m_addressConflict = true;
        logError(lf_bus, "own slave address %2.2x is used by another participant", address);
      }
    }
    m_seenAddresses[address] |= SEEN;
    address = getMasterAddress(address);
    if (address == SYN) {
      return;
    }
  }
  if ((m_seenAddresses[address]&SEEN) == 0) {
    if (!m_device->isReadOnly() && address == m_ownMasterAddress) {
      if (!m_addressConflict) {
        m_addressConflict = true;
        logError(lf_bus, "own master address %2.2x is used by another participant", address);
      }
    } else {
      m_masterCount++;
      if (m_autoLockCount && m_masterCount > m_lockCount) {
        m_lockCount = m_masterCount;
      }
      logNotice(lf_bus, "new master %2.2x, master count %d", address, m_masterCount);
    }
    m_seenAddresses[address] |= SEEN;
  }
}

void BusHandler::receiveCompleted() {
  unsigned char srcAddress = m_command[0], dstAddress = m_command[1];
  if (srcAddress == dstAddress) {
    logError(lf_bus, "invalid self-addressed message from %2.2x", srcAddress);
    return;
  }
  addSeenAddress(srcAddress);
  addSeenAddress(dstAddress);

  bool master = isMaster(dstAddress);
  if (dstAddress == BROADCAST) {
    logInfo(lf_update, "update BC cmd: %s", m_command.getDataStr().c_str());
    if (m_command.size() >= 5+9 && m_command[2] == 0x07 && m_command[3] == 0x04) {
      unsigned char slaveAddress = (unsigned char)((srcAddress+5)&0xff);
      addSeenAddress(slaveAddress);
      Message* message = m_messages->getScanMessage(slaveAddress);
      if (message && (message->getLastUpdateTime() == 0 || message->getLastSlaveData().size() < 10)) {
        // e.g. 10fe07040a b5564149303001248901
        m_seenAddresses[slaveAddress] |= SCAN_INIT;
        SymbolString idData;
        istringstream input;
        result_t result = message->prepareMaster(m_ownMasterAddress, idData, input);
        if (result == RESULT_OK) {
          idData.clear();
          idData.push_back(9);
          for (size_t i = 5; i <= 5+9; i++) {
            idData.push_back(m_command[i]);
          }
          result = message->storeLastData(pt_slaveData, idData, 0);
        }
        if (result == RESULT_OK) {
          m_seenAddresses[slaveAddress] |= SCAN_DONE;
        }
        logNotice(lf_update, "store BC ident: %s", getResultCode(result));
      }
    }
  } else if (master) {
    logInfo(lf_update, "update MM cmd: %s", m_command.getDataStr().c_str());
  } else {
    logInfo(lf_update, "update MS cmd: %s / %s", m_command.getDataStr().c_str(), m_response.getDataStr().c_str());
  }
  Message* message = m_messages->find(m_command);
  if (m_grabMessages) {
    uint64_t key;
    if (message) {
      key = message->getKey();
    } else {
      key = Message::createKey(m_command, 4);  // up to 4 DD bytes
    }
    m_grabbedMessages[key].setLastData(m_command, m_response);
  }
  if (message == NULL) {
    if (dstAddress == BROADCAST) {
      logNotice(lf_update, "unknown BC cmd: %s", m_command.getDataStr().c_str());
    } else if (master) {
      logNotice(lf_update, "unknown MM cmd: %s", m_command.getDataStr().c_str());
    } else {
      logNotice(lf_update, "unknown MS cmd: %s / %s", m_command.getDataStr().c_str(),
          m_response.getDataStr().c_str());
    }
  } else {
    m_messages->invalidateCache(message);
    string circuit = message->getCircuit();
    string name = message->getName();
    result_t result = message->storeLastData(m_command, m_response);
    ostringstream output;
    if (result == RESULT_OK) {
      result = message->decodeLastData(output);
    }
    if (result < RESULT_OK) {
      logError(lf_update, "unable to parse %s %s from %s / %s: %s", circuit.c_str(), name.c_str(),
          m_command.getDataStr().c_str(), m_response.getDataStr().c_str(), getResultCode(result));
    } else {
      string data = output.str();
      if (m_answer && dstAddress == (master ? m_ownMasterAddress : m_ownSlaveAddress)) {
        logNotice(lf_update, "self-update %s %s QQ=%2.2x: %s", circuit.c_str(), name.c_str(), srcAddress,
            data.c_str());  // TODO store in database of internal variables
      } else if (message->getDstAddress() == SYN) {  // any destination
        if (message->getSrcAddress() == SYN) {  // any destination and any source
          logNotice(lf_update, "update %s %s QQ=%2.2x ZZ=%2.2x: %s", circuit.c_str(), name.c_str(), srcAddress,
              dstAddress, data.c_str());
        } else {
          logNotice(lf_update, "update %s %s ZZ=%2.2x: %s", circuit.c_str(), name.c_str(), dstAddress, data.c_str());
        }
      } else if (message->getSrcAddress() == SYN) {  // any source
        logNotice(lf_update, "update %s %s QQ=%2.2x: %s", circuit.c_str(), name.c_str(), srcAddress, data.c_str());
      } else {
        logNotice(lf_update, "update %s %s: %s", circuit.c_str(), name.c_str(), data.c_str());
      }
    }
  }
}

result_t BusHandler::startScan(bool full, string levels) {
  if (m_runningScans > 0) {
    return RESULT_ERR_DUPLICATE;
  }
  deque<Message*> messages = m_messages->findAll("scan", "", levels, true);
  for (deque<Message*>::iterator it = messages.begin(); it < messages.end(); it++) {
    Message* message = *it;
    if (message->getPrimaryCommand() == 0x07 && message->getSecondaryCommand() == 0x04) {
      messages.erase(it--);  // query pb 0x07 / sb 0x04 only once
    }
  }

  Message* scanMessage = m_messages->getScanMessage();
  if (scanMessage == NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  m_scanResults.clear();

  deque<unsigned char> slaves;
  for (unsigned char slave = 1; slave != 0; slave++) {  // 0 is known to be a master
    if (!isValidAddress(slave, false) || isMaster(slave)) {
      continue;
    }
    if (!full && (m_seenAddresses[slave]&SEEN) == 0) {
      unsigned char master = getMasterAddress(slave);  // check if we saw the corresponding master already
      if (master == SYN || (m_seenAddresses[master]&SEEN) == 0) {
        continue;
      }
    }
    slaves.push_back(slave);
  }
  messages.push_front(scanMessage);
  ScanRequest* request = new ScanRequest(m_messages, messages, slaves, this);
  result_t result = request->prepare(m_ownMasterAddress);
  if (result < RESULT_OK) {
    delete request;
    return result == RESULT_ERR_EOF ? RESULT_EMPTY : result;
  }
  m_runningScans++;
  m_nextRequests.push(request);
  return RESULT_OK;
}

void BusHandler::setScanResult(unsigned char dstAddress, string str) {
  m_seenAddresses[dstAddress] |= SCAN_INIT;
  if (str.length() > 0) {
    m_seenAddresses[dstAddress] |= SCAN_DONE;
    m_scanResults[dstAddress] = str;
    logNotice(lf_bus, "scan %2.2x: %s", dstAddress, str.c_str());
  }
}

void BusHandler::setScanFinished() {
  if (m_runningScans > 0) {
    m_runningScans--;
  }
}

void BusHandler::formatScanResult(ostringstream& output) {
  if (m_runningScans > 0) {
    output << m_runningScans << " scan(s) still running" << endl;
  }
  bool first = true;
  for (unsigned char slave = 1; slave != 0; slave++) {  // 0 is known to be a master
    map<unsigned char, string>::iterator it = m_scanResults.find(slave);
    if (it != m_scanResults.end()) {
      if (first) {
        first = false;
      } else {
        output << endl;
      }
      output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave) << it->second;
    }
  }
  if (first) {
    // fallback to autoscan results
    for (unsigned char slave = 1; slave != 0; slave++) {  // 0 is known to be a master
      if (isValidAddress(slave, false) && !isMaster(slave) && (m_seenAddresses[slave]&SCAN_DONE) != 0) {
        Message* message = m_messages->getScanMessage(slave);
        if (message != NULL && message->getLastUpdateTime() > 0) {
          if (first) {
            first = false;
          } else {
            output << endl;
          }
          output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave);
          message->decodeLastData(output, 0, true);
        }
      }
    }
  }
}

void BusHandler::formatSeenInfo(ostringstream& output) {
  unsigned char address = 0;
  for (int index = 0; index < 256; index++, address++) {
    if (isValidAddress(address, false) && ((m_seenAddresses[address]&SEEN) != 0
        || (!m_device->isReadOnly() && (address == m_ownMasterAddress || address == m_ownSlaveAddress)))) {
      output << endl << "address " << setfill('0') << setw(2) << hex << static_cast<unsigned>(address);
      unsigned char master;
      if (isMaster(address)) {
        output << ": master";
        master = address;
      } else {
        output << ": slave";
        master = getMasterAddress(address);
      }
      if (master != SYN) {
        output << " #" << setw(0) << dec << static_cast<unsigned>(getMasterNumber(master));
      }
      if (!m_device->isReadOnly() && (address == m_ownMasterAddress || address == m_ownSlaveAddress)) {
        output << ", ebusd";
        if (m_answer) {
          output << " (answering)";
        }
        if (m_addressConflict && (m_seenAddresses[address]&SEEN) != 0) {
          output << ", conflict";
        }
      }
      if ((m_seenAddresses[address]&SCAN_DONE) != 0) {
        output << ", scanned";
        Message* message = m_messages->getScanMessage(address);
        if (message != NULL && message->getLastUpdateTime() > 0) {
          // add detailed scan info: Manufacturer ID SW HW
          output << " \"";
          result_t result = message->decodeLastData(output, OF_NAMES);
          if (result != RESULT_OK) {
            output << "\" error: " << getResultCode(result);
          } else {
            output << "\"";
          }
        }
      }
      string loadedFiles = m_messages->getLoadedFiles(address);
      if (!loadedFiles.empty()) {
        output << ", loaded " << loadedFiles;
      }
    }
  }
}

result_t BusHandler::scanAndWait(unsigned char dstAddress, SymbolString& slave) {
  if (!isValidAddress(dstAddress) || isMaster(dstAddress)) {
    return RESULT_ERR_INVALID_ADDR;
  }
  m_seenAddresses[dstAddress] |= SCAN_INIT;
  Message* scanMessage = m_messages->getScanMessage();
  if (scanMessage == NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  istringstream input;
  SymbolString master;
  result_t result = scanMessage->prepareMaster(m_ownMasterAddress, master, input, UI_FIELD_SEPARATOR, dstAddress);
  if (result == RESULT_OK) {
    result = sendAndWait(master, slave);
    if (result == RESULT_OK) {
      Message* message = m_messages->getScanMessage(dstAddress);
      if (message != NULL && message != scanMessage) {
        scanMessage = message;
        // update the cache, expected to work since this is a clone
        scanMessage->storeLastData(pt_masterData, master, 0);
      }
    }
    if (result != RESULT_ERR_NO_SIGNAL) {
      m_seenAddresses[dstAddress] |= SCAN_DONE;
    }
  }
  if (result != RESULT_OK || slave.size() == 0) {  // avoid "invalid position" during decode
    return result;
  }
  return scanMessage->storeLastData(pt_slaveData, slave, 0);  // update the cache
}

bool BusHandler::enableGrab(bool enable) {
  if (enable == m_grabMessages) {
    return false;
  }
  if (!enable) {
    m_grabbedMessages.clear();
  }
  m_grabMessages = enable;
  return true;
}

void BusHandler::formatGrabResult(const bool unknown, ostringstream& output, const bool decode) {
  if (!m_grabMessages) {
    output << "grab disabled";
  } else {
    bool first = true;
    for (map<uint64_t, GrabbedMessage>::iterator it = m_grabbedMessages.begin(); it != m_grabbedMessages.end();
        it++) {
      if (it->second.dump(unknown, m_messages, first, output, decode)) {
        first = false;
      }
    }
  }
}

unsigned char BusHandler::getNextScanAddress(unsigned char lastAddress, bool& scanned) {
  if (lastAddress == SYN) {
    return SYN;
  }
  while (++lastAddress != 0) {  // 0 is known to be a master
    if (!isValidAddress(lastAddress, false) || isMaster(lastAddress)) {
      continue;
    }
    if ((m_seenAddresses[lastAddress]&(SEEN|LOAD_INIT)) == SEEN) {
      scanned = (m_seenAddresses[lastAddress]&SCAN_INIT) != 0;
      return lastAddress;
    }
    unsigned char master = getMasterAddress(lastAddress);
    if (master != SYN && (m_seenAddresses[master]&SEEN) != 0 && (m_seenAddresses[lastAddress]&LOAD_INIT) == 0) {
      scanned = (m_seenAddresses[lastAddress]&SCAN_INIT) != 0;
      return lastAddress;
    }
  }
  return SYN;
}

void BusHandler::setScanConfigLoaded(unsigned char address, string file) {
  m_seenAddresses[address] |= LOAD_INIT;
  if (!file.empty()) {
    m_seenAddresses[address] |= LOAD_DONE;
    m_messages->addLoadedFile(address, file, "");
  }
}

}  // namespace ebusd

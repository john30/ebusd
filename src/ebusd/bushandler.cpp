/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2021 John Baier <ebusd@ebusd.eu>
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
#include "ebusd/main.h"
#include "lib/utils/log.h"

namespace ebusd {

using std::dec;
using std::hex;
using std::setfill;
using std::setw;
using std::endl;

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
  case bs_recvCmdCrc: return "receive command CRC";
  case bs_recvCmdAck: return "receive command ACK";
  case bs_recvRes:    return "receive response";
  case bs_recvResCrc: return "receive response CRC";
  case bs_sendResAck: return "send response ACK";
  case bs_recvCmd:    return "receive command";
  case bs_recvResAck: return "receive response ACK";
  case bs_sendCmdCrc: return "send command CRC";
  case bs_sendCmdAck: return "send command ACK";
  case bs_sendRes:    return "send response";
  case bs_sendResCrc: return "send response CRC";
  case bs_sendSyn:    return "send SYN";
  default:            return "unknown";
  }
}

result_t PollRequest::prepare(symbol_t ownMasterAddress) {
  istringstream input;
  result_t result = m_message->prepareMaster(m_index, ownMasterAddress, SYN, UI_FIELD_SEPARATOR, &input, &m_master);
  if (result == RESULT_OK) {
    string str = m_master.getStr();
    logInfo(lf_bus, "poll cmd: %s", str.c_str());
  }
  return result;
}

bool PollRequest::notify(result_t result, const SlaveSymbolString& slave) {
  if (result == RESULT_OK) {
    result = m_message->storeLastData(m_index, slave);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
  }
  if (result < RESULT_OK) {
    logError(lf_bus, "poll %s %s failed: %s", m_message->getCircuit().c_str(), m_message->getName().c_str(),
        getResultCode(result));
  }
  return false;
}


result_t ScanRequest::prepare(symbol_t ownMasterAddress) {
  if (m_slaves.empty()) {
    return RESULT_ERR_EOF;
  }
  symbol_t dstAddress = m_slaves.front();
  istringstream input;
  m_result = m_message->prepareMaster(m_index, ownMasterAddress, dstAddress, UI_FIELD_SEPARATOR, &input, &m_master);
  if (m_result >= RESULT_OK) {
    string str = m_master.getStr();
    logInfo(lf_bus, "scan %2.2x cmd: %s", dstAddress, str.c_str());
  }
  return m_result;
}

bool ScanRequest::notify(result_t result, const SlaveSymbolString& slave) {
  symbol_t dstAddress = m_master[1];
  if (result == RESULT_OK) {
    if (m_message == m_messageMap->getScanMessage()) {
      Message* message = m_messageMap->getScanMessage(dstAddress);
      if (message != nullptr) {
        m_message = message;
        m_message->storeLastData(m_index, m_master);  // expected to work since this is a clone
      }
    } else if (m_message->getDstAddress() == SYN) {
      m_message = m_message->derive(dstAddress, true);
      m_messageMap->add(true, m_message);
      m_message->storeLastData(m_index, m_master);  // expected to work since this is a clone
    }
    result = m_message->storeLastData(m_index, slave);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
    if (result == RESULT_OK) {
      ostringstream output;
      result = m_message->decodeLastData(true, nullptr, -1, OF_NONE, &output);  // decode data
      string str = output.str();
      m_busHandler->setScanResult(dstAddress, m_notifyIndex+m_index, str);
    }
  }
  if (result < RESULT_OK) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    if (m_deleteOnFinish) {
      if (result == RESULT_ERR_TIMEOUT) {
        logNotice(lf_bus, "scan %2.2x timed out (%d slaves left)", dstAddress, m_slaves.size());
      } else {
        logError(lf_bus, "scan %2.2x failed (%d slaves left): %s", dstAddress, m_slaves.size(), getResultCode(result));
      }
    }
    m_messages.clear();  // skip remaining secondary messages
  } else if (m_messages.empty()) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    if (m_deleteOnFinish) {
      logNotice(lf_bus, "scan %2.2x completed (%d slaves left)", dstAddress, m_slaves.size());
    }
  }
  m_result = result;
  if (m_slaves.empty()) {
    if (m_deleteOnFinish) {
      logNotice(lf_bus, "scan finished");
    }
    m_busHandler->setScanFinished();
    return false;
  }
  if (m_messages.empty()) {
    m_messages = m_allMessages;
  }
  m_index = 0;
  m_message = m_messages.front();
  m_messages.pop_front();
  result = prepare(m_master[0]);
  if (result < RESULT_OK) {
    m_busHandler->setScanFinished();
    if (result != RESULT_ERR_EOF) {
      m_result = result;
    }
    return false;  // give up
  }
  return true;
}


bool ActiveBusRequest::notify(result_t result, const SlaveSymbolString& slave) {
  if (result == RESULT_OK) {
    string str = slave.getStr();
    logDebug(lf_bus, "read res: %s", str.c_str());
  }
  m_result = result;
  *m_slave = slave;
  return false;
}


void GrabbedMessage::setLastData(const MasterSymbolString& master, const SlaveSymbolString& slave) {
  time(&m_lastTime);
  m_lastMaster = master;
  m_lastSlave = slave;
  m_count++;
}


/**
 * Decode the input @a SymbolString with the specified @a DataType and length.
 * @param type the @a DataType.
 * @param input the @a SymbolString to read the binary value from.
 * @param length the number of symbols to read.
 * @param offsets the last offset to the baseOffset to read.
 * @param firstOnly whether to read only the first non-erroneous offset.
 * @param output the ostringstream to append the formatted value to.
 * @return @a RESULT_OK on success, or an error code.
 */
bool decodeType(const DataType* type, const SymbolString& input, size_t length,
    size_t offsets, bool firstOnly, ostringstream* output) {
  bool first = true;
  string in = input.getStr(input.getDataOffset());
  for (size_t offset = 0; offset <= offsets; offset++) {
    ostringstream out;
    result_t result = type->readSymbols(offset, length, input, OF_NONE, &out);
    if (result != RESULT_OK) {
      continue;
    }
    if (type->isNumeric() && type->hasFlag(DAY)) {
      unsigned int value = 0;
      if (type->readRawValue(offset, length, input, &value) == RESULT_OK) {
        out.str("");
        out << DataField::getDayName(reinterpret_cast<const NumberDataType*>(type)->getMinValue()+value);
      }
    }
    if (first) {
      first = false;
      *output << endl << " ";
      ostringstream::pos_type cnt = output->tellp();
      type->dump(OF_NONE, length, false, output);
      cnt = output->tellp() - cnt;
      while (cnt < 5) {
        *output << " ";
        cnt += 1;
      }
    } else {
      *output << ",";
    }
    *output << " " << in.substr(offset*2, length*2);
    if (type->isNumeric()) {
      *output << "=" << out.str();
    } else {
      *output << "=\"" << out.str() << "\"";
    }
    if (firstOnly) {
      return true;  // only the first offset with maximum length when adjustable maximum size is at least 8 bytes
    }
  }
  return !first;
}

bool GrabbedMessage::dump(bool unknown, MessageMap* messages, bool first, bool decode, ostringstream* output,
    bool isDirectMode) const {
  Message* message = messages->find(m_lastMaster);
  if (unknown && message) {
    return false;
  }
  if (!first) {
    *output << endl;
  }
  symbol_t dstAddress = m_lastMaster[1];
  *output << m_lastMaster.getStr();
  if (dstAddress != BROADCAST && !isMaster(dstAddress)) {
    *output << (isDirectMode ? " " : " / ") << m_lastSlave.getStr();
  }
  if (!isDirectMode) {
    *output << " = " << m_count;
    if (message) {
      *output << ": " << message->getCircuit() << " " << message->getName();
    }
  }
  if (decode) {
    DataTypeList *types = DataTypeList::getInstance();
    if (!types) {
      return true;
    }
    bool master = isMaster(dstAddress) || dstAddress == BROADCAST || m_lastSlave.getDataSize() <= 0;
    size_t remain = master ? m_lastMaster.getDataSize() : m_lastSlave.getDataSize();
    if (remain == 0) {
      return true;
    }
    for (const auto& it : *types) {
      const DataType* baseType = it.second;
      if ((baseType->getBitCount() % 8) != 0 || baseType->isIgnored()) {  // skip bit and ignored types
        continue;
      }
      size_t maxLength = baseType->getBitCount()/8;
      bool firstOnly = maxLength >= 8;
      if (maxLength > remain) {
        maxLength = remain;
      }
      if (baseType->isAdjustableLength()) {
        for (size_t length = maxLength; length >= 1; length--) {
          const DataType* type = types->get(baseType->getId(), length);
          bool decoded;
          if (master) {
            decoded = decodeType(type, m_lastMaster, length, remain-length, firstOnly, output);
          } else {
            decoded = decodeType(type, m_lastSlave, length, remain-length, firstOnly, output);
          }
          if (decoded && firstOnly) {
            break;  // only a single offset with maximum length when adjustable maximum size is at least 8 bytes
          }
        }
      } else if (maxLength > 0) {
        if (master) {
          decodeType(baseType, m_lastMaster, maxLength, remain-maxLength, false, output);
        } else {
          decodeType(baseType, m_lastSlave, maxLength, remain-maxLength, false, output);
        }
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

result_t BusHandler::sendAndWait(const MasterSymbolString& master, SlaveSymbolString* slave) {
  result_t result = RESULT_ERR_NO_SIGNAL;
  slave->clear();
  ActiveBusRequest request(master, slave);
  logInfo(lf_bus, "send message: %s", master.getStr().c_str());

  for (int sendRetries = m_failedSendRetries + 1; sendRetries > 0; sendRetries--) {
    m_nextRequests.push(&request);
    bool success = m_finishedRequests.remove(&request, true);
    result = success ? request.m_result : RESULT_ERR_TIMEOUT;
    if (result == RESULT_OK) {
      break;
    }
    if (!success || result == RESULT_ERR_NO_SIGNAL || result == RESULT_ERR_SEND || result == RESULT_ERR_DEVICE) {
      logError(lf_bus, "send to %2.2x: %s, give up", master[1], getResultCode(result));
      break;
    }
    logError(lf_bus, "send to %2.2x: %s%s", master[1], getResultCode(result), sendRetries > 1 ? ", retry" : "");
    request.m_busLostRetries = 0;
  }
  return result;
}

result_t BusHandler::readFromBus(Message* message, const string& inputStr, symbol_t dstAddress,
    symbol_t srcAddress) {
  symbol_t masterAddress = srcAddress == SYN ? m_ownMasterAddress : srcAddress;
  result_t ret = RESULT_EMPTY;
  MasterSymbolString master;
  SlaveSymbolString slave;
  for (size_t index = 0; index < message->getCount(); index++) {
    istringstream input(inputStr);
    ret = message->prepareMaster(index, masterAddress, dstAddress, UI_FIELD_SEPARATOR, &input, &master);
    if (ret != RESULT_OK) {
      logError(lf_bus, "prepare message part %d: %s", index, getResultCode(ret));
      break;
    }
    // send message
    ret = sendAndWait(master, &slave);
    if (ret != RESULT_OK) {
      logError(lf_bus, "send message part %d: %s", index, getResultCode(ret));
      break;
    }
    ret = message->storeLastData(index, slave);
    if (ret < RESULT_OK) {
      logError(lf_bus, "store message part %d: %s", index, getResultCode(ret));
      break;
    }
  }
  return ret;
}

void BusHandler::run() {
  unsigned int symCount = 0;
  time_t now, lastTime;
  time(&lastTime);
  lastTime += 2;
  logNotice(lf_bus, "bus started with own address %2.2x/%2.2x%s", m_ownMasterAddress, m_ownSlaveAddress,
      m_answer?" in answer mode":"");

  do {
    if (m_device->isValid() && !m_reconnect) {
      result_t result = handleSymbol();
      time(&now);
      if (result != RESULT_ERR_TIMEOUT && now >= lastTime) {
        symCount++;
      }
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
      if (!m_device->isValid()) {
        logNotice(lf_bus, "device invalid");
      }
      if (!Wait(5)) {
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
      m_symbolLatencyMin = m_symbolLatencyMax = m_arbitrationDelayMin = m_arbitrationDelayMax = -1;
      time(&lastTime);
      lastTime += 2;
    }
  } while (isRunning());
}

result_t BusHandler::handleSymbol() {
  unsigned int timeout = SYN_TIMEOUT;
  symbol_t sendSymbol = ESC;
  bool sending = false;

  // check if another symbol has to be sent and determine timeout for receive
  switch (m_state) {
  case bs_noSignal:
    timeout = m_generateSynInterval > 0 ? m_generateSynInterval : SIGNAL_TIMEOUT;
    break;

  case bs_skip:
    timeout = SYN_TIMEOUT;
    [[fallthrough]];
  case bs_ready:
    if (m_currentRequest != nullptr) {
      setState(bs_ready, RESULT_ERR_TIMEOUT);  // just to be sure an old BusRequest is cleaned up
    }
    if (!m_device->isArbitrating() && m_currentRequest == nullptr && m_remainLockCount == 0) {
      BusRequest* startRequest = m_nextRequests.peek();
      if (startRequest == nullptr && m_pollInterval > 0) {  // check for poll/scan
        time_t now;
        time(&now);
        if (m_lastPoll == 0 || difftime(now, m_lastPoll) > m_pollInterval) {
          Message* message = m_messages->getNextPoll();
          if (message != nullptr) {
            m_lastPoll = now;
            auto request = new PollRequest(message);
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
      if (startRequest != nullptr) {  // initiate arbitration
        logDebug(lf_bus, "start request %2.2x", startRequest->m_master[0]);
        result_t ret = m_device->startArbitration(startRequest->m_master[0]);
        if (ret == RESULT_OK) {
          logDebug(lf_bus, "arbitration start with %2.2x", startRequest->m_master[0]);
        } else {
          logError(lf_bus, "arbitration start: %s", getResultCode(ret));
          m_nextRequests.remove(startRequest);
          m_currentRequest = startRequest;
          setState(bs_ready, ret);  // force the failed request to be notified
        }
      }
    }
    break;

  case bs_recvCmd:
  case bs_recvCmdCrc:
    timeout = m_slaveRecvTimeout;
    break;

  case bs_recvCmdAck:
    timeout = m_slaveRecvTimeout;
    break;

  case bs_recvRes:
  case bs_recvResCrc:
    if (m_response.size() > 0 || m_slaveRecvTimeout > SYN_TIMEOUT) {
      timeout = m_slaveRecvTimeout;
    } else {
      timeout = SYN_TIMEOUT;
    }
    break;

  case bs_recvResAck:
    timeout = m_slaveRecvTimeout;
    break;

  case bs_sendCmd:
    if (m_currentRequest != nullptr) {
      sendSymbol = m_currentRequest->m_master[m_nextSendPos];  // unescaped command
      sending = true;
    }
    break;

  case bs_sendCmdCrc:
    if (m_currentRequest != nullptr) {
      sendSymbol = m_crc;
      sending = true;
    }
    break;

  case bs_sendResAck:
    if (m_currentRequest != nullptr) {
      sendSymbol = m_crcValid ? ACK : NAK;
      sending = true;
    }
    break;

  case bs_sendCmdAck:
    if (m_answer) {
      sendSymbol = m_crcValid ? ACK : NAK;
      sending = true;
    }
    break;

  case bs_sendRes:
    if (m_answer) {
      sendSymbol = m_response[m_nextSendPos];  // unescaped response
      sending = true;
    }
    break;

  case bs_sendResCrc:
    if (m_answer) {
      sendSymbol = m_crc;
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
  struct timespec sentTime, recvTime;
  if (sending) {
    if (m_state != bs_sendSyn && (sendSymbol == ESC || sendSymbol == SYN)) {
      if (m_escape) {
        sendSymbol = (symbol_t)(sendSymbol == ESC ? 0x00 : 0x01);
      } else {
        m_escape = sendSymbol;
        sendSymbol = ESC;
      }
    }
    result = m_device->send(sendSymbol);
    clockGettime(&sentTime);
    if (result == RESULT_OK) {
      if (m_state == bs_ready) {
        timeout = m_busAcquireTimeout;
      } else {
        timeout = SEND_TIMEOUT;
      }
    } else {
      sending = false;
      timeout = SYN_TIMEOUT;
      setState(bs_skip, result);
    }
  } else {
    clockGettime(&sentTime);  // for measuring arbitration delay in enhanced protocol
  }

  // receive next symbol (optionally check reception of sent symbol)
  symbol_t recvSymbol;
  ArbitrationState arbitrationState = as_none;
  result = m_device->recv(timeout, &recvSymbol, &arbitrationState);
  if (sending) {
    clockGettime(&recvTime);
  }
  bool sentAutoSyn = false;
  if (!sending && result == RESULT_ERR_TIMEOUT && m_generateSynInterval > 0
      && timeout >= m_generateSynInterval && (m_state == bs_noSignal || m_state == bs_skip)) {
    // check if acting as AUTO-SYN generator is required
    result = m_device->send(SYN);
    if (result != RESULT_OK) {
      return setState(bs_skip, result);
    }
    clockGettime(&sentTime);
    recvSymbol = ESC;
    result = m_device->recv(SEND_TIMEOUT, &recvSymbol, &arbitrationState);
    clockGettime(&recvTime);
    if (result != RESULT_OK) {
      logError(lf_bus, "unable to receive sent AUTO-SYN symbol: %s", getResultCode(result));
      return setState(bs_noSignal, result);
    }
    if (recvSymbol != SYN) {
      logError(lf_bus, "received %2.2x instead of AUTO-SYN symbol", recvSymbol);
      return setState(bs_noSignal, result);
    }
    measureLatency(&sentTime, &recvTime);
    if (m_generateSynInterval != SYN_TIMEOUT) {
      // received own AUTO-SYN symbol back again: act as AUTO-SYN generator now
      m_generateSynInterval = SYN_TIMEOUT;
      logNotice(lf_bus, "acting as AUTO-SYN generator");
    }
    m_remainLockCount = 0;
    m_lastSynReceiveTime = recvTime;
    sentAutoSyn = true;
    setState(bs_ready, RESULT_OK);
  }
  switch (arbitrationState) {
    case as_lost:
      logDebug(lf_bus, "arbitration lost");
      if (m_currentRequest == nullptr) {
        BusRequest *startRequest = m_nextRequests.peek();
        if (startRequest != nullptr && m_nextRequests.remove(startRequest)) {
          m_currentRequest = startRequest;  // force the failed request to be notified
        }
      }
      setState(m_state, RESULT_ERR_BUS_LOST);
      break;
    case as_won:  // implies RESULT_OK
      if (m_currentRequest != nullptr) {
        logNotice(lf_bus, "arbitration won while handling another request");
        setState(bs_ready, RESULT_OK);  // force the current request to be notified
      } else {
        BusRequest *startRequest = m_nextRequests.peek();
        if (m_state != bs_ready || startRequest == nullptr || !m_nextRequests.remove(startRequest)) {
          logNotice(lf_bus, "arbitration won in invalid state %s", getStateCode(m_state));
          setState(bs_ready, RESULT_ERR_TIMEOUT);
        } else {
          logDebug(lf_bus, "arbitration won");
          m_currentRequest = startRequest;
          sendSymbol = m_currentRequest->m_master[0];
          sending = true;
        }
      }
      break;
    case as_running:
      break;
    case as_error:
      logError(lf_bus, "arbitration start error");
      // cancel request
      if (!m_currentRequest) {
        BusRequest *startRequest = m_nextRequests.peek();
        if (startRequest && m_nextRequests.remove(startRequest)) {
          m_currentRequest = startRequest;
        }
      }
      if (m_currentRequest) {
        setState(m_state, RESULT_ERR_BUS_LOST);
      }
      break;
    default:  // only as_none
      break;
  }
  if (sentAutoSyn && !sending) {
    return RESULT_OK;
  }
  time_t now;
  time(&now);
  if (result != RESULT_OK) {
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
    clockGettime(&m_lastSynReceiveTime);
    return setState(bs_ready, m_state == bs_skip ? RESULT_OK : RESULT_ERR_SYN);
  }

  if (sending && m_state != bs_ready) {  // check received symbol for equality if not in arbitration
    if (recvSymbol != sendSymbol) {
      return setState(bs_skip, RESULT_ERR_SYMBOL);
    }
    measureLatency(&sentTime, &recvTime);
  }

  switch (m_state) {
  case bs_ready:
  case bs_recvCmd:
  case bs_recvRes:
  case bs_sendCmd:
  case bs_sendRes:
    SymbolString::updateCrc(recvSymbol, &m_crc);
    break;
  default:
    break;
  }

  if (m_escape) {
    // check escape/unescape state
    if (sending) {
      if (sendSymbol == ESC) {
        return RESULT_OK;
      }
      sendSymbol = recvSymbol = m_escape;
    } else {
      if (recvSymbol > 0x01) {
        return setState(bs_skip, RESULT_ERR_ESC);
      }
      recvSymbol = recvSymbol == 0x00 ? ESC : SYN;
    }
    m_escape = 0;
  } else if (!sending && recvSymbol == ESC) {
    m_escape = ESC;
    return RESULT_OK;
  }

  switch (m_state) {
  case bs_noSignal:
    return setState(bs_skip, RESULT_OK);

  case bs_skip:
    return RESULT_OK;

  case bs_ready:
    if (m_currentRequest != nullptr && sending) {
      // check arbitration
      if (recvSymbol == sendSymbol) {  // arbitration successful
        // measure arbitration delay
        long long latencyLong = (sentTime.tv_sec*1000000000 + sentTime.tv_nsec
        - m_lastSynReceiveTime.tv_sec*1000000000 - m_lastSynReceiveTime.tv_nsec)/1000;
        if (latencyLong >= 0 && latencyLong <= 10000) {  // skip clock skew or out of reasonable range
          auto latency = static_cast<int>(latencyLong);
          logDebug(lf_bus, "arbitration delay %d micros", latency);
          if (m_arbitrationDelayMin < 0 || (latency < m_arbitrationDelayMin || latency > m_arbitrationDelayMax)) {
            if (m_arbitrationDelayMin == -1 || latency < m_arbitrationDelayMin) {
              m_arbitrationDelayMin = latency;
            }
            if (m_arbitrationDelayMax == -1 || latency > m_arbitrationDelayMax) {
              m_arbitrationDelayMax = latency;
            }
            logInfo(lf_bus, "arbitration delay %d - %d micros", m_arbitrationDelayMin, m_arbitrationDelayMax);
          }
        }
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
    m_command.push_back(recvSymbol);
    m_repeat = false;
    return setState(bs_recvCmd, RESULT_OK);

  case bs_recvCmd:
    m_command.push_back(recvSymbol);
    if (m_command.isComplete()) {  // all data received
      return setState(bs_recvCmdCrc, RESULT_OK);
    }
    return RESULT_OK;

  case bs_recvCmdCrc:
    m_crcValid = recvSymbol == m_crc;
    if (m_command[1] == BROADCAST) {
      if (m_crcValid) {
        addSeenAddress(m_command[0]);
        messageCompleted();
        return setState(bs_skip, RESULT_OK);
      }
      return setState(bs_skip, RESULT_ERR_CRC);
    }
    if (m_answer) {
      symbol_t dstAddress = m_command[1];
      if (dstAddress == m_ownMasterAddress || dstAddress == m_ownSlaveAddress) {
        if (m_crcValid) {
          addSeenAddress(m_command[0]);
          m_currentAnswering = true;
          return setState(bs_sendCmdAck, RESULT_OK);
        }
        return setState(bs_sendCmdAck, RESULT_ERR_CRC);
      }
    }
    if (m_crcValid) {
      addSeenAddress(m_command[0]);
      return setState(bs_recvCmdAck, RESULT_OK);
    }
    if (m_repeat) {
      return setState(bs_skip, RESULT_ERR_CRC);
    }
    return setState(bs_recvCmdAck, RESULT_ERR_CRC);

  case bs_recvCmdAck:
    if (recvSymbol == ACK) {
      if (!m_crcValid) {
        return setState(bs_skip, RESULT_ERR_ACK);
      }
      if (m_currentRequest != nullptr) {
        if (isMaster(m_currentRequest->m_master[1])) {
          messageCompleted();
          return setState(bs_sendSyn, RESULT_OK);
        }
      } else if (isMaster(m_command[1])) {
        messageCompleted();
        return setState(bs_skip, RESULT_OK);
      }

      m_repeat = false;
      return setState(bs_recvRes, RESULT_OK);
    }
    if (recvSymbol == NAK) {
      if (!m_repeat) {
        m_repeat = true;
        m_crc = 0;
        m_nextSendPos = 0;
        m_command.clear();
        if (m_currentRequest != nullptr) {
          return setState(bs_sendCmd, RESULT_ERR_NAK, true);
        }
        return setState(bs_recvCmd, RESULT_ERR_NAK);
      }
      return setState(bs_skip, RESULT_ERR_NAK);
    }
    return setState(bs_skip, RESULT_ERR_ACK);

  case bs_recvRes:
    m_response.push_back(recvSymbol);
    if (m_response.isComplete()) {  // all data received
      return setState(bs_recvResCrc, RESULT_OK);
    }
    return RESULT_OK;

  case bs_recvResCrc:
    m_crcValid = recvSymbol == m_crc;
    if (m_crcValid) {
      if (m_currentRequest != nullptr) {
        return setState(bs_sendResAck, RESULT_OK);
      }
      return setState(bs_recvResAck, RESULT_OK);
    }
    if (m_repeat) {
      if (m_currentRequest != nullptr) {
        return setState(bs_sendSyn, RESULT_ERR_CRC);
      }
      return setState(bs_skip, RESULT_ERR_CRC);
    }
    if (m_currentRequest != nullptr) {
      return setState(bs_sendResAck, RESULT_ERR_CRC);
    }
    return setState(bs_recvResAck, RESULT_ERR_CRC);

  case bs_recvResAck:
    if (recvSymbol == ACK) {
      if (!m_crcValid) {
        return setState(bs_skip, RESULT_ERR_ACK);
      }
      messageCompleted();
      return setState(bs_skip, RESULT_OK);
    }
    if (recvSymbol == NAK) {
      if (!m_repeat) {
        m_repeat = true;
        if (m_currentAnswering) {
          m_nextSendPos = 0;
          return setState(bs_sendRes, RESULT_ERR_NAK, true);
        }
        m_response.clear();
        return setState(bs_recvRes, RESULT_ERR_NAK, true);
      }
      return setState(bs_skip, RESULT_ERR_NAK);
    }
    return setState(bs_skip, RESULT_ERR_ACK);

  case bs_sendCmd:
    if (!sending || m_currentRequest == nullptr) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    m_nextSendPos++;
    if (m_nextSendPos >= m_currentRequest->m_master.size()) {
      return setState(bs_sendCmdCrc, RESULT_OK);
    }
    return RESULT_OK;

  case bs_sendCmdCrc:
    if (m_currentRequest->m_master[1] == BROADCAST) {
      messageCompleted();
      return setState(bs_sendSyn, RESULT_OK);
    }
    m_crcValid = true;
    return setState(bs_recvCmdAck, RESULT_OK);

  case bs_sendResAck:
    if (!sending || m_currentRequest == nullptr) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    if (!m_crcValid) {
      if (!m_repeat) {
        m_repeat = true;
        m_response.clear();
        return setState(bs_recvRes, RESULT_ERR_NAK, true);
      }
      return setState(bs_sendSyn, RESULT_ERR_ACK);
    }
    messageCompleted();
    return setState(bs_sendSyn, RESULT_OK);

  case bs_sendCmdAck:
    if (!sending || !m_answer) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    if (!m_crcValid) {
      if (!m_repeat) {
        m_repeat = true;
        m_crc = 0;
        m_command.clear();
        return setState(bs_recvCmd, RESULT_ERR_NAK, true);
      }
      return setState(bs_skip, RESULT_ERR_ACK);
    }
    if (isMaster(m_command[1])) {
      messageCompleted();  // TODO decode command and store value into database of internal variables
      return setState(bs_skip, RESULT_OK);
    }

    m_nextSendPos = 0;
    m_repeat = false;
    {
      Message* message;
      message = m_messages->find(m_command);
      if (message == nullptr) {
        message = m_messages->find(m_command, true);
        if (message != nullptr && message->getSrcAddress() != SYN) {
          message = nullptr;
        }
      }
      if (message == nullptr || message->isWrite()) {
        // don't know this request or definition has wrong direction, deny
        return setState(bs_skip, RESULT_ERR_INVALID_ARG);
      }
      istringstream input;  // TODO create input from database of internal variables
      if (message == m_messages->getScanMessage() || message == m_messages->getScanMessage(m_ownSlaveAddress)) {
        input.str(SCAN_ANSWER);
      }
      // build response and store in m_response for sending back to requesting master
      m_response.clear();
      result = message->prepareSlave(&input, &m_response);
      if (result != RESULT_OK) {
        return setState(bs_skip, result);
      }
    }
    return setState(bs_sendRes, RESULT_OK);

  case bs_sendRes:
    if (!sending || !m_answer) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    m_nextSendPos++;
    if (m_nextSendPos >= m_response.size()) {
      // slave data completely sent
      return setState(bs_sendResCrc, RESULT_OK);
    }
    return RESULT_OK;

  case bs_sendResCrc:
    if (!sending || !m_answer) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    return setState(bs_recvResAck, RESULT_OK);

  case bs_sendSyn:
    if (!sending) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    return setState(bs_skip, RESULT_OK);
  }
  return RESULT_OK;
}

result_t BusHandler::setState(BusState state, result_t result, bool firstRepetition) {
  if (m_currentRequest != nullptr) {
    if (result == RESULT_ERR_BUS_LOST && m_currentRequest->m_busLostRetries < m_busLostRetries) {
      logDebug(lf_bus, "%s during %s, retry", getResultCode(result), getStateCode(m_state));
      m_currentRequest->m_busLostRetries++;
      m_nextRequests.push(m_currentRequest);  // repeat
      m_currentRequest = nullptr;
    } else if (state == bs_sendSyn || (result != RESULT_OK && !firstRepetition)) {
      logDebug(lf_bus, "notify request: %s", getResultCode(result));
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
      m_currentRequest = nullptr;
    }
    if (state == bs_skip) {
      m_device->startArbitration(SYN);  // reset arbitration state
    }
  }

  if (state == bs_noSignal) {  // notify all requests
    m_response.clear();  // notify with empty response
    while ((m_currentRequest = m_nextRequests.pop()) != nullptr) {
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

  m_escape = 0;
  if (state == m_state) {
    return result;
  }
  if ((result < RESULT_OK && !(result == RESULT_ERR_TIMEOUT && state == bs_skip && m_state == bs_ready))
      || (result != RESULT_OK && state == bs_skip && m_state != bs_ready)) {
    logDebug(lf_bus, "%s during %s, switching to %s", getResultCode(result), getStateCode(m_state),
        getStateCode(state));
  } else if (m_currentRequest != nullptr || state == bs_sendCmd || state == bs_sendCmdCrc || state == bs_sendCmdAck
      || state == bs_sendRes || state == bs_sendResCrc || state == bs_sendResAck || state == bs_sendSyn) {
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
    m_crc = 0;
    m_crcValid = false;
    m_response.clear();
    m_nextSendPos = 0;
    m_currentAnswering = false;
  } else if (state == bs_recvRes || state == bs_sendRes) {
    m_crc = 0;
  }
  return result;
}

void BusHandler::measureLatency(struct timespec* sentTime, struct timespec* recvTime) {
  long long latencyLong = (recvTime->tv_sec*1000000000 + recvTime->tv_nsec
      - sentTime->tv_sec*1000000000 - sentTime->tv_nsec)/1000000;
  if (latencyLong < 0 || latencyLong > 1000) {
    return;  // clock skew or out of reasonable range
  }
  auto latency = static_cast<int>(latencyLong);
  logDebug(lf_bus, "send/receive symbol latency %d ms", latency);
  if (m_symbolLatencyMin >= 0 && (latency >= m_symbolLatencyMin && latency <= m_symbolLatencyMax)) {
    return;
  }
  if (m_symbolLatencyMin == -1 || latency < m_symbolLatencyMin) {
    m_symbolLatencyMin = latency;
  }
  if (m_symbolLatencyMax == -1 || latency > m_symbolLatencyMax) {
    m_symbolLatencyMax = latency;
  }
  logInfo(lf_bus, "send/receive symbol latency %d - %d ms", m_symbolLatencyMin, m_symbolLatencyMax);
}

bool BusHandler::addSeenAddress(symbol_t address) {
  if (!isValidAddress(address, false)) {
    return false;
  }
  bool hadConflict = m_addressConflict;
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
      return m_addressConflict && !hadConflict;
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
  return m_addressConflict && !hadConflict;
}

void BusHandler::messageCompleted() {
  const char* prefix = m_currentRequest ? "sent" : "received";
  if (m_currentRequest) {
    m_command = m_currentRequest->m_master;
  }
  symbol_t srcAddress = m_command[0], dstAddress = m_command[1];
  if (srcAddress == dstAddress) {
    logError(lf_bus, "invalid self-addressed message from %2.2x", srcAddress);
    return;
  }
  if (!m_currentAnswering) {
    addSeenAddress(dstAddress);
  }

  bool master = isMaster(dstAddress);
  if (dstAddress == BROADCAST) {
    logInfo(lf_update, "%s BC cmd: %s", prefix, m_command.getStr().c_str());
    if (m_command.getDataSize() >= 10 && m_command[2] == 0x07 && m_command[3] == 0x04) {
      symbol_t slaveAddress = getSlaveAddress(srcAddress);
      addSeenAddress(slaveAddress);
      Message* message = m_messages->getScanMessage(slaveAddress);
      if (message && (message->getLastUpdateTime() == 0 || message->getLastSlaveData().getDataSize() < 10)) {
        // e.g. 10fe07040a b5564149303001248901
        MasterSymbolString dummyMaster;
        istringstream input;
        result_t result = message->prepareMaster(0, m_ownMasterAddress, SYN, UI_FIELD_SEPARATOR, &input,
            &dummyMaster);
        if (result == RESULT_OK) {
          SlaveSymbolString idData;
          idData.push_back(10);
          for (size_t i = 0; i < 10; i++) {
            idData.push_back(m_command.dataAt(i));
          }
          result = message->storeLastData(0, idData);
          if (result == RESULT_OK) {
            ostringstream output;
            result = message->decodeLastData(true, nullptr, -1, OF_NONE, &output);
            if (result == RESULT_OK) {
              string str = output.str();
              setScanResult(slaveAddress, 0, str);
            }
          }
        }
        logNotice(lf_update, "store broadcast ident: %s", getResultCode(result));
      }
    }
  } else if (master) {
    logInfo(lf_update, "%s MM cmd: %s", prefix, m_command.getStr().c_str());
  } else {
    logInfo(lf_update, "%s MS cmd: %s / %s", prefix, m_command.getStr().c_str(), m_response.getStr().c_str());
    if (m_command.size() >= 5 && m_command[2] == 0x07 && m_command[3] == 0x04) {
      Message* message = m_messages->getScanMessage(dstAddress);
      if (message && (message->getLastUpdateTime() == 0 || message->getLastSlaveData().getDataSize() < 10)) {
        result_t result = message->storeLastData(m_command, m_response);
        if (result == RESULT_OK) {
          ostringstream output;
          result = message->decodeLastData(true, nullptr, -1, OF_NONE, &output);
          if (result == RESULT_OK) {
            string str = output.str();
            setScanResult(dstAddress, 0, str);
          }
        }
        logNotice(lf_update, "store %2.2x ident: %s", dstAddress, getResultCode(result));
      }
    }
  }
  Message* message = m_messages->find(m_command);
  if (m_grabMessages) {
    uint64_t key;
    if (message) {
      key = message->getKey();
    } else {
      key = Message::createKey(m_command, m_command[1] == BROADCAST ? 1 : 4);  // up to 4 DD bytes (1 for broadcast)
    }
    m_grabbedMessages[key].setLastData(m_command, m_response);
  }
  if (message == nullptr) {
    if (dstAddress == BROADCAST) {
      logNotice(lf_update, "%s unknown BC cmd: %s", prefix, m_command.getStr().c_str());
    } else if (master) {
      logNotice(lf_update, "%s unknown MM cmd: %s", prefix, m_command.getStr().c_str());
    } else {
      logNotice(lf_update, "%s unknown MS cmd: %s / %s", prefix, m_command.getStr().c_str(),
        m_response.getStr().c_str());
    }
  } else {
    m_messages->invalidateCache(message);
    string circuit = message->getCircuit();
    string name = message->getName();
    const char* mode = message->isScanMessage() ? message->isWrite() ? "scan-write" : "scan-read"
      : message->isPassive() ? message->isWrite() ? "update-write" : "update-read"
      : message->getPollPriority() > 0 ? message->isWrite() ? "poll-write" : "poll-read"
      : message->isWrite() ? "write" : "read";
    result_t result = message->storeLastData(m_command, m_response);
    ostringstream output;
    if (result == RESULT_OK) {
      result = message->decodeLastData(false, nullptr, -1, OF_NONE, &output);
    }
    if (result < RESULT_OK) {
      logError(lf_update, "unable to parse %s %s %s from %s / %s: %s", mode, circuit.c_str(), name.c_str(),
          m_command.getStr().c_str(), m_response.getStr().c_str(), getResultCode(result));
    } else {
      string data = output.str();
      if (m_answer && dstAddress == (master ? m_ownMasterAddress : m_ownSlaveAddress)) {
        logNotice(lf_update, "%s %s self-update %s %s QQ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(),
            srcAddress, data.c_str());  // TODO store in database of internal variables
      } else if (message->getDstAddress() == SYN) {  // any destination
        if (message->getSrcAddress() == SYN) {  // any destination and any source
          logNotice(lf_update, "%s %s %s %s QQ=%2.2x ZZ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(),
              srcAddress, dstAddress, data.c_str());
        } else {
          logNotice(lf_update, "%s %s %s %s ZZ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(), dstAddress,
              data.c_str());
        }
      } else if (message->getSrcAddress() == SYN) {  // any source
        logNotice(lf_update, "%s %s %s %s QQ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(), srcAddress,
            data.c_str());
      } else {
        logNotice(lf_update, "%s %s %s %s: %s", prefix, mode, circuit.c_str(), name.c_str(), data.c_str());
      }
    }
  }
}

result_t BusHandler::prepareScan(symbol_t slave, bool full, const string& levels, bool* reload,
    ScanRequest** request) {
  Message* scanMessage = m_messages->getScanMessage();
  if (scanMessage == nullptr) {
    return RESULT_ERR_NOTFOUND;
  }
  if (m_device->isReadOnly()) {
    return RESULT_OK;
  }
  deque<Message*> messages;
  m_messages->findAll("scan", "", levels, true, true, false, false, true, true, 0, 0, false, &messages);
  auto it = messages.begin();
  while (it != messages.end()) {
    Message* message = *it;
    if (message->getPrimaryCommand() == 0x07 && message->getSecondaryCommand() == 0x04) {
      it = messages.erase(it);  // query pb 0x07 / sb 0x04 only once
    } else {
      it++;
    }
  }

  deque<symbol_t> slaves;
  if (slave != SYN) {
    slaves.push_back(slave);
    if (!*reload) {
      Message* message = m_messages->getScanMessage(slave);
      if (message == nullptr || message->getLastChangeTime() == 0) {
        *reload = true;
      }
    }
  } else {
    *reload = true;
    for (slave = 1; slave != 0; slave++) {  // 0 is known to be a master
      if (!isValidAddress(slave, false) || isMaster(slave)) {
        continue;
      }
      if (!full && (m_seenAddresses[slave]&SEEN) == 0) {
        symbol_t master = getMasterAddress(slave);  // check if we saw the corresponding master already
        if (master == SYN || (m_seenAddresses[master]&SEEN) == 0) {
          continue;
        }
      }
      slaves.push_back(slave);
    }
  }
  if (*reload) {
    messages.push_front(scanMessage);
  }
  if (messages.empty()) {
    return RESULT_OK;
  }
  *request = new ScanRequest(slave == SYN, m_messages, messages, slaves, this, *reload ? 0 : 1);
  result_t result = (*request)->prepare(m_ownMasterAddress);
  if (result < RESULT_OK) {
    delete *request;
    *request = nullptr;
    return result == RESULT_ERR_EOF ? RESULT_EMPTY : result;
  }
  return RESULT_OK;
}

result_t BusHandler::startScan(bool full, const string& levels) {
  if (m_runningScans > 0) {
    return RESULT_ERR_DUPLICATE;
  }
  ScanRequest* request = nullptr;
  bool reload = true;
  result_t result = prepareScan(SYN, full, levels, &reload, &request);
  if (result != RESULT_OK) {
    return result;
  }
  if (!request) {
    return RESULT_ERR_NOTFOUND;
  }
  m_scanResults.clear();
  m_runningScans++;
  m_nextRequests.push(request);
  return RESULT_OK;
}

void BusHandler::setScanResult(symbol_t dstAddress, size_t index, const string& str) {
  m_seenAddresses[dstAddress] |= SCAN_INIT;
  if (str.length() > 0) {
    m_seenAddresses[dstAddress] |= SCAN_DONE;
    vector<string>& result = m_scanResults[dstAddress];
    if (index >= result.size()) {
      result.resize(index+1);
    }
    result[index] = str;
    logNotice(lf_bus, "scan %2.2x: %s", dstAddress, str.c_str());
  }
}

void BusHandler::setScanFinished() {
  if (m_runningScans > 0) {
    m_runningScans--;
  }
}

bool BusHandler::formatScanResult(symbol_t slave, bool leadingNewline, ostringstream* output) const {
  const auto it = m_scanResults.find(slave);
  if (it == m_scanResults.end()) {
    return false;
  }
  if (leadingNewline) {
    *output << endl;
  }
  *output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave);
  for (const auto &result : it->second) {
    *output << result;
  }
  return true;
}

void BusHandler::formatScanResult(ostringstream* output) const {
  if (m_runningScans > 0) {
    *output << m_runningScans << " scan(s) still running" << endl;
  }
  bool first = true;
  for (symbol_t slave = 1; slave != 0; slave++) {  // 0 is known to be a master
    if (formatScanResult(slave, !first, output)) {
      first = false;
    }
  }
  if (first) {
    // fallback to autoscan results
    for (symbol_t slave = 1; slave != 0; slave++) {  // 0 is known to be a master
      if (isValidAddress(slave, false) && !isMaster(slave) && (m_seenAddresses[slave]&SCAN_DONE) != 0) {
        Message* message = m_messages->getScanMessage(slave);
        if (message != nullptr && message->getLastUpdateTime() > 0) {
          if (first) {
            first = false;
          } else {
            *output << endl;
          }
          *output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave);
          message->decodeLastData(true, nullptr, -1, OF_NONE, output);
        }
      }
    }
  }
}

void BusHandler::formatSeenInfo(ostringstream* output) const {
  symbol_t address = 0;
  for (int index = 0; index < 256; index++, address++) {
    bool ownAddress = !m_device->isReadOnly() && (address == m_ownMasterAddress || address == m_ownSlaveAddress);
    if (!isValidAddress(address, false) || ((m_seenAddresses[address]&SEEN) == 0 && !ownAddress)) {
      continue;
    }
    *output << endl << "address " << setfill('0') << setw(2) << hex << static_cast<unsigned>(address);
    symbol_t master;
    if (isMaster(address)) {
      *output << ": master";
      master = address;
    } else {
      *output << ": slave";
      master = getMasterAddress(address);
    }
    if (master != SYN) {
      *output << " #" << setw(0) << dec << getMasterNumber(master);
    }
    if (ownAddress) {
      *output << ", ebusd";
      if (m_answer) {
        *output << " (answering)";
      }
      if (m_addressConflict && (m_seenAddresses[address]&SEEN) != 0) {
        *output << ", conflict";
      }
    }
    if ((m_seenAddresses[address]&SCAN_DONE) != 0) {
      *output << ", scanned";
      Message* message = m_messages->getScanMessage(address);
      if (message != nullptr && message->getLastUpdateTime() > 0) {
        // add detailed scan info: Manufacturer ID SW HW
        *output << " \"";
        result_t result = message->decodeLastData(false, nullptr, -1, OF_NAMES, output);
        if (result != RESULT_OK) {
          *output << "\" error: " << getResultCode(result);
        } else {
          *output << "\"";
        }
      }
    }
    const vector<string>& loadedFiles = m_messages->getLoadedFiles(address);
    if (!loadedFiles.empty()) {
      bool first = true;
      for (const auto& loadedFile : loadedFiles) {
        if (first) {
          first = false;
          *output << ", loaded \"";
        } else {
          *output << ", \"";
        }
        *output << loadedFile << "\"";
        string comment;
        if (m_messages->getLoadedFileInfo(loadedFile, &comment)) {
          if (!comment.empty()) {
            *output << " (" << comment << ")";
          }
        }
      }
    }
  }
}

void BusHandler::formatUpdateInfo(ostringstream* output) const {
  if (hasSignal()) {
    *output << ",\"s\":" << m_maxSymPerSec;
  }
  *output << ",\"c\":" << m_masterCount
          << ",\"m\":" << m_messages->size()
          << ",\"ro\":" << (m_device->isReadOnly() ? 1 : 0)
          << ",\"an\":" << (m_answer ? 1 : 0)
          << ",\"co\":" << (m_addressConflict ? 1 : 0);
  if (m_grabMessages) {
    size_t unknownCnt = 0;
    for (auto it : m_grabbedMessages) {
      Message* message = m_messages->find(it.second.getLastMasterData());
      if (!message) {
        unknownCnt++;
      }
    }
    *output << ",\"gu\":" << unknownCnt;
  }
  unsigned char address = 0;
  for (int index = 0; index < 256; index++, address++) {
    bool ownAddress = !m_device->isReadOnly() && (address == m_ownMasterAddress || address == m_ownSlaveAddress);
    if (!isValidAddress(address, false) || ((m_seenAddresses[address]&SEEN) == 0 && !ownAddress)) {
      continue;
    }
    *output << ",\"" << setfill('0') << setw(2) << hex << static_cast<unsigned>(address) << dec << setw(0)
            << "\":{\"o\":" << (ownAddress ? 1 : 0);
    const auto it = m_scanResults.find(address);
    if (it != m_scanResults.end()) {
      *output << ",\"s\":\"";
      for (const auto& result : it->second) {
        *output << result;
      }
      *output << "\"";
    }
    if ((m_seenAddresses[address]&SCAN_DONE) != 0) {
      Message* message = m_messages->getScanMessage(address);
      if (message != nullptr && message->getLastUpdateTime() > 0) {
        // add detailed scan info: Manufacturer ID SW HW
        message->decodeLastData(true, nullptr, -1, OF_NAMES|OF_NUMERIC|OF_JSON|OF_SHORT, output);
      }
    }
    const vector<string>& loadedFiles = m_messages->getLoadedFiles(address);
    if (!loadedFiles.empty()) {
      *output << ",\"f\":[";
      bool first = true;
      for (const auto& loadedFile : loadedFiles) {
        if (first) {
          first = false;
        } else {
          *output << ",";
        }
        *output << "{\"f\":\"" << loadedFile << "\"";
        string comment;
        if (m_messages->getLoadedFileInfo(loadedFile, &comment)) {
          if (!comment.empty()) {
            *output << ",\"c\":\"" << comment << "\"";
          }
        }
        *output << "}";
      }
      *output << "]";
    }
    *output << "}";
  }
  vector<string> loadedFiles = m_messages->getLoadedFiles();
  if (!loadedFiles.empty()) {
    *output << ",\"l\":{";
    bool first = true;
    for (const auto& loadedFile : loadedFiles) {
      if (first) {
        first = false;
      } else {
        *output << ",";
      }
      *output << "\"" << loadedFile << "\":{";
      string comment;
      size_t hash, size;
      time_t time;
      if (m_messages->getLoadedFileInfo(loadedFile, &comment, &hash, &size, &time)) {
        *output << "\"h\":\"";
        MappedFileReader::formatHash(hash, output);
        *output << "\",\"s\":" << size << ",\"t\":" << time;
      }
      *output << "}";
    }
    *output << "}";
  }
}

result_t BusHandler::scanAndWait(symbol_t dstAddress, bool loadScanConfig, bool reload) {
  if (!isValidAddress(dstAddress, false) || isMaster(dstAddress)) {
    return RESULT_ERR_INVALID_ADDR;
  }
  ScanRequest* request = nullptr;
  bool hasAdditionalScanMessages = m_messages->hasAdditionalScanMessages();
  result_t result = prepareScan(dstAddress, false, "", &reload, &request);
  if (result != RESULT_OK) {
    return result;
  }
  bool requestExecuted = false;
  if (request) {
    if (reload) {
      m_scanResults.erase(dstAddress);
    } else if (m_scanResults.find(dstAddress) != m_scanResults.end()) {
      m_scanResults[dstAddress].resize(1);
    }
    m_runningScans++;
    m_nextRequests.push(request);
    requestExecuted = m_finishedRequests.remove(request, true);
    result = requestExecuted ? request->m_result : RESULT_ERR_TIMEOUT;
    delete request;
    request = nullptr;
  }
  if (loadScanConfig) {
    string file;
    bool timedOut = result == RESULT_ERR_TIMEOUT;
    bool loadFailed = false;
    if (timedOut || result == RESULT_OK) {
      result = loadScanConfigFile(m_messages, dstAddress, false, &file);  // try to load even if one message timed out
      loadFailed = result != RESULT_OK;
      if (timedOut && loadFailed) {
        result = RESULT_ERR_TIMEOUT;  // back to previous result
      }
    }
    if (result == RESULT_OK) {
      executeInstructions(m_messages);
      setScanConfigLoaded(dstAddress, file);
      if (!hasAdditionalScanMessages && m_messages->hasAdditionalScanMessages()) {
        // additional scan messages now available
        scanAndWait(dstAddress, false, false);
      }
    } else if (loadFailed || (requestExecuted && timedOut) || result == RESULT_ERR_NOTAUTHORIZED) {
      setScanConfigLoaded(dstAddress, "");
    }
  }
  return result;
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

void BusHandler::formatGrabResult(bool unknown, bool decode, ostringstream* output, bool isDirectMode,
    time_t since, time_t until) const {
  if (!m_grabMessages) {
    if (!isDirectMode) {
      *output << "grab disabled";
    }
    return;
  }
  bool first = true;
  for (const auto& it : m_grabbedMessages) {
    if ((since > 0 && it.second.getLastTime() < since)
    || (until > 0 && it.second.getLastTime() >= until)) {
      continue;
    }
    if (it.second.dump(unknown, m_messages, first, decode, output, isDirectMode)) {
      first = false;
    }
  }
  if (isDirectMode && !first) {
    *output << endl;
  }
}

symbol_t BusHandler::getNextScanAddress(symbol_t lastAddress) const {
  if (lastAddress == SYN) {
    return SYN;
  }
  while (++lastAddress != 0) {  // 0 is known to be a master
    if (!isValidAddress(lastAddress, false) || isMaster(lastAddress)) {
      continue;
    }
    if ((m_seenAddresses[lastAddress]&(SEEN|LOAD_INIT)) == SEEN) {
      return lastAddress;
    }
    symbol_t master = getMasterAddress(lastAddress);
    if (master == SYN || (m_seenAddresses[master]&SEEN) == 0) {
      continue;
    }
    if ((m_seenAddresses[lastAddress]&LOAD_INIT) == 0) {
      return lastAddress;
    }
  }
  return SYN;
}

void BusHandler::setScanConfigLoaded(symbol_t address, const string& file) {
  m_seenAddresses[address] |= LOAD_INIT;
  if (!file.empty()) {
    m_seenAddresses[address] |= LOAD_DONE;
    m_messages->addLoadedFile(address, file, "");
  }
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/protocol_direct.h"
#include "lib/utils/log.h"

namespace ebusd {

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

/**
 * The @a ProtocolState value by internal @a BusState value index.
 */
static const ProtocolState protocolStateByBusState[] = {
  ps_noSignal,  // bs_noSignal
  ps_idle,      // bs_skip
  ps_idle,      // bs_ready
  ps_recv,      // bs_recvCmd
  ps_recv,      // bs_recvCmdCrc
  ps_recv,      // bs_recvCmdAck
  ps_recv,      // bs_recvRes
  ps_recv,      // bs_recvResCrc
  ps_recv,      // bs_recvResAck
  ps_send,      // bs_sendCmd
  ps_send,      // bs_sendCmdCrc
  ps_send,      // bs_sendResAck
  ps_send,      // bs_sendCmdAck
  ps_send,      // bs_sendRes
  ps_send,      // bs_sendResCrc
  ps_send,      // bs_sendSyn
};


void DirectProtocolHandler::run() {
  unsigned int symCount = 0;
  time_t now, lastTime;
  time(&lastTime);
  lastTime += 2;
  logNotice(lf_bus, "bus started with own address %2.2x/%2.2x%s", m_ownMasterAddress, m_ownSlaveAddress,
      m_config.answer?" in answer mode":"");
  do {
    bool valid = m_device->isValid();
    if (valid && !m_reconnect) {
      unsigned int recvTimeout = 0;
      symbol_t sentSymbol = ESC;
      struct timespec sentTime;
      result_t result = handleSend(&recvTimeout, &sentSymbol, &sentTime);
      bool sent = result == RESULT_CONTINUE;
      do {
        if (result >= RESULT_OK) {
          result = handleReceive(recvTimeout, sent, sentSymbol, &sentTime);
        }
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
        recvTimeout = 0;  // for further buffered bytes
        sent = false;
      } while (result == RESULT_CONTINUE);
    } else {
      if (!valid) {
        logNotice(lf_bus, "device invalid");
        setState(bs_noSignal, RESULT_ERR_DEVICE);
      }
      if (!Wait(5)) {
        break;
      }
      m_reconnect = false;
      result_t result = m_device->open();
      if (result == RESULT_OK) {
        logNotice(lf_bus, "re-opened %s", m_device->getName());
        if (m_config.initialSend && !m_config.readOnly) {
          m_device->send(ESC);
        }
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

#ifndef FALLTHROUGH
#if defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH [[fallthrough]];
#else
#define FALLTHROUGH
#endif
#endif

result_t DirectProtocolHandler::handleSend(unsigned int* recvTimeout, symbol_t* sentSymbol,
struct timespec* sentTime) {
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
    FALLTHROUGH
  case bs_ready:
    if (m_currentRequest != nullptr) {
      setState(bs_ready, RESULT_ERR_TIMEOUT);  // just to be sure an old BusRequest is cleaned up
    }
    if (!m_device->isArbitrating() && m_currentRequest == nullptr && m_remainLockCount == 0) {
      BusRequest* startRequest = m_nextRequests.peek();
      if (startRequest == nullptr) {
        m_listener->notifyProtocolStatus(ps_empty, RESULT_OK);
        startRequest = m_nextRequests.peek();
      }
      if (startRequest != nullptr) {  // initiate arbitration
        symbol_t master = startRequest->getMaster()[0];
        logDebug(lf_bus, "start request %2.2x", master);
        result_t ret = m_device->startArbitration(master);
        if (ret == RESULT_OK) {
          logDebug(lf_bus, "arbitration start with %2.2x", master);
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
    timeout = m_config.slaveRecvTimeout;
    break;

  case bs_recvCmdAck:
    timeout = m_config.slaveRecvTimeout;
    break;

  case bs_recvRes:
  case bs_recvResCrc:
    if (m_response.size() > 0 || m_config.slaveRecvTimeout > SYN_TIMEOUT) {
      timeout = m_config.slaveRecvTimeout;
    } else {
      timeout = SYN_TIMEOUT;
    }
    break;

  case bs_recvResAck:
    timeout = m_config.slaveRecvTimeout;
    break;

  case bs_sendCmd:
    if (m_currentRequest != nullptr) {
      sendSymbol = m_currentRequest->getMaster()[m_nextSendPos];  // unescaped command
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
    if (m_currentAnswering) {
      sendSymbol = m_crcValid ? ACK : NAK;
      sending = true;
    }
    break;

  case bs_sendRes:
    if (m_currentAnswering) {
      sendSymbol = m_response[m_nextSendPos];  // unescaped response
      sending = true;
    }
    break;

  case bs_sendResCrc:
    if (m_currentAnswering) {
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
  if (sending && !m_config.readOnly) {
    if (m_state != bs_sendSyn && (sendSymbol == ESC || sendSymbol == SYN)) {
      if (m_escape) {
        sendSymbol = (symbol_t)(sendSymbol == ESC ? 0x00 : 0x01);
      } else {
        m_escape = sendSymbol;
        sendSymbol = ESC;
      }
    }
    result_t result = m_device->send(sendSymbol);
    clockGettime(sentTime);
    if (result == RESULT_OK) {
      if (m_state == bs_ready) {
        timeout = m_config.busAcquireTimeout;
      } else {
        timeout = SEND_TIMEOUT;
      }
      *sentSymbol = sendSymbol;
    } else {
      sending = false;
      timeout = SYN_TIMEOUT;
      setState(bs_skip, result);
    }
    *recvTimeout = timeout;
    return sending ? RESULT_CONTINUE : result;
  } else {
    clockGettime(sentTime);  // for measuring arbitration delay in enhanced protocol
  }
  *recvTimeout = timeout;
  return RESULT_OK;
}

result_t DirectProtocolHandler::handleReceive(unsigned int timeout, bool sending, symbol_t sentSymbol,
struct timespec* sentTime) {
  // receive next symbol (optionally check reception of sent symbol)
  symbol_t recvSymbol;
  struct timespec recvTime;
  ArbitrationState arbitrationState = as_none;
  result_t result = m_device->recv(timeout, &recvSymbol, &arbitrationState);
  bool sentAutoSyn = false;
  if (sending) {
    clockGettime(&recvTime);
  } else if (!m_config.readOnly && result == RESULT_ERR_TIMEOUT && m_generateSynInterval > 0
      && timeout >= m_generateSynInterval && (m_state == bs_noSignal || m_state == bs_skip)) {
    // check if acting as AUTO-SYN generator is required
    result = m_device->send(SYN);
    if (result != RESULT_OK) {
      return setState(bs_skip, result);
    }
    clockGettime(sentTime);
    recvSymbol = ESC;
    result = m_device->recv(SEND_TIMEOUT, &recvSymbol, &arbitrationState);
    clockGettime(&recvTime);
    if (result < RESULT_OK) {
      logError(lf_bus, "unable to receive sent AUTO-SYN symbol: %s", getResultCode(result));
      return setState(bs_noSignal, result);
    }
    if (recvSymbol != SYN) {
      logError(lf_bus, "received %2.2x instead of AUTO-SYN symbol", recvSymbol);
      return setState(bs_noSignal, result);
    }
    measureLatency(sentTime, &recvTime);
    if (m_generateSynInterval != SYN_INTERVAL) {
      // received own AUTO-SYN symbol back again: act as AUTO-SYN generator now
      m_generateSynInterval = SYN_INTERVAL;
      logNotice(lf_bus, "acting as AUTO-SYN generator");
    }
    m_remainLockCount = 0;
    m_lastSynReceiveTime = recvTime;
    sentAutoSyn = true;
    setState(bs_ready, RESULT_OK);
  }
  switch (arbitrationState) {
    case as_lost:
    case as_timeout:
      logDebug(lf_bus, arbitrationState == as_lost ? "arbitration lost" : "arbitration lost (timed out)");
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
          sentSymbol = m_currentRequest->getMaster()[0];
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
    return result;
  }
  time_t now;
  time(&now);
  if (result < RESULT_OK) {
    if ((m_generateSynInterval != SYN_INTERVAL && difftime(now, m_lastReceive) > 1)
      // at least one full second has passed since last received symbol
      || m_state == bs_noSignal) {
      return setState(bs_noSignal, result);
    }
    return setState(bs_skip, result);
  }

  m_lastReceive = now;
  if ((recvSymbol == SYN) && (m_state != bs_sendSyn)) {
    if (result == RESULT_CONTINUE) {
      if (m_remainLockCount == 0) {
        m_remainLockCount = 1;  // avoid starting arbitration when more data is already buffered
      }
    } else if (!sending) {
      if (m_remainLockCount > 0 && m_command.size() != 1) {
        m_remainLockCount--;
      } else if (m_remainLockCount == 0 && m_command.size() == 1) {
        m_remainLockCount = 1;  // wait for next AUTO-SYN after SYN / address / SYN (bus locked for own priority)
      }
    }
    clockGettime(&m_lastSynReceiveTime);
    return setState(bs_ready, m_state == bs_skip || m_remainLockCount > 0 ? result : RESULT_ERR_SYN);
  }

  if (sending && m_state != bs_ready) {  // check received symbol for equality if not in arbitration
    if (recvSymbol != sentSymbol) {
      return setState(bs_skip, RESULT_ERR_SYMBOL);
    }
    measureLatency(sentTime, &recvTime);
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
      if (sentSymbol == ESC) {
        return result;
      }
      sentSymbol = recvSymbol = m_escape;
    } else {
      if (recvSymbol > 0x01) {
        return setState(bs_skip, RESULT_ERR_ESC);
      }
      recvSymbol = recvSymbol == 0x00 ? ESC : SYN;
    }
    m_escape = 0;
  } else if (!sending && recvSymbol == ESC) {
    m_escape = ESC;
    return result;
  }

  switch (m_state) {
  case bs_noSignal:
    return setState(bs_skip, result);

  case bs_skip:
    return result;

  case bs_ready:
    if (m_currentRequest != nullptr && sending) {
      // check arbitration
      if (recvSymbol == sentSymbol) {  // arbitration successful
        // measure arbitration delay
        int64_t latencyLong = (sentTime->tv_sec*1000000000LL + sentTime->tv_nsec
        - m_lastSynReceiveTime.tv_sec*1000000000LL - m_lastSynReceiveTime.tv_nsec)/1000;
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
        return setState(bs_sendCmd, result);
      }
      // arbitration lost. if same priority class found, try again after next AUTO-SYN
      m_remainLockCount = isMaster(recvSymbol) ? 2 : 1;  // number of SYN to wait for before next send try
      if ((recvSymbol & 0x0f) != (sentSymbol & 0x0f) && m_lockCount > m_remainLockCount) {
        // if different priority class found, try again after N AUTO-SYN symbols (at least next AUTO-SYN)
        m_remainLockCount = m_lockCount;
      }
      setState(m_state, RESULT_ERR_BUS_LOST);  // try again later
    }
    m_command.push_back(recvSymbol);
    m_repeat = false;
    return setState(bs_recvCmd, result);

  case bs_recvCmd:
    if ((m_command.size() == 0 && !isMaster(recvSymbol))
    || (m_command.size() == 1 && !isValidAddress(recvSymbol))) {
      return setState(bs_skip, RESULT_ERR_INVALID_ADDR);
    }
    m_command.push_back(recvSymbol);
    if (m_command.isComplete()) {  // all data received
      return setState(bs_recvCmdCrc, result);
    }
    return result;

  case bs_recvCmdCrc:
    m_crcValid = recvSymbol == m_crc;
    if (m_command[1] == BROADCAST) {
      if (m_crcValid) {
        addSeenAddress(m_command[0]);
        messageCompleted();
        return setState(bs_skip, result);
      }
      return setState(bs_skip, RESULT_ERR_CRC);
    }
    if (m_crcValid) {
      addSeenAddress(m_command[0]);
      m_currentAnswering = getAnswer();
      return setState(m_currentAnswering ? bs_sendCmdAck : bs_recvCmdAck, result);
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
        if (isMaster(m_currentRequest->getMaster()[1])) {
          messageCompleted();
          return setState(bs_sendSyn, result);
        }
      } else if (isMaster(m_command[1])) {
        messageCompleted();
        return setState(bs_skip, result);
      }

      m_repeat = false;
      return setState(bs_recvRes, result);
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
      return setState(bs_recvResCrc, result);
    }
    return result;

  case bs_recvResCrc:
    m_crcValid = recvSymbol == m_crc;
    if (m_crcValid) {
      if (m_currentRequest != nullptr) {
        return setState(bs_sendResAck, result);
      }
      return setState(bs_recvResAck, result);
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
      return setState(bs_skip, result);
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
    if (m_nextSendPos >= m_currentRequest->getMaster().size()) {
      return setState(bs_sendCmdCrc, result);
    }
    return result;

  case bs_sendCmdCrc:
    if (m_currentRequest->getMaster()[1] == BROADCAST) {
      messageCompleted();
      return setState(bs_sendSyn, result);
    }
    m_crcValid = true;
    return setState(bs_recvCmdAck, result);

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
    return setState(bs_sendSyn, result);

  case bs_sendCmdAck:
    if (!sending || !m_currentAnswering) {
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
    // response to send was already prepared during bs_recvCmdCrc in m_response
    if (isMaster(m_command[1])) {
      messageCompleted();
      return setState(bs_skip, result);
    }

    m_nextSendPos = 0;
    m_repeat = false;
    return setState(bs_sendRes, result);

  case bs_sendRes:
    if (!sending || !m_currentAnswering) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    m_nextSendPos++;
    if (m_nextSendPos >= m_response.size()) {
      // slave data completely sent
      return setState(bs_sendResCrc, result);
    }
    return result;

  case bs_sendResCrc:
    if (!sending || !m_currentAnswering) {
      return setState(bs_skip, RESULT_ERR_INVALID_ARG);
    }
    return setState(bs_recvResAck, result);

  case bs_sendSyn:
    if (!sending) {
      return setState(bs_ready, RESULT_ERR_INVALID_ARG);
    }
    return setState(bs_ready, result);
  }
  return result;
}

result_t DirectProtocolHandler::setState(BusState state, result_t result, bool firstRepetition) {
  if (m_currentRequest != nullptr) {
    if (result == RESULT_ERR_BUS_LOST && m_currentRequest->getBusLostRetries() < m_config.busLostRetries) {
      logDebug(lf_bus, "%s during %s, retry", getResultCode(result), getStateCode(m_state));
      m_currentRequest->incrementBusLostRetries();
      m_nextRequests.push(m_currentRequest);  // repeat
      m_currentRequest = nullptr;
    } else if (state == bs_sendSyn || (result < RESULT_OK && !firstRepetition)) {
      logDebug(lf_bus, "notify request: %s", getResultCode(result));
      bool restart = m_currentRequest->notify(
        result == RESULT_ERR_SYN && (m_state == bs_recvCmdAck || m_state == bs_recvRes)
        ? RESULT_ERR_TIMEOUT : result, m_response);
      if (restart) {
        m_currentRequest->resetBusLostRetries();
        m_nextRequests.push(m_currentRequest);
      } else if (m_currentRequest->deleteOnFinish()) {
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
      m_currentRequest->notify(RESULT_ERR_NO_SIGNAL, m_response);
      if (m_currentRequest->deleteOnFinish()) {
        delete m_currentRequest;
      } else {
        m_finishedRequests.push(m_currentRequest);
      }
    }
  }

  m_escape = 0;
  if (state == m_state) {
    if (m_listener && result < RESULT_OK && state != bs_noSignal) {
      m_listener->notifyProtocolStatus(m_listenerState, result);
    }
    return result;
  }
  if ((result < RESULT_OK && !(result == RESULT_ERR_TIMEOUT && state == bs_skip && m_state == bs_ready))
      || (result < RESULT_OK && state == bs_skip && m_state != bs_ready)) {
    logDebug(lf_bus, "%s during %s, switching to %s", getResultCode(result), getStateCode(m_state),
        getStateCode(state));
  } else if (m_currentRequest != nullptr || state == bs_sendCmd || state == bs_sendCmdCrc || state == bs_sendCmdAck
      || state == bs_sendRes || state == bs_sendResCrc || state == bs_sendResAck || state == bs_sendSyn
      || m_state == bs_sendSyn) {
    logDebug(lf_bus, "switching from %s to %s", getStateCode(m_state), getStateCode(state));
  }
  if (state == bs_noSignal) {
    if (m_generateSynInterval == 0 || m_state != bs_skip) {
      logError(lf_bus, "signal lost");
    }
  } else if (m_state == bs_noSignal) {
    if (m_generateSynInterval == 0 || state != bs_skip) {
      logNotice(lf_bus, "signal acquired");
    }
  }
  if (m_listener) {
    ProtocolState pstate = protocolStateByBusState[state];
    if (pstate == ps_idle && m_generateSynInterval == SYN_INTERVAL) {
      pstate = ps_idleSYN;
    }
    if (result < RESULT_OK || pstate != m_listenerState) {
      m_listener->notifyProtocolStatus(pstate, result);
      m_listenerState = pstate;
    }
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

bool DirectProtocolHandler::addSeenAddress(symbol_t address) {
  if (!ProtocolHandler::addSeenAddress(address)) {
    return false;
  }
  if (m_config.lockCount == 0 && m_masterCount > m_lockCount) {
    m_lockCount = m_masterCount;
  }
  return true;
}

void DirectProtocolHandler::messageCompleted() {
  // do an explicit copy here in case being called by another thread
  const MasterSymbolString command(m_currentRequest ? m_currentRequest->getMaster() : m_command);
  const SlaveSymbolString response(m_response);
  symbol_t srcAddress = command[0], dstAddress = command[1];
  if (srcAddress == dstAddress) {
    logError(lf_bus, "invalid self-addressed message from %2.2x", srcAddress);
    return;
  }
  if (!m_currentAnswering || (dstAddress != m_ownMasterAddress && dstAddress != m_ownSlaveAddress)) {
    // also add given answers to list of seen addresses
    addSeenAddress(dstAddress);
  }

  const char* prefix = m_currentAnswering ? "answered" : m_currentRequest ? "sent" : "received";
  MessageDirection direction = m_currentAnswering ? md_answer : m_currentRequest ? md_send : md_recv;
  bool master = isMaster(dstAddress);
  if (dstAddress == BROADCAST || master) {
    logInfo(lf_update, "%s %s cmd: %s", prefix, master ? "MM" : "BC", command.getStr().c_str());
  } else {
    logInfo(lf_update, "%s MS cmd: %s / %s", prefix, command.getStr().c_str(), response.getStr().c_str());
  }
  m_listener->notifyProtocolMessage(direction, command, response);
}

uint64_t DirectProtocolHandler::createAnswerKey(symbol_t srcAddress, symbol_t dstAddress, symbol_t pb, symbol_t sb,
    const symbol_t* id, size_t idLen) {
  uint64_t key = (uint64_t)idLen << (8 * 7 + 5);
  key |= (uint64_t)getMasterNumber(srcAddress) << (8 * 7);  // 0..25
  key |= (uint64_t)dstAddress << (8 * 6);
  key |= (uint64_t)pb << (8 * 5);
  key |= (uint64_t)sb << (8 * 4);
  int exp = 3;
  for (size_t pos = 0; pos < idLen; pos++) {
    key |= (uint64_t)id[pos] << (8 * exp--);
  }
  return key;
}

bool DirectProtocolHandler::setAnswer(symbol_t srcAddress, symbol_t dstAddress, symbol_t pb, symbol_t sb,
    const symbol_t* id, size_t idLen, const SlaveSymbolString& answer) {
  if (!m_config.answer || (!id && idLen > 0) || idLen > 4 || !isValidAddress(dstAddress, false)
  || (srcAddress != SYN && !isMaster(srcAddress))) {
    return false;
  }
  if (isMaster(dstAddress)) {
    if (answer.size() > 7) {
      return false;
    }
    // answer used here only for having the expected length of the MM data tail
  } else {
    if (!answer.isComplete()) {
      return false;
    }
  }
  uint64_t key = createAnswerKey(srcAddress, dstAddress, pb, sb, id, idLen);
  m_answerByKey[key] = answer;
  return true;
}

bool DirectProtocolHandler::hasAnswer(symbol_t dstAddress) const {
  if (m_answerByKey.empty()) {
    return false;
  }
  for (auto const &answer : m_answerByKey) {
    if ((answer.first >> (8 * 6)) == dstAddress) {
      return true;
    }
  }
  return false;
}

bool DirectProtocolHandler::getAnswer() {
  if (m_answerByKey.empty()) {
    return false;
  }
  // walk through the stored answers to find the longest match
  m_response.clear();
  size_t len = m_command[4];
  bool master = isMaster(m_command[1]);
  uint64_t key = createAnswerKey(m_command[0], m_command[1], m_command[2], m_command[3], m_command.data()+5, len);
  do {
    auto it = m_answerByKey.find(key);
    if (it == m_answerByKey.end()) {
      it = m_answerByKey.find(key&~(0x1fLL << (8 * 7)));  // without specific src
    }
    if (it != m_answerByKey.end()) {
      // found the answer
      if (master) {
        if (len+it->second.size() == m_command[4]) {
          m_response = it->second;  // copied for having the data size only
          return true;
        }
        // data length mismatch, find shorter one
      } else {
        m_response = it->second;
        return true;
      }
      break;
    }
    if (len == 0) {
      break;
    }
    // reduce the key
    len--;
    key = (key&~(0x07LL << (8 * 7 + 5))&~(0xffLL << (8 * (3-len)))) | (len << (8 * 7 + 5));
  } while (true);
  return false;
}

}  // namespace ebusd

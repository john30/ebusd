/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2023 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/protocol.h"
#include "lib/ebus/protocol_direct.h"
#include <string>
#include "lib/utils/log.h"

namespace ebusd {

bool ActiveBusRequest::notify(result_t result, const SlaveSymbolString& slave) {
  if (result == RESULT_OK) {
    string str = slave.getStr();
    logDebug(lf_bus, "read res: %s", str.c_str());
  }
  m_result = result;
  *m_slave = slave;
  return false;
}


ProtocolHandler* ProtocolHandler::create(const ebus_protocol_config_t config,
  Device* device, ProtocolListener* listener) {
  return new DirectProtocolHandler(config, device, listener);
}

void ProtocolHandler::clear() {
  memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
  m_masterCount = 1;
}

bool ProtocolHandler::addRequest(BusRequest* request, bool wait) {
  m_nextRequests.push(request);
  return !wait || m_finishedRequests.remove(request, true);
}

result_t ProtocolHandler::sendAndWait(const MasterSymbolString& master, SlaveSymbolString* slave) {
  if (!hasSignal()) {
    return RESULT_ERR_NO_SIGNAL;  // don't wait when there is no signal
  }
  result_t result = RESULT_ERR_NO_SIGNAL;
  slave->clear();
  ActiveBusRequest request(master, slave);
  logInfo(lf_bus, "send message: %s", master.getStr().c_str());

  for (int sendRetries = m_config.failedSendRetries + 1; sendRetries > 0; sendRetries--) {
    bool success = addRequest(&request, true);
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

void ProtocolHandler::measureLatency(struct timespec* sentTime, struct timespec* recvTime) {
  int64_t latencyLong = (recvTime->tv_sec*1000000000 + recvTime->tv_nsec
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

bool ProtocolHandler::addSeenAddress(symbol_t address) {
  if (!isValidAddress(address, false)) {
    return false;
  }
  if (!isMaster(address)) {
    if (!m_device->isReadOnly() && address == m_ownSlaveAddress) {
      if (!m_addressConflict) {
        m_addressConflict = true;
        logError(lf_bus, "own slave address %2.2x is used by another participant", address);
      }
    }
    if (!m_seenAddresses[address]) {
      m_listener->notifyProtocolSeenAddress(address);
    }
    m_seenAddresses[address] = true;
    address = getMasterAddress(address);
    if (address == SYN) {
      return false;
    }
  }
  if (m_seenAddresses[address]) {
    return false;
  }
  bool ret = false;
  if (!m_device->isReadOnly() && address == m_ownMasterAddress) {
    if (!m_addressConflict) {
      m_addressConflict = true;
      logError(lf_bus, "own master address %2.2x is used by another participant", address);
    }
  } else {
    m_masterCount++;
    ret = true;
    logNotice(lf_bus, "new master %2.2x, master count %d", address, m_masterCount);
  }
  m_listener->notifyProtocolSeenAddress(address);
  m_seenAddresses[address] = true;
  return ret;
}

}  // namespace ebusd

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

#include <string>
#include <iomanip>
#include "lib/ebus/device_trans.h"
#include "lib/ebus/protocol.h"
#include "lib/ebus/protocol_direct.h"
#include "lib/utils/log.h"

namespace ebusd {

using std::hex;
using std::setfill;
using std::setw;

const char* getProtocolStateCode(ProtocolState state) {
  switch (state) {
    case ps_noSignal: return "no signal";
    case ps_idle:     return "idle";
    case ps_idleSYN:  return "idle, SYN generator";
    case ps_recv:     return "receive";
    case ps_send:     return "send";
    case ps_empty:    return "idle, empty";
    default:          return "unknown";
  }
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

ProtocolHandler* ProtocolHandler::create(const ebus_protocol_config_t config,
  ProtocolListener* listener) {
  const char* name = config.device;
  bool enhanced = false;
  uint8_t speed = 0;
  if (name[0] == 'e' && name[1] && name[2] && name[3] == ':') {
    speed = name[2] == 's' ? 2 : name[2] == 'h' ? 1 : 0;
    enhanced = speed > 0 && name[1] == 'n';
    if (enhanced) {
      name += 4;
    } else {
      speed = 0;
    }
  }
  Transport* transport;
  // symlink device name may contain colon, so only check for absence of slash only
  if (strchr(name, '/') == nullptr) {
    char* in = strdup(name);
    bool udp = false;
    char* addrpos = in;
    char* portpos = strchr(addrpos, ':');
    // support tcp:<ip>[:<port>] and udp:<ip>[:<port>]
    if (portpos == addrpos+3 && (strncmp(addrpos, "tcp", 3) == 0 || (udp=(strncmp(addrpos, "udp", 3) == 0)))) {
      addrpos += 4;
      portpos = strchr(addrpos, ':');
    }
    uint16_t port;
    if (portpos == nullptr) {
      port = 9999;
    } else {
      result_t result = RESULT_OK;
      port = (uint16_t)parseInt(portpos+1, 10, 1, 65535, &result);
      if (result != RESULT_OK) {
        free(in);
        return nullptr;  // invalid port
      }
      *portpos = 0;
    }
    char* hostOrIp = strdup(addrpos);
    free(in);
    transport = new NetworkTransport(name, config.extraLatency, hostOrIp, port, udp);
  } else {
    // support ens:/dev/<device>, enh:/dev/<device>, and /dev/<device>
    // as well as symlinks like /dev/serial/by-id/...Espressif_00:01:02:03...
    transport = new SerialTransport(name, config.extraLatency, !config.noDeviceCheck, speed);
  }
  Device* device;
  if (enhanced) {
    device = new EnhancedDevice(transport);
  } else {
    device = new PlainDevice(transport);
  }
  return new DirectProtocolHandler(config, device, listener);
}

result_t ProtocolHandler::open() {
  result_t result = m_device->open();
  if (result != RESULT_OK) {
    logError(lf_bus, "unable to open %s: %s", m_device->getName(), getResultCode(result));
  } else if (!m_device->isValid()) {
    logError(lf_bus, "device %s not available", m_device->getName());
  }
  return result;
}

void ProtocolHandler::formatInfo(ostringstream* ostream, bool verbose, bool noWait) {
  m_device->formatInfo(ostream, verbose, true);
  if (isReadOnly()) {
    *ostream << ", readonly";
  }
  if (noWait) {
    return;
  }
  m_device->formatInfo(ostream, verbose, false);
}

void ProtocolHandler::formatInfoJson(ostringstream* ostream) const {
  m_device->formatInfoJson(ostream);
}

void ProtocolHandler::notifyDeviceData(const symbol_t* data, size_t len, bool received) {
  if (received && m_dumpFile) {
    m_dumpFile->write(data, len);
  }
  if (!m_logRawFile && !m_logRawEnabled) {
    return;
  }
  if (m_logRawBytes) {
    if (m_logRawFile) {
      m_logRawFile->write(data, len, received);
    } else if (m_logRawEnabled) {
      for (size_t pos = 0; pos < len; pos++) {
        logNotice(lf_bus, "%c%02x", received ? '<' : '>', data[pos]);
      }
    }
    return;
  }
  for (size_t pos = 0; pos < len; pos++) {
    symbol_t symbol = data[pos];
    if (symbol != SYN) {
      if (received && !m_logRawLastReceived && symbol == m_logRawLastSymbol) {
        continue;  // skip received echo of previously sent symbol
      }
      if (m_logRawBuffer.tellp() == 0 || received != m_logRawLastReceived) {
        m_logRawLastReceived = received;
        if (m_logRawBuffer.tellp() == 0 && m_logRawLastSymbol != SYN) {
          m_logRawBuffer << "...";
        }
        m_logRawBuffer << (received ? "<" : ">");
      }
      m_logRawBuffer << setw(2) << setfill('0') << hex << static_cast<unsigned>(symbol);
    }
    m_logRawLastSymbol = symbol;
    if (m_logRawBuffer.tellp() > (symbol == SYN ? 0 : 64)) {  // flush: direction+5 hdr+24 max data+crc+direction+ack+1
      if (symbol != SYN) {
        m_logRawBuffer << "...";
      }
      const string bufStr = m_logRawBuffer.str();
      const char* str = bufStr.c_str();
      if (m_logRawFile) {
        m_logRawFile->write((const unsigned char*)str, strlen(str), received, false);
      } else {
        logNotice(lf_bus, str);
      }
      m_logRawBuffer.str("");
    }
  }
}

void ProtocolHandler::notifyDeviceStatus(bool error, const char* message) {
  if (error) {
    logError(lf_device, message);
  } else {
    logNotice(lf_device, message);
  }
}


void ProtocolHandler::clear() {
  memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
  m_masterCount = 1;
}

result_t ProtocolHandler::addRequest(BusRequest* request, bool wait) {
  if (m_config.readOnly) {
    return RESULT_ERR_DEVICE;
  }
  m_nextRequests.push(request);
  if (!wait || m_finishedRequests.remove(request, true)) {
    return RESULT_OK;
  }
  return RESULT_ERR_TIMEOUT;
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
    result = addRequest(&request, true);
    bool success = result == RESULT_OK;
    if (success) {
      result = request.m_result;
    }
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
    if (!m_config.readOnly && address == m_ownSlaveAddress) {
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
  if (!m_config.readOnly && address == m_ownMasterAddress) {
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

void ProtocolHandler::setDumpFile(const char* dumpFile, unsigned int dumpSize, bool dumpFlush) {
  if (m_dumpFile) {
    delete m_dumpFile;
    m_dumpFile = nullptr;
  }
  if (dumpFile && dumpFile[0]) {
    m_dumpFile = new RotateFile(dumpFile, dumpSize, false, dumpFlush ? 1 : 16);
  }
}

bool ProtocolHandler::toggleDump() {
  if (!m_dumpFile) {
    return false;
  }
  bool enabled = !m_dumpFile->isEnabled();
  m_dumpFile->setEnabled(enabled);
  return enabled;
}

void ProtocolHandler::setLogRawFile(const char* logRawFile, unsigned int logRawSize) {
  if (logRawFile[0]) {
    m_logRawFile = new RotateFile(logRawFile, logRawSize, true);
    m_logRawFile->setEnabled(m_logRawEnabled);
  } else {
    m_logRawFile = nullptr;
  }
}

bool ProtocolHandler::toggleLogRaw(bool bytes) {
  bool enabled;
  m_logRawBytes = bytes;
  if (m_logRawFile) {
    enabled = !m_logRawFile->isEnabled();
    m_logRawFile->setEnabled(enabled);
  } else {
    enabled = !m_logRawEnabled;
    m_logRawEnabled = enabled;
  }
  return enabled;
}

}  // namespace ebusd

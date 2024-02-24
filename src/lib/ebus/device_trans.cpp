/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2024 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/device_trans.h"
#include <cstdlib>
#include <string>
#include <iomanip>
#include "lib/utils/clock.h"

namespace ebusd {

using std::hex;
using std::dec;
using std::setfill;
using std::setw;
using std::setprecision;
using std::fixed;



result_t BaseDevice::startArbitration(symbol_t masterAddress) {
  if (m_arbitrationCheck) {
    if (masterAddress != SYN) {
      return RESULT_ERR_ARB_RUNNING;  // should not occur
    }
    return RESULT_OK;
  }
  m_arbitrationMaster = masterAddress;
  return RESULT_OK;
}

bool BaseDevice::cancelRunningArbitration(ArbitrationState* arbitrationState) {
  if (m_arbitrationMaster == SYN) {
    return false;
  }
  if (arbitrationState) {
    *arbitrationState = as_error;
  }
  m_arbitrationMaster = SYN;
  m_arbitrationCheck = 0;
  return true;
}


result_t PlainDevice::send(symbol_t value) {
  result_t result = m_transport->write(&value, 1);
  if (result == RESULT_OK && m_listener != nullptr) {
    m_listener->notifyDeviceData(&value, 1, false);
  }
  return result;
}

result_t PlainDevice::recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) {
  if (m_arbitrationMaster != SYN && arbitrationState) {
    *arbitrationState = as_running;
  }
  uint64_t until = timeout == 0 ? 0 : clockGetMillis() + timeout + m_transport->getLatency();
  const uint8_t* data = nullptr;
  size_t len = 0;
  result_t result;
  do {
    result = m_transport->read(timeout, &data, &len);
    if (result == RESULT_OK) {
      break;
    }
    if (result != RESULT_ERR_TIMEOUT) {
      cancelRunningArbitration(arbitrationState);
      return result;
    }
    if (timeout == 0) {
      break;
    }
    uint64_t now = clockGetMillis();
    if (timeout == 0 || now >= until) {
      break;
    }
    timeout = static_cast<unsigned>(until - now);
  } while (true);
  if (result == RESULT_OK && len > 0 && data) {
    *value = *data;
    m_transport->readConsumed(1);
    if (m_listener != nullptr) {
      m_listener->notifyDeviceData(value, 1, true);
    }
    if (len > 1) {
      result = RESULT_CONTINUE;
    }
    if (*value != SYN || m_arbitrationMaster == SYN || m_arbitrationCheck) {
      if (m_arbitrationMaster != SYN && arbitrationState) {
        if (m_arbitrationCheck) {
          *arbitrationState = *value == m_arbitrationMaster ? as_won : as_lost;
          m_arbitrationMaster = SYN;
          m_arbitrationCheck = 0;
        } else {
          *arbitrationState = m_arbitrationMaster == SYN ? as_none : as_start;
        }
      }
      return result;
    }
    if (len == 1 && arbitrationState) {
      // arbitration executed by ebusd itself
      bool wrote = m_transport->write(&m_arbitrationMaster, 1) == RESULT_OK;  // send as fast as possible
      if (!wrote) {
        cancelRunningArbitration(arbitrationState);
        return result;
      }
      if (m_listener != nullptr) {
        m_listener->notifyDeviceData(&m_arbitrationMaster, 1, false);
      }
      m_arbitrationCheck = 1;
      *arbitrationState = as_running;
    }
  }
  return result;
}


/** the features requested. */
#define REQUEST_FEATURES 0x01

void EnhancedDevice::formatInfo(ostringstream* ostream, bool verbose, bool prefix) {
  BaseDevice::formatInfo(ostream, verbose, prefix);
  if (prefix) {
    *ostream << ", enhanced";
    return;
  }
  bool infoAdded = false;
  if (verbose) {
    string info = getEnhancedInfos();
    if (!info.empty()) {
      *ostream << ", " << info;
      infoAdded = true;
    }
  }
  if (!infoAdded) {
    string ver = getEnhancedVersion();
    if (!ver.empty()) {
      *ostream << ", firmware " << ver;
    }
  }
}

void EnhancedDevice::formatInfoJson(ostringstream* ostream) const {
  string ver = getEnhancedVersion();
  if (!ver.empty()) {
    *ostream << ",\"dv\":\"" << ver << "\"";
  }
}

result_t EnhancedDevice::requestEnhancedInfo(symbol_t infoId, bool wait) {
  if (m_extraFeatures == 0) {
    return RESULT_ERR_INVALID_ARG;
  }
  if (wait) {
    for (unsigned int i = 0; i < 4; i++) {
      if (m_infoLen == 0) {
        break;
      }
      usleep(40000 + i*40000);
    }
    if (m_infoLen > 0) {
      if (m_infoReqTime > 0 && time(NULL) > m_infoReqTime+5) {
        // request timed out
        if (m_listener != nullptr) {
          m_listener->notifyDeviceStatus(false, "info request timed out");
        }
        m_infoLen = 0;
        m_infoReqTime = 0;
      } else {
        return RESULT_ERR_DUPLICATE;
      }
    }
  }
  if (infoId == 0xff) {
    // just waited for completion
    return RESULT_OK;
  }
  uint8_t buf[] = makeEnhancedSequence(ENH_REQ_INFO, infoId);
  result_t result = m_transport->write(buf, 2);
  if (result == RESULT_OK) {
    m_infoBuf[0] = infoId;
    m_infoLen = 1;
    m_infoPos = 1;
    time(&m_infoReqTime);
  } else {
    m_infoLen = 0;
    m_infoPos = 0;
  }
  return result;
}

string EnhancedDevice::getEnhancedInfos() {
  if (m_extraFeatures == 0) {
    return "";
  }
  result_t res;
  string fails = "";
  if (m_enhInfoTemperature.empty()) {  // use empty temperature for potential refresh after reset
    res = requestEnhancedInfo(0);
    if (res != RESULT_OK) {
      return "cannot request version";
    }
    res = requestEnhancedInfo(1);
    if (res != RESULT_OK) {
      return "cannot request ID";
    }
    res = requestEnhancedInfo(2);
    if (res != RESULT_OK) {
      fails += ", cannot request config";
      requestEnhancedInfo(0xff);  // wait for completion
      m_infoLen = 0;  // cancel anyway
    }
  }
  res = requestEnhancedInfo(6);
  if (res != RESULT_OK) {
    return "cannot request reset info";
  }
  res = requestEnhancedInfo(3);
  if (res != RESULT_OK) {
    return "cannot request temperature";
  }
  res = requestEnhancedInfo(4);
  if (res != RESULT_OK) {
    return "cannot request supply voltage";
  }
  res = requestEnhancedInfo(5);
  if (res != RESULT_OK) {
    fails += ", cannot request bus voltage";
  }
  if (m_enhInfoIsWifi) {
    res = requestEnhancedInfo(7);
    if (res != RESULT_OK) {
      fails += ", cannot request rssi";
    }
  }
  res = requestEnhancedInfo(0xff);  // wait for completion
  if (res != RESULT_OK) {
    m_enhInfoBusVoltage = "bus voltage unknown";
    m_infoLen = 0;  // cancel anyway
  }
  return "firmware " + m_enhInfoVersion + ", " + m_enhInfoTemperature + ", " + m_enhInfoSupplyVoltage + ", "
  + m_enhInfoBusVoltage;
}

result_t EnhancedDevice::send(symbol_t value) {
  uint8_t buf[] = makeEnhancedSequence(ENH_REQ_SEND, value);
  result_t result = m_transport->write(buf, 2);
  if (result == RESULT_OK && m_listener != nullptr) {
    m_listener->notifyDeviceData(&value, 1, false);
  }
  return result;
}

result_t EnhancedDevice::recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) {
  if (arbitrationState && m_arbitrationMaster != SYN) {
    *arbitrationState = as_running;
  }
  uint64_t until = timeout == 0 ? 0 : clockGetMillis() + timeout + m_transport->getLatency();
  const uint8_t* data = nullptr;
  size_t len = 0;
  result_t result;
  do {
    result = m_transport->read(timeout, &data, &len);
    if (result == RESULT_OK) {
      result = handleEnhancedBufferedData(data, len, value, arbitrationState);
      if (result >= RESULT_OK) {
        break;
      }
    }
    if (result != RESULT_ERR_TIMEOUT) {
      cancelRunningArbitration(arbitrationState);
      return result;
    }
    if (timeout == 0) {
      break;
    }
    uint64_t now = clockGetMillis();
    if (timeout == 0 || now >= until) {
      break;
    }
    timeout = static_cast<unsigned>(until - now);
  } while (true);
  return result;
}

result_t EnhancedDevice::startArbitration(symbol_t masterAddress) {
  if (m_arbitrationCheck) {
    if (masterAddress != SYN) {
      return RESULT_ERR_ARB_RUNNING;  // should not occur
    }
    if (!cancelRunningArbitration(nullptr)) {
      return RESULT_ERR_SEND;
    }
    return RESULT_OK;
  }
  m_arbitrationMaster = masterAddress;
  if (masterAddress != SYN) {
    uint8_t buf[] = makeEnhancedSequence(ENH_REQ_START, masterAddress);
    result_t result = m_transport->write(buf, 2);
    if (result != RESULT_OK) {
      m_arbitrationMaster = SYN;
      return result;
    }
    m_arbitrationCheck = 1;
  }
  return RESULT_OK;
}

bool EnhancedDevice::cancelRunningArbitration(ArbitrationState* arbitrationState) {
  if (!BaseDevice::cancelRunningArbitration(arbitrationState)) {
    return false;
  }
  symbol_t buf[2] = makeEnhancedSequence(ENH_REQ_START, SYN);
  return m_transport->write(buf, 2) == RESULT_OK;
}

result_t EnhancedDevice::notifyTransportStatus(bool opened) {
  result_t result = BaseDevice::notifyTransportStatus(opened);  // always OK
  if (opened) {
    symbol_t buf[2] = makeEnhancedSequence(ENH_REQ_INIT, REQUEST_FEATURES);  // extra feature: info
    result = m_transport->write(buf, 2);
    if (result != RESULT_OK) {
      return result;
    }
    m_resetTime = time(NULL);
    m_resetRequested = true;
  } else {
    // reset state
    m_resetTime = 0;
    m_extraFeatures = 0;
    m_infoLen = 0;
    m_enhInfoVersion = "";
    m_enhInfoIsWifi = false;
    m_enhInfoTemperature = "";
    m_enhInfoSupplyVoltage = "";
    m_enhInfoBusVoltage = "";
    m_arbitrationMaster = SYN;
    m_arbitrationCheck = 0;
  }
  return result;
}

result_t EnhancedDevice::handleEnhancedBufferedData(const uint8_t* data, size_t len,
symbol_t* value, ArbitrationState* arbitrationState) {
  bool valueSet = false;
  bool sent = false;
  bool more = false;
  size_t pos;
  for (pos = 0; pos < len; pos++) {
    symbol_t ch = data[pos];
    if (!(ch&ENH_BYTE_FLAG)) {
      if (valueSet) {
        more = true;
        break;
      }
      *value = ch;
      valueSet = true;
      continue;
    }
    uint8_t kind = ch&ENH_BYTE_MASK;
    if (kind == ENH_BYTE1 && len < pos + 2) {
      break;  // transfer not complete yet
    }
    if (kind == ENH_BYTE2) {
      if (m_listener != nullptr) {
        m_listener->notifyDeviceStatus(true, "unexpected enhanced byte 2");
      }
      continue;
    }
    // kind is ENH_BYTE1
    pos++;
    symbol_t ch2 = data[pos];
    if ((ch2 & ENH_BYTE_MASK) != ENH_BYTE2) {
      if (m_listener != nullptr) {
        m_listener->notifyDeviceStatus(true, "missing enhanced byte 2");
      }
      continue;
    }
    symbol_t data = (symbol_t)(((ch&0x03) << 6) | (ch2&0x3f));
    symbol_t cmd = (ch >> 2)&0xf;
    switch (cmd) {
      case ENH_RES_STARTED:
      case ENH_RES_FAILED:
        if (valueSet) {
          more = true;
          pos--;  // keep ENH_BYTE1 for later run
          len = 0;  // abort outer loop
          break;
        }
        sent = cmd == ENH_RES_STARTED;
        if (arbitrationState) {
          *arbitrationState = sent ? as_won : as_lost;
        }
        m_arbitrationMaster = SYN;
        m_arbitrationCheck = 0;
        *value = data;
        valueSet = true;
        break;
      case ENH_RES_RECEIVED:
        if (valueSet) {
          more = true;
          pos--;  // keep ENH_BYTE1 for later run
          len = 0;  // abort outer loop
          break;
        }
        *value = data;
        if (data == SYN && arbitrationState && *arbitrationState == as_running && m_arbitrationCheck) {
          if (m_arbitrationCheck < 3) {  // wait for three SYN symbols before switching to timeout
            m_arbitrationCheck++;
          } else {
            *arbitrationState = as_timeout;
            m_arbitrationMaster = SYN;
            m_arbitrationCheck = 0;
          }
        }
        valueSet = true;
        break;
      case ENH_RES_RESETTED:
        if (arbitrationState && *arbitrationState != as_none) {
          *arbitrationState = as_error;
          m_arbitrationMaster = SYN;
          m_arbitrationCheck = 0;
        }
        m_enhInfoTemperature = "";
        m_enhInfoSupplyVoltage = "";
        m_enhInfoBusVoltage = "";
        m_infoLen = 0;
        if (!m_resetRequested && m_resetTime+3 >= time(NULL)) {
          if (data == m_extraFeatures) {
            // skip explicit response to init request
            valueSet = false;
            break;
          }
          // response to init request had different feature flags
          m_resetRequested = true;
        }
        m_extraFeatures = data;
        if (m_listener != nullptr) {
          m_listener->notifyDeviceStatus(false, (m_extraFeatures&0x01) ? "reset, supports info" : "reset");
        }
        if (m_resetRequested) {
          m_resetRequested = false;
          if (m_extraFeatures&0x01) {
            requestEnhancedInfo(0, false);  // request version, ignore result
          }
          valueSet = false;
          break;
        }
        m_transport->close();  // on self-reset of device close and reopen it to have a clean startup
        cancelRunningArbitration(arbitrationState);
        break;
      case ENH_RES_INFO:
        if (m_infoLen == 1) {
          m_infoLen = data+1;
        } else if (m_infoLen && m_infoPos < m_infoLen && m_infoPos < sizeof(m_infoBuf)) {
          m_infoBuf[m_infoPos++] = data;
          if (m_infoPos >= m_infoLen) {
            notifyInfoRetrieved();
            m_infoLen = 0;
          }
        } else {
          m_infoLen = 0;  // reset on invalid response
        }
        break;
      case ENH_RES_ERROR_EBUS:
      case ENH_RES_ERROR_HOST:
        if (m_listener != nullptr) {
          ostringstream stream;
          stream << (cmd == ENH_RES_ERROR_EBUS ? "eBUS comm error: " : "host comm error: ");
          switch (data) {
            case ENH_ERR_FRAMING:
              stream << "framing";
              break;
            case ENH_ERR_OVERRUN:
              stream << "overrun";
              break;
            default:
              stream << "unknown 0x" << setw(2) << setfill('0') << hex << static_cast<unsigned>(data);
              break;
          }
          string str = stream.str();
          m_listener->notifyDeviceStatus(true, str.c_str());
        }
        cancelRunningArbitration(arbitrationState);
        break;
      default:
        if (m_listener != nullptr) {
          ostringstream stream;
          stream << "unexpected enhanced command 0x" << setw(2) << setfill('0') << hex
                 << static_cast<unsigned>(cmd);
          string str = stream.str();
          m_listener->notifyDeviceStatus(true, str.c_str());
        }
        len = 0;  // abort outer loop
        break;
    }
    if (len == 0) {
      break;  // abort received
    }
  }
  m_transport->readConsumed(pos);
  if (valueSet && m_listener != nullptr) {
    m_listener->notifyDeviceData(value, 1, !sent);
  }
  return more ? RESULT_CONTINUE : valueSet ? RESULT_OK : RESULT_ERR_TIMEOUT;
}

void EnhancedDevice::notifyInfoRetrieved() {
  symbol_t id = m_infoBuf[0];
  symbol_t* data = m_infoBuf+1;
  size_t len = m_infoLen-1;
  unsigned int val;
  ostringstream stream;
  switch ((len << 8) | id) {
    case 0x0200:
    case 0x0500:  // with firmware version and jumper info
    case 0x0800:  // with firmware version, jumper info, and bootloader version
      stream << hex << static_cast<unsigned>(data[1])  // features mask
             << "." << static_cast<unsigned>(data[0]);  // version minor
      if (len >= 5) {
        stream << "[" << setfill('0') << setw(2) << hex << static_cast<unsigned>(data[2])
                << setw(2) << static_cast<unsigned>(data[3]) << "]";
      }
      if (len >= 8) {
        stream << "." << dec << static_cast<unsigned>(data[5]);
        stream << "[" << setfill('0') << setw(2) << hex << static_cast<unsigned>(data[6])
               << setw(2) << static_cast<unsigned>(data[7]) << "]";
      }
      m_enhInfoVersion = stream.str();
      stream.str(" ");
      stream << "firmware " << m_enhInfoVersion;
      if (len >= 5) {
        stream << ", jumpers 0x" << setw(2) << static_cast<unsigned>(data[4]);
        m_enhInfoIsWifi = (data[4]&0x08) != 0;
      }
      stream << setfill(' ');  // reset
      break;
    case 0x0901:
    case 0x0802:
    case 0x0302:
      stream << (id == 1 ? "ID" : "config");
      stream << hex << setfill('0');
      for (size_t pos = 0; pos < len; pos++) {
        stream << " " << setw(2) << static_cast<unsigned>(data[pos]);
      }
      if (id == 2 && (data[2]&0x3f) != 0x3f) {
        // non-default arbitration delay
        val = (data[2]&0x3f)*10;  // steps of 10us
        stream << ", arbitration delay " << dec << static_cast<unsigned>(val) << " us";
      }
      break;
    case 0x0203:
      val = (static_cast<unsigned>(data[0]) << 8) | static_cast<unsigned>(data[1]);
      stream << "temperature " << static_cast<unsigned>(val) << " °C";
      m_enhInfoTemperature = stream.str();
      break;
    case 0x0204:
      stream << "supply voltage ";
      if (data[0] | data[1]) {
        val = (static_cast<unsigned>(data[0]) << 8) | static_cast<unsigned>(data[1]);
        stream << static_cast<unsigned>(val) << " mV";
      } else {
        stream << "unknown";
      }
      m_enhInfoSupplyVoltage = stream.str();
      break;
    case 0x0205:
      stream << "bus voltage ";
      if (data[0] | data[1]) {
        stream << fixed << setprecision(1)
               << static_cast<float>(data[1] / 10.0) << " V - "
               << static_cast<float>(data[0] / 10.0) << " V";
      } else {
        stream << "unknown";
      }
      m_enhInfoBusVoltage = stream.str();
      break;
    case 0x0206:
      stream << "reset cause ";
      if (data[0]) {
        stream << static_cast<unsigned>(data[0]) << "=";
        switch (data[0]) {
          case 1: stream << "power-on"; break;
          case 2: stream << "brown-out"; break;
          case 3: stream << "watchdog"; break;
          case 4: stream << "clear"; break;
          case 5: stream << "reset"; break;
          case 6: stream << "stack"; break;
          case 7: stream << "memory"; break;
          default: stream << "other"; break;
        }
        stream << ", restart count " << static_cast<unsigned>(data[1]);
      } else {
        stream << "unknown";
      }
      break;
    case 0x0107:
      stream << "rssi ";
      if (data[0]) {
        stream << static_cast<signed>(((int8_t*)data)[0]) << " dBm";
      } else {
        stream << "unknown";
      }
      break;
    default:
      stream << "unknown 0x" << hex << setfill('0') << setw(2)
             << static_cast<unsigned>(id) << ", len " << dec << setw(0)
             << static_cast<unsigned>(len);
      break;
  }
  if (m_listener != nullptr) {
    m_listener->notifyDeviceStatus(false, ("extra info: "+stream.str()).c_str());
  }
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2022 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/device.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#ifdef HAVE_LINUX_SERIAL
#  include <linux/serial.h>
#endif
#ifdef HAVE_FREEBSD_UFTDI
#  include <dev/usb/uftdiio.h>
#endif
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <ios>
#include <iomanip>
#include "lib/ebus/data.h"
#include "lib/utils/clock.h"
#include "lib/utils/tcpsocket.h"

namespace ebusd {

#define MTU 1540

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

// ebusd enhanced protocol IDs:
#define ENH_REQ_INIT ((uint8_t)0x0)
#define ENH_RES_RESETTED ((uint8_t)0x0)
#define ENH_REQ_SEND ((uint8_t)0x1)
#define ENH_RES_RECEIVED ((uint8_t)0x1)
#define ENH_REQ_START ((uint8_t)0x2)
#define ENH_RES_STARTED ((uint8_t)0x2)
#define ENH_REQ_INFO ((uint8_t)0x3)
#define ENH_RES_INFO ((uint8_t)0x3)
#define ENH_RES_FAILED ((uint8_t)0xa)
#define ENH_RES_ERROR_EBUS ((uint8_t)0xb)
#define ENH_RES_ERROR_HOST ((uint8_t)0xc)

// ebusd enhanced error codes for the ERROR_* responses
#define ENH_ERR_FRAMING ((uint8_t)0x00)
#define ENH_ERR_OVERRUN ((uint8_t)0x01)

#define ENH_BYTE_FLAG ((uint8_t)0x80)
#define ENH_BYTE_MASK ((uint8_t)0xc0)
#define ENH_BYTE1 ((uint8_t)0xc0)
#define ENH_BYTE2 ((uint8_t)0x80)
#define makeEnhancedSequence(cmd, data) {(uint8_t)(ENH_BYTE1 | ((cmd)<<2) | (((data)&0xc0)>>6)), (uint8_t)(ENH_BYTE2 | ((data)&0x3f))}

Device::Device(const char* name, bool checkDevice, unsigned int latency, bool readOnly, bool initialSend,
    bool enhancedProto)
  : m_name(name), m_checkDevice(checkDevice),
    m_latency(HOST_LATENCY_MS+(enhancedProto?ENHANCED_LATENCY_MS:0)+latency), m_readOnly(readOnly),
    m_initialSend(initialSend), m_enhancedProto(enhancedProto), m_fd(-1), m_resetRequested(false),
    m_listener(nullptr), m_arbitrationMaster(SYN),
    m_arbitrationCheck(0), m_bufSize(((MAX_LEN+1+3)/4)*4), m_bufLen(0), m_bufPos(0),
    m_extraFatures(0), m_infoId(0xff), m_infoReqTime(0), m_infoLen(0), m_infoPos(0) {
  m_buffer = reinterpret_cast<symbol_t*>(malloc(m_bufSize));
  if (!m_buffer) {
    m_bufSize = 0;
  }
}

Device::~Device() {
  close();
  if (m_buffer) {
    free(m_buffer);
  }
}

Device* Device::create(const char* name, unsigned int extraLatency, bool checkDevice, bool readOnly, bool initialSend) {
  bool highSpeed = strncmp(name, "ens:", 4) == 0;
  bool enhanced = highSpeed || strncmp(name, "enh:", 4) == 0;
  if (enhanced) {
    name += 4;
  }
  if (strchr(name, '/') == nullptr && strchr(name, ':') != nullptr) {
    char* in = strdup(name);
    bool udp = false;
    char* addrpos = in;
    char* portpos = strchr(addrpos, ':');
    if (!enhanced && portpos >= addrpos+3 && strncmp(addrpos, "enh", 3) == 0) {
      enhanced = true;  // support enhtcp:<ip>:<port> and enhudp:<ip>:<port>
      addrpos += 3;
      if (portpos == addrpos) {
        addrpos++;
        portpos = strchr(addrpos, ':');
      }
    }  // else: support enh:<ip>:<port> defaulting to TCP
    if (portpos == addrpos+3 && (strncmp(addrpos, "tcp", 3) == 0 || (udp=(strncmp(addrpos, "udp", 3) == 0)))) {
      addrpos += 4;
      portpos = strchr(addrpos, ':');
    }
    if (portpos == nullptr) {
      free(in);
      return nullptr;  // invalid protocol or missing port
    }
    result_t result = RESULT_OK;
    uint16_t port = (uint16_t)parseInt(portpos+1, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      free(in);
      return nullptr;  // invalid port
    }
    *portpos = 0;
    char* hostOrIp = strdup(addrpos);
    free(in);
    return new NetworkDevice(name, hostOrIp, port, extraLatency, readOnly, initialSend, udp, enhanced);
  }
  // support enh:/dev/<device>, ens:/dev/<device>, and /dev/<device>
  return new SerialDevice(name, checkDevice, extraLatency, readOnly, initialSend, enhanced, highSpeed);
}

result_t Device::open() {
  close();
  return m_bufSize == 0 ? RESULT_ERR_DEVICE : RESULT_OK;
}

result_t Device::afterOpen() {
  m_bufLen = 0;
  m_extraFatures = 0;
  if (m_enhancedProto) {
    symbol_t buf[2] = makeEnhancedSequence(ENH_REQ_INIT, 0x01);  // extra feature: info
#ifdef DEBUG_RAW_TRAFFIC
    fprintf(stdout, "raw enhanced > %2.2x %2.2x\n", buf[0], buf[1]);
    fflush(stdout);
#endif
    if (::write(m_fd, buf, 2) != 2) {
      return RESULT_ERR_SEND;
    }
    if (m_listener != nullptr) {
      m_listener->notifyStatus(false, "resetting");
    }
    m_resetRequested = true;
  } else if (m_initialSend && !write(ESC)) {
    return RESULT_ERR_SEND;
  }
  return RESULT_OK;
}

void Device::close() {
  if (m_fd != -1) {
    ::close(m_fd);
    m_fd = -1;
  }
  m_bufLen = 0;  // flush read buffer
}

bool Device::isValid() {
  if (m_fd == -1) {
    return false;
  }
  if (m_checkDevice) {
    checkDevice();
  }
  return m_fd != -1;
}

result_t Device::requestEnhancedInfo(symbol_t infoId) {
  if (!m_enhancedProto || m_extraFatures == 0) {
    return RESULT_ERR_INVALID_ARG;
  }
  for (unsigned int i = 0; i < 4; i++) {
    if (m_infoId == 0xff) {
      break;
    }
    usleep(40000 + i*40000);
  }
  if (m_infoId != 0xff) {
    if (m_infoReqTime > 0 && time(NULL) > m_infoReqTime+5) {
      // request timed out
      if (m_listener != nullptr) {
        m_listener->notifyStatus(false, "info request timed out");
      }
      m_infoId = 0xff;
      m_infoReqTime = 0;
    } else {
      return RESULT_ERR_DUPLICATE;
    }
  }
  if (infoId == 0xff) {
    // just waited for completion
    return RESULT_OK;
  }
  return sendEnhancedInfoRequest(infoId);
}

result_t Device::sendEnhancedInfoRequest(symbol_t infoId) {
  symbol_t buf[2] = makeEnhancedSequence(ENH_REQ_INFO, infoId);
#ifdef DEBUG_RAW_TRAFFIC
  fprintf(stdout, "raw enhanced > %2.2x %2.2x\n", buf[0], buf[1]);
  fflush(stdout);
#endif
  if (::write(m_fd, buf, 2) != 2) {
    return RESULT_ERR_DEVICE;
  }
  m_infoPos = 0;
  m_infoId = infoId;
  time(&m_infoReqTime);
  return RESULT_OK;
}

string Device::getEnhancedInfos() {
  if (!m_enhancedProto || m_extraFatures == 0) {
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
      m_infoPos = 0;
      m_infoId = 0xff;
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
  res = requestEnhancedInfo(0xff);  // wait for completion
  if (res != RESULT_OK) {
    m_enhInfoBusVoltage = "bus voltage unknown";
    m_infoPos = 0;
    m_infoId = 0xff;
  }
  return "firmware " + m_enhInfoVersion + ", " + m_enhInfoTemperature + ", " + m_enhInfoSupplyVoltage + ", "
  + m_enhInfoBusVoltage;
}

result_t Device::send(symbol_t value) {
  if (!isValid()) {
    return RESULT_ERR_DEVICE;
  }
  if (m_readOnly || !write(value)) {
    return RESULT_ERR_SEND;
  }
  if (m_listener != nullptr) {
    m_listener->notifyDeviceData(value, false);
  }
  return RESULT_OK;
}

/**
 * the maximum duration in milliseconds to wait for an enhanced sequence to complete after the first part was already
 * retrieved (3ms rounded up to the next 10ms): 2* (Start+8Bit+Stop+Extra @ 9600Bd)
 */
#define ENHANCED_COMPLETE_WAIT_DURATION 10


bool Device::cancelRunningArbitration(ArbitrationState* arbitrationState) {
  if (m_enhancedProto && m_arbitrationMaster != SYN) {
    *arbitrationState = as_error;
    m_arbitrationMaster = SYN;
    m_arbitrationCheck = 0;
    write(SYN, true);
    return true;
  }
  if (m_enhancedProto || m_arbitrationMaster == SYN) {
    return false;
  }
  *arbitrationState = as_error;
  m_arbitrationMaster = SYN;
  m_arbitrationCheck = 0;
  return true;
}

result_t Device::recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) {
  if (m_arbitrationMaster != SYN) {
    *arbitrationState = as_running;
  }
  if (!isValid()) {
    cancelRunningArbitration(arbitrationState);
    return RESULT_ERR_DEVICE;
  }
  bool repeated = false;
  timeout += m_latency;
  uint64_t until = clockGetMillis() + timeout;
  do {
    bool isAvailable = available();
    if (!isAvailable && timeout > 0) {
      int ret;
      struct timespec tdiff;

      // set select timeout
      tdiff.tv_sec = timeout/1000;
      tdiff.tv_nsec = (timeout%1000)*1000000;

#ifdef HAVE_PPOLL
      nfds_t nfds = 1;
      struct pollfd fds[nfds];

      memset(fds, 0, sizeof(fds));

      fds[0].fd = m_fd;
      fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
      ret = ppoll(fds, nfds, &tdiff, nullptr);
      if (ret >= 0 && fds[0].revents & (POLLERR | POLLHUP | POLLRDHUP)) {
        ret = -1;
      }
#else
#ifdef HAVE_PSELECT
      fd_set readfds, exceptfds;

      FD_ZERO(&readfds);
      FD_ZERO(&exceptfds);
      FD_SET(m_fd, &readfds);

      ret = pselect(m_fd + 1, &readfds, nullptr, &exceptfds, &tdiff, nullptr);
      if (ret >= 1 && FD_ISSET(m_fd, &exceptfds)) {
        ret = -1;
      }
#else
      ret = 1;  // ignore timeout if neither ppoll nor pselect are available
#endif
#endif
      if (ret == -1) {
#ifdef DEBUG_RAW_TRAFFIC
        fprintf(stdout, "poll error %d\n", errno);
#endif
        close();
        cancelRunningArbitration(arbitrationState);
        return RESULT_ERR_DEVICE;
      }
      if (ret == 0) {
        return RESULT_ERR_TIMEOUT;
      }
    }

    // directly read byte from device
    bool incomplete = false;
    if (read(value, isAvailable, arbitrationState, &incomplete)) {
      break;  // don't repeat on successful read
    }
    if (!isAvailable && incomplete && !repeated) {
      // for a two-byte transfer another poll is needed
      repeated = true;
      timeout = m_latency+ENHANCED_COMPLETE_WAIT_DURATION;
      continue;
    }
    uint64_t now = clockGetMillis();
    if (now >= until) {
      return RESULT_ERR_TIMEOUT;
    }
    timeout = static_cast<unsigned>(until - now);
  } while (true);
  if (m_enhancedProto || *value != SYN || m_arbitrationMaster == SYN || m_arbitrationCheck) {
    if (m_listener != nullptr) {
      m_listener->notifyDeviceData(*value, true);
    }
    if (!m_enhancedProto && m_arbitrationMaster != SYN) {
      if (m_arbitrationCheck) {
        *arbitrationState = *value == m_arbitrationMaster ? as_won : as_lost;
        m_arbitrationMaster = SYN;
        m_arbitrationCheck = 0;
      } else {
        *arbitrationState = m_arbitrationMaster == SYN ? as_none : as_start;
      }
    }
    return RESULT_OK;
  }
  // non-enhanced: arbitration executed by ebusd itself
  bool wrote = write(m_arbitrationMaster);  // send as fast as possible
  if (m_listener != nullptr) {
    m_listener->notifyDeviceData(*value, true);
  }
  if (!wrote) {
    cancelRunningArbitration(arbitrationState);
    return RESULT_OK;
  }
  if (m_listener != nullptr) {
    m_listener->notifyDeviceData(m_arbitrationMaster, false);
  }
  m_arbitrationCheck = 1;
  *arbitrationState = as_running;
  return RESULT_OK;
}

result_t Device::startArbitration(symbol_t masterAddress) {
  if (m_arbitrationCheck) {
    if (masterAddress != SYN) {
      return RESULT_ERR_ARB_RUNNING;  // should not occur
    }
    m_arbitrationCheck = 0;
    m_arbitrationMaster = SYN;
    if (m_enhancedProto) {
      // cancel running arbitration
      if (!write(SYN, true)) {
        return RESULT_ERR_SEND;
      }
    }
    return RESULT_OK;
  }
  if (m_readOnly) {
    return RESULT_ERR_SEND;
  }
  m_arbitrationMaster = masterAddress;
  if (m_enhancedProto && masterAddress != SYN) {
    if (!write(masterAddress, true)) {
      m_arbitrationMaster = SYN;
      return RESULT_ERR_SEND;
    }
    m_arbitrationCheck = 1;
  }
  return RESULT_OK;
}

bool Device::write(symbol_t value, bool startArbitration) {
  if (m_enhancedProto) {
    symbol_t buf[2] = makeEnhancedSequence(startArbitration ? ENH_REQ_START : ENH_REQ_SEND, value);
#ifdef DEBUG_RAW_TRAFFIC
    fprintf(stdout, "raw enhanced > %2.2x %2.2x\n", buf[0], buf[1]);
    fflush(stdout);
#endif
    return ::write(m_fd, buf, 2) == 2;
  }
#ifdef DEBUG_RAW_TRAFFIC
  fprintf(stdout, "raw > %2.2x\n", value);
  fflush(stdout);
#endif
#ifdef SIMULATE_NON_WRITABILITY
  return true;
#else
  return ::write(m_fd, &value, 1) == 1;
#endif
}

bool Device::available() {
  if (m_bufLen <= 0) {
    return false;
  }
  if (!m_enhancedProto) {
    return true;
  }
  // peek into the received enhanced proto bytes to determine received bus symbol availability
  for (size_t pos = 0; pos < m_bufLen; pos++) {
    symbol_t ch = m_buffer[(pos+m_bufPos)%m_bufSize];
    if (!(ch&ENH_BYTE_FLAG)) {
#ifdef DEBUG_RAW_TRAFFIC
      fprintf(stdout, "raw avail direct @%d+%d %2.2x\n", m_bufPos, pos, ch);
      fflush(stdout);
#endif
      return true;
    }
    if ((ch&ENH_BYTE_MASK) == ENH_BYTE1) {
      if (pos+1 >= m_bufLen) {
        return false;
      }
      symbol_t cmd = (ch >> 2)&0xf;
      // peek into next byte to check if enhanced sequence is ok
      ch = m_buffer[(pos+m_bufPos+1)%m_bufSize];
      if (!(ch&ENH_BYTE_FLAG) || (ch&ENH_BYTE_MASK) != ENH_BYTE2) {
#ifdef DEBUG_RAW_TRAFFIC
        fprintf(stdout, "raw avail enhanced following bad @%d+%d %2.2x %2.2x\n", m_bufPos, pos,
          m_buffer[(pos+m_bufPos)%m_bufSize], ch);
        fflush(stdout);
#endif
        if (m_listener != nullptr) {
          m_listener->notifyStatus(true, "unexpected available enhanced following byte 1");
        }
        // drop first byte of invalid sequence
        m_bufPos = (m_bufPos + 1) % m_bufSize;
        m_bufLen--;
        pos--;  // check same pos again
        continue;
      }
      if (cmd == ENH_RES_RECEIVED || cmd == ENH_RES_STARTED || cmd == ENH_RES_FAILED) {
        // found a sequence that yields in available bus byte
#ifdef DEBUG_RAW_TRAFFIC
        fprintf(stdout, "raw avail enhanced @%d+%d %2.2x %2.2x\n", m_bufPos, pos, m_buffer[(pos+m_bufPos)%m_bufSize], ch);
        fflush(stdout);
#endif
        return true;
      }
#ifdef DEBUG_RAW_TRAFFIC
      fprintf(stdout, "raw avail enhanced skip cmd %d @%d+%d %2.2x\n", cmd, m_bufPos, pos, ch);
      fflush(stdout);
#endif
      pos++;  // skip enhanced sequence of 2 bytes
      continue;
    }
#ifdef DEBUG_RAW_TRAFFIC
    fprintf(stdout, "raw avail enhanced bad @%d+%d %2.2x\n", m_bufPos, pos, ch);
    fflush(stdout);
#endif
    if (m_listener != nullptr) {
      m_listener->notifyStatus(true, "unexpected available enhanced byte 2");
    }
    // skip byte from erroneous protocol
    m_bufPos = (m_bufPos+1)%m_bufSize;
    m_bufLen--;
    pos--;  // check byte 2 again from scratch and allow as byte 1
  }
  return false;
}

bool Device::read(symbol_t* value, bool isAvailable, ArbitrationState* arbitrationState, bool* incomplete) {
  if (!isAvailable) {
    if (m_bufLen > 0 && m_bufPos != 0) {
      if (m_bufLen > m_bufSize / 2) {
        // more than half of input buffer consumed is taken as signal that ebusd is too slow
        m_bufLen = 0;
        if (m_listener != nullptr) {
          m_listener->notifyStatus(true, "buffer overflow");
        }
      } else {
        size_t tail;
        if (m_bufPos+m_bufLen > m_bufSize) {
          // move wrapped tail away
          tail = (m_bufPos+m_bufLen) % m_bufSize;
          size_t head = m_bufLen-tail;
          memmove(m_buffer+head, m_buffer, tail);
#ifdef DEBUG_RAW_TRAFFIC
          fprintf(stdout, "raw move tail %d @0 to @%d\n", tail, head);
          fflush(stdout);
#endif
        } else {
          tail = 0;
        }
        // move head to first position
        memmove(m_buffer, m_buffer + m_bufPos, m_bufLen - tail);
#ifdef DEBUG_RAW_TRAFFIC
        fprintf(stdout, "raw move head %d @%d to 0\n", m_bufLen - tail, m_bufPos);
        fflush(stdout);
#endif
      }
    }
    m_bufPos = 0;
    // fill up the buffer
    ssize_t size = ::read(m_fd, m_buffer + m_bufLen, m_bufSize - m_bufLen);
    if (size <= 0) {
      return false;
    }
#ifdef DEBUG_RAW_TRAFFIC
    fprintf(stdout, "raw %ld+%ld <", m_bufLen, size);
    for (int pos=0; pos < size; pos++) {
      fprintf(stdout, " %2.2x", m_buffer[(m_bufLen+pos)%m_bufSize]);
    }
    fprintf(stdout, "\n");
    fflush(stdout);
#endif
    m_bufLen += size;
  }
  if (m_enhancedProto) {
    if (handleEnhancedBufferedData(value, arbitrationState)) {
      return true;
    }
  }
  if (!available()) {
    if (incomplete) {
      *incomplete = m_enhancedProto && m_bufLen > 0;
    }
    return false;
  }
  if (!m_enhancedProto) {
    *value = m_buffer[m_bufPos];
    m_bufPos = (m_bufPos+1)%m_bufSize;
    m_bufLen--;
    return true;
  }
  return handleEnhancedBufferedData(value, arbitrationState);
}

bool Device::handleEnhancedBufferedData(symbol_t* value, ArbitrationState* arbitrationState) {
  while (m_bufLen > 0) {
    symbol_t ch = m_buffer[m_bufPos];
    if (!(ch&ENH_BYTE_FLAG)) {
      *value = ch;
      m_bufPos = (m_bufPos+1)%m_bufSize;
      m_bufLen--;
      return true;
    }
    uint8_t kind = ch&ENH_BYTE_MASK;
    if (kind == ENH_BYTE1 && m_bufLen < 2) {
      return false;  // transfer not complete yet
    }
    m_bufPos = (m_bufPos+1)%m_bufSize;
    m_bufLen--;
    if (kind == ENH_BYTE2) {
      if (m_listener != nullptr) {
        m_listener->notifyStatus(true, "unexpected enhanced byte 2");
      }
      return false;
    }
    // kind is ENH_BYTE1
    symbol_t ch2 = m_buffer[m_bufPos];
    m_bufPos = (m_bufPos + 1) % m_bufSize;
    m_bufLen--;
    if ((ch2 & ENH_BYTE_MASK) != ENH_BYTE2) {
      if (m_listener != nullptr) {
        m_listener->notifyStatus(true, "missing enhanced byte 2");
      }
      return false;
    }
    symbol_t data = (symbol_t)(((ch&0x03) << 6) | (ch2&0x3f));
    symbol_t cmd = (ch >> 2)&0xf;
    switch (cmd) {
      case ENH_RES_STARTED:
        *arbitrationState = as_won;
        if (m_listener != NULL) {
          m_listener->notifyDeviceData(data, false);
        }
        m_arbitrationMaster = SYN;
        m_arbitrationCheck = 0;
        *value = data;
        return true;
      case ENH_RES_FAILED:
        *arbitrationState = as_lost;
        if (m_listener != NULL) {
          m_listener->notifyDeviceData(m_arbitrationMaster, false);
        }
        m_arbitrationMaster = SYN;
        m_arbitrationCheck = 0;
        *value = data;
        return true;
      case ENH_RES_RECEIVED:
        *value = data;
        if (data == SYN && *arbitrationState == as_running && m_arbitrationCheck) {
          if (m_arbitrationCheck < 3) {  // wait for three SYN symbols before switching to timeout
            m_arbitrationCheck++;
          } else {
            *arbitrationState = as_timeout;
            m_arbitrationMaster = SYN;
            m_arbitrationCheck = 0;
          }
        }
        return true;
      case ENH_RES_RESETTED:
        if (*arbitrationState != as_none) {
          *arbitrationState = as_error;
          m_arbitrationMaster = SYN;
          m_arbitrationCheck = 0;
        }
        m_enhInfoTemperature = "";
        m_enhInfoSupplyVoltage = "";
        m_enhInfoBusVoltage = "";
        m_infoId = 0xff;
        m_extraFatures = data;
        if (m_resetRequested) {
          m_resetRequested = false;
          if (m_extraFatures&0x01) {
            sendEnhancedInfoRequest(0);  // request version, ignore result
          }
        } else {
          close();  // on self-reset of device close and reopen it to have a clean startup
          cancelRunningArbitration(arbitrationState);
        }
        if (m_listener != nullptr) {
          m_listener->notifyStatus(false, (m_extraFatures&0x01) ? "reset, supports info" : "reset");
        }
        break;
      case ENH_RES_INFO:
        if (m_infoLen == 0) {
          m_infoLen = data;
          m_infoPos = 0;
        } else if (m_infoPos < m_infoLen && m_infoPos < sizeof(m_infoBuf)) {
          m_infoBuf[m_infoPos++] = data;
          if (m_infoPos >= m_infoLen) {
            unsigned int val;
            ostringstream stream;
            switch ((m_infoLen << 8) | m_infoId) {
              case 0x0200:
              case 0x0500:  // with firmware version and jumper info
              case 0x0800:  // with firmware version, jumper info, and bootloader version
                stream << std::hex << static_cast<unsigned>(m_infoBuf[1])  // features mask
                       << "." << static_cast<unsigned>(m_infoBuf[0]);  // version minor
                if (m_infoLen >= 5) {
                  stream << "[" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned>(m_infoBuf[2])
                         << std::setw(2) << static_cast<unsigned>(m_infoBuf[3]) << "]";
                }
                if (m_infoLen >= 8) {
                  stream << "." << std::dec << static_cast<unsigned>(m_infoBuf[5]);
                  stream << "[" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned>(m_infoBuf[6])
                         << std::setw(2) << static_cast<unsigned>(m_infoBuf[7]) << "]";
                }
                m_enhInfoVersion = stream.str();
                stream.str(" ");
                stream << "firmware " << m_enhInfoVersion;
                if (m_infoLen >= 5) {
                  stream << ", jumpers 0x" << std::setw(2) << static_cast<unsigned>(m_infoBuf[4]);
                }
                stream << std::setfill(' ');  // reset
                break;
              case 0x0901:
              case 0x0802:
              case 0x0302:
                stream << (m_infoId == 1 ? "ID" : "config");
                stream << std::hex << std::setfill('0');
                for (uint8_t pos = 0; pos < m_infoPos; pos++) {
                  stream << " " << std::setw(2) << static_cast<unsigned>(m_infoBuf[pos]);
                }
                if (m_infoId == 2 && (m_infoBuf[2]&0x3f) != 0x3f) {
                  // non-default arbitration delay
                  val = (m_infoBuf[2]&0x3f)*10;  // steps of 10us
                  stream << ", arbitration delay " << std::dec << static_cast<unsigned>(val) << " us";
                }
                break;
              case 0x0203:
                val = (static_cast<unsigned>(m_infoBuf[0]) << 8) | static_cast<unsigned>(m_infoBuf[1]);
                stream << "temperature " << static_cast<unsigned>(val) << " °C";
                m_enhInfoTemperature = stream.str();
                break;
              case 0x0204:
                stream << "supply voltage ";
                if (m_infoBuf[0] | m_infoBuf[1]) {
                  val = (static_cast<unsigned>(m_infoBuf[0]) << 8) | static_cast<unsigned>(m_infoBuf[1]);
                  stream << static_cast<unsigned>(val) << " mV";
                } else {
                  stream << "unknown";
                }
                m_enhInfoSupplyVoltage = stream.str();
                break;
              case 0x0205:
                stream << "bus voltage ";
                if (m_infoBuf[0] | m_infoBuf[1]) {
                  stream << std::fixed << std::setprecision(1)
                         << static_cast<float>(m_infoBuf[1] / 10.0) << " V - "
                         << static_cast<float>(m_infoBuf[0] / 10.0) << " V";
                } else {
                  stream << "unknown";
                }
                m_enhInfoBusVoltage = stream.str();
                break;
              case 0x0206:
                stream << "reset cause ";
                if (m_infoBuf[0]) {
                  stream << static_cast<unsigned>(m_infoBuf[0]) << "=";
                  switch (m_infoBuf[0]) {
                    case 1: stream << "power-on"; break;
                    case 2: stream << "brown-out"; break;
                    case 3: stream << "watchdog"; break;
                    case 4: stream << "clear"; break;
                    case 5: stream << "reset"; break;
                    case 6: stream << "stack"; break;
                    case 7: stream << "memory"; break;
                    default: stream << "other"; break;
                  }
                  stream << ", restart count " << static_cast<unsigned>(m_infoBuf[1]);
                } else {
                  stream << "unknown";
                }
                break;
              default:
                stream << "unknown 0x" << std::hex << std::setfill('0') << std::setw(2)
                       << static_cast<unsigned>(m_infoId) << ", len " << std::dec << std::setw(0)
                       << static_cast<unsigned>(m_infoPos);
                break;
            }
            m_listener->notifyStatus(false, ("extra info: "+stream.str()).c_str());
            m_infoLen = 0;
            m_infoId = 0xff;
          }
        } else {
          m_infoLen = 0;  // reset on invalid response
          m_infoId = 0xff;
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
              stream << "unknown 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(data);
              break;
          }
          string str = stream.str();
          m_listener->notifyStatus(true, str.c_str());
        }
        cancelRunningArbitration(arbitrationState);
        break;
      default:
        if (m_listener != nullptr) {
          ostringstream stream;
          stream << "unexpected enhanced command 0x" << std::setw(2) << std::setfill('0') << std::hex
                 << static_cast<unsigned>(cmd);
          string str = stream.str();
          m_listener->notifyStatus(true, str.c_str());
        }
        return false;
    }
  }
  return false;
}


result_t SerialDevice::open() {
  result_t result = Device::open();
  if (result != RESULT_OK) {
    return result;
  }
  struct termios newSettings;

  // open file descriptor
  m_fd = ::open(m_name, O_RDWR | O_NOCTTY | O_NDELAY);

  if (m_fd < 0) {
    return RESULT_ERR_NOTFOUND;
  }
  if (isatty(m_fd) == 0) {
    close();
    return RESULT_ERR_NOTFOUND;
  }

  if (flock(m_fd, LOCK_EX|LOCK_NB)) {
    close();
    return RESULT_ERR_DEVICE;
  }

#ifdef HAVE_LINUX_SERIAL
  struct serial_struct serial;
  if (ioctl(m_fd, TIOCGSERIAL, &serial) == 0) {
    serial.flags |= ASYNC_LOW_LATENCY;
    ioctl(m_fd, TIOCSSERIAL, &serial);
  }
#endif

#ifdef HAVE_FREEBSD_UFTDI
  int param = 0;
  // flush tx/rx and set low latency on uftdi device
  if (ioctl(m_fd, UFTDIIOC_GET_LATENCY, &param) == 0) {
    ioctl(m_fd, UFTDIIOC_RESET_IO, &param);
    param = 1;
    ioctl(m_fd, UFTDIIOC_SET_LATENCY, &param);
  }
#endif

  // save current settings
  tcgetattr(m_fd, &m_oldSettings);

  // create new settings
  memset(&newSettings, 0, sizeof(newSettings));

#ifdef HAVE_CFSETSPEED
  cfsetspeed(&newSettings, m_enhancedProto ? (m_enhancedHighSpeed ? B115200 : B9600) : B2400);
#else
  cfsetispeed(&newSettings, m_enhancedProto ? (m_enhancedHighSpeed ? B115200 : B9600) : B2400);
  cfsetospeed(&newSettings, m_enhancedProto ? (m_enhancedHighSpeed ? B115200 : B9600) : B2400);
#endif
  newSettings.c_cflag |= (CS8 | CLOCAL | CREAD);
  newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // non-canonical mode
  newSettings.c_iflag |= IGNPAR;  // ignore parity errors
  newSettings.c_oflag &= ~OPOST;

  // non-canonical mode: read() blocks until at least one byte is available
  newSettings.c_cc[VMIN]  = 1;
  newSettings.c_cc[VTIME] = 0;

  // int flag = TIOCM_RTS|TIOCM_DTR;
  // empty device buffer
  tcflush(m_fd, TCIFLUSH);

  // activate new settings of serial device
  if (tcsetattr(m_fd, TCSANOW, &newSettings)) {
    close();
    return RESULT_ERR_DEVICE;
  }

  // set serial device into blocking mode
  fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

  return afterOpen();
}

void SerialDevice::close() {
  if (m_fd != -1) {
    // empty device buffer
    tcflush(m_fd, TCIOFLUSH);

    // restore previous settings of the device
    tcsetattr(m_fd, TCSANOW, &m_oldSettings);
  }
  Device::close();
}

void SerialDevice::checkDevice() {
  int cnt;
  if (ioctl(m_fd, FIONREAD, &cnt) == -1) {
    close();
  }
}

result_t NetworkDevice::open() {
  result_t result = Device::open();
  if (result != RESULT_OK) {
    return result;
  }
  m_fd = socketConnect(m_hostOrIp, m_port, m_udp, nullptr, 5, 2);  // wait up to 5 seconds for established connection
  if (m_fd < 0) {
    return RESULT_ERR_GENERIC_IO;
  }
  if (!m_udp) {
    usleep(25000);  // wait 25ms for potential initial garbage
  }
  int cnt;
  symbol_t buf[MTU];
  int ioerr;
  while ((ioerr=ioctl(m_fd, FIONREAD, &cnt)) >= 0 && cnt > 1) {
    // skip buffered input
    ssize_t read = ::read(m_fd, &buf, MTU);
    if (read <= 0) {
      break;
    }
  }
  if (ioerr < 0) {
    close();
    return RESULT_ERR_GENERIC_IO;
  }
  return afterOpen();
}

void NetworkDevice::checkDevice() {
  int cnt;
  if (ioctl(m_fd, FIONREAD, &cnt) < 0) {
    close();
  }
}

}  // namespace ebusd

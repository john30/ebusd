/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023-2025 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/transport.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#ifdef HAVE_LINUX_SERIAL
#  include <linux/serial.h>
#endif
#ifdef HAVE_FREEBSD_UFTDI
#  include <dev/usb/uftdiio.h>
#endif
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include "lib/ebus/data.h"
#include "lib/utils/tcpsocket.h"
#ifdef DEBUG_RAW_TRAFFIC
#  include "lib/utils/clock.h"
#endif

namespace ebusd {


#define MTU 1540

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif


#ifdef DEBUG_RAW_TRAFFIC
  #define DEBUG_RAW_TRAFFIC_HEAD(format, args...) fprintf(stdout, "%ld raw: " format, clockGetMillis(), args)
  #define DEBUG_RAW_TRAFFIC_ITEM(args...) fprintf(stdout, args)
  #define DEBUG_RAW_TRAFFIC_FINAL() fprintf(stdout, "\n"); fflush(stdout)
  #undef DEBUG_RAW_TRAFFIC
  #define DEBUG_RAW_TRAFFIC(format, args...) fprintf(stdout, "%ld raw: " format "\n", clockGetMillis(), args); fflush(stdout)
#else
  #define DEBUG_RAW_TRAFFIC_HEAD(format, args...)
  #undef DEBUG_RAW_TRAFFIC_ITEM
  #define DEBUG_RAW_TRAFFIC_FINAL()
  #define DEBUG_RAW_TRAFFIC(format, args...)
#endif



FileTransport::FileTransport(const char* name, unsigned int latency, bool checkDevice)
  : Transport(name, HOST_LATENCY_MS+latency),
    m_checkDevice(checkDevice),
    m_fd(-1),
    m_bufSize(((MAX_LEN+1+3)/4)*4), m_bufLen(0) {
  m_buffer = reinterpret_cast<symbol_t*>(malloc(m_bufSize));
  if (!m_buffer) {
    m_bufSize = 0;
  }
}

FileTransport::~FileTransport() {
  close();
  if (m_buffer) {
    free(m_buffer);
    m_buffer = nullptr;
  }
}

result_t FileTransport::open() {
  close();
  result_t result;
  if (m_bufSize == 0) {
    result = RESULT_ERR_DEVICE;
  } else {
    result = openInternal();
  }
  if (m_listener != nullptr && result == RESULT_OK) {
    result = m_listener->notifyTransportStatus(true);
  }
  if (result != RESULT_OK) {
    close();
  }
  return result;
}

void FileTransport::close() {
  if (m_fd == -1) {
    return;
  }
  ::close(m_fd);
  m_fd = -1;
  m_bufLen = 0;  // flush read buffer
  if (m_listener != nullptr) {
    m_listener->notifyTransportStatus(false);
  }
}

bool FileTransport::isValid() {
  if (m_fd == -1) {
    return false;
  }
  if (m_checkDevice) {
    checkDevice();
  }
  return m_fd != -1;
}

result_t FileTransport::write(const uint8_t* data, size_t len) {
  if (!isValid()) {
    return RESULT_ERR_DEVICE;
  }
#ifdef DEBUG_RAW_TRAFFIC_ITEM
  DEBUG_RAW_TRAFFIC_HEAD("%ld >", len);
  for (size_t pos=0; pos < len; pos++) {
    DEBUG_RAW_TRAFFIC_ITEM(" %2.2x", data[pos]);
  }
  DEBUG_RAW_TRAFFIC_FINAL();
#endif
  return (::write(m_fd, data, len) == len) ? RESULT_OK : RESULT_ERR_DEVICE;
}

result_t FileTransport::read(unsigned int timeout, const uint8_t** data, size_t* len) {
  if (!isValid()) {
    return RESULT_ERR_DEVICE;
  }
  if (timeout == 0) {
    if (m_bufLen > 0) {
      *data = m_buffer;
      *len = m_bufLen;
      return RESULT_OK;
    }
    return RESULT_ERR_TIMEOUT;
  }
  if (timeout > 0) {
    timeout += m_latency;
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
    FD_SET(m_fd, &exceptfds);

    ret = pselect(m_fd + 1, &readfds, nullptr, &exceptfds, &tdiff, nullptr);
    if (ret >= 1 && FD_ISSET(m_fd, &exceptfds)) {
      ret = -1;
    }
#else
    ret = 1;  // ignore timeout if neither ppoll nor pselect are available
#endif
#endif
    if (ret == -1) {
      DEBUG_RAW_TRAFFIC("poll error %d", errno);
      close();
      return RESULT_ERR_DEVICE;
    }
    if (ret == 0) {
      return RESULT_ERR_TIMEOUT;
    }
  }

  // directly read byte from device
  if (m_bufLen > 0 && m_bufLen > m_bufSize - m_bufSize / 4) {
    // more than 3/4 of input buffer consumed is taken as signal that ebusd is too slow
    m_bufLen = 0;
    if (m_listener != nullptr) {
      m_listener->notifyTransportMessage(true, "buffer overflow");
    }
  }
  // fill up the buffer
  ssize_t size = ::read(m_fd, m_buffer + m_bufLen, m_bufSize - m_bufLen);
  if (size <= 0) {
    return RESULT_ERR_TIMEOUT;
  }
#ifdef DEBUG_RAW_TRAFFIC_ITEM
  DEBUG_RAW_TRAFFIC_HEAD("%ld+%ld <", m_bufLen, size);
  for (int pos=0; pos < size; pos++) {
    DEBUG_RAW_TRAFFIC_ITEM(" %2.2x", m_buffer[(m_bufLen+pos)%m_bufSize]);
  }
  DEBUG_RAW_TRAFFIC_FINAL();
#endif
  m_bufLen += size;
  *data = m_buffer;
  *len = m_bufLen;
  return RESULT_OK;
}

void FileTransport::readConsumed(size_t len) {
  if (len >= m_bufLen) {
    m_bufLen = 0;
  } else if (len > 0) {
    size_t tail = m_bufLen - len;
    memmove(m_buffer, m_buffer + len, tail);
    DEBUG_RAW_TRAFFIC("move %ld @%ld to 0", tail, len);
    m_bufLen = tail;
  }
}


result_t SerialTransport::openInternal() {
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

  if (flock(m_fd, LOCK_EX|LOCK_NB) != 0) {
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
  cfsetspeed(&newSettings, m_speed ? (m_speed > 1 ? B115200 : B9600) : B2400);
#else
  cfsetispeed(&newSettings, m_speed ? (m_speed > 1 ? B115200 : B9600) : B2400);
#endif
  newSettings.c_cflag |= (CS8 | CLOCAL | CREAD);
  newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // non-canonical mode
  newSettings.c_iflag |= IGNPAR;  // ignore parity errors
  newSettings.c_oflag &= ~OPOST;

  // non-canonical mode: read() blocks until at least one byte is available
  newSettings.c_cc[VMIN]  = 1;
  newSettings.c_cc[VTIME] = 0;

  // empty device buffer
  tcflush(m_fd, TCIFLUSH);

  // activate new settings of serial device
  if (tcsetattr(m_fd, TCSANOW, &newSettings)) {
    close();
    return RESULT_ERR_DEVICE;
  }

  // set serial device into blocking mode
  fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

  return RESULT_OK;
}

void SerialTransport::close() {
  if (m_fd != -1) {
    // empty device buffer
    tcflush(m_fd, TCIOFLUSH);

    // restore previous settings of the device
    tcsetattr(m_fd, TCSANOW, &m_oldSettings);
  }
  FileTransport::close();
}

void SerialTransport::checkDevice() {
  int cnt;
  if (ioctl(m_fd, FIONREAD, &cnt) == -1) {
    close();
  }
}

result_t NetworkTransport::openInternal() {
  // wait up to 5 seconds for established connection
  m_fd = socketConnect(m_hostOrIp, m_port, m_udp ? IPPROTO_UDP : 0, nullptr, 5, 2);
  if (m_fd < 0) {
    return RESULT_ERR_GENERIC_IO;
  }
  int cnt;
  symbol_t buf[MTU];
  int ioerr;
  while ((ioerr=ioctl(m_fd, FIONREAD, &cnt)) >= 0 && cnt > 1) {
    if (!m_udp) {
      break;  // no need to skip anything on a fresh TCP connection
    }
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
  return RESULT_OK;
}

void NetworkTransport::checkDevice() {
  int cnt;
  if (ioctl(m_fd, FIONREAD, &cnt) < 0) {
    close();
  }
}

}  // namespace ebusd

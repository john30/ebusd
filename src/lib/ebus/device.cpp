/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2018 John Baier <ebusd@ebusd.eu>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef HAVE_LINUX_SERIAL
#  include <linux/serial.h>
#endif
#ifdef HAVE_FREEBSD_UFTDI
#  include <dev/usb/uftdiio.h>
#endif
#include <errno.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "lib/ebus/data.h"

namespace ebusd {

#define MTU 1540

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

Device::~Device() {
  close();
}

Device* Device::create(const char* name, bool checkDevice, bool readOnly, bool initialSend) {
  if (strchr(name, '/') == nullptr && strchr(name, ':') != nullptr) {
    char* in = strdup(name);
    bool udp = false;
    char* addrpos = in;
    char* portpos = strchr(addrpos, ':');
    if (portpos == addrpos+3 && (strncmp(addrpos, "tcp", 3) == 0 || (udp=(strncmp(addrpos, "udp", 3) == 0)))) {
      addrpos += 4;
      portpos = strchr(addrpos, ':');
    }
    if (portpos == nullptr) {
      free(in);
      return nullptr;  // invalid protocol or missing port
    }
    result_t result = RESULT_OK;
    unsigned int port = parseInt(portpos+1, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      free(in);
      return nullptr;  // invalid port
    }
    struct sockaddr_in address;
    memset(reinterpret_cast<char*>(&address), 0, sizeof(address));
    *portpos = 0;
    if (inet_aton(addrpos, &address.sin_addr) == 0) {
      struct hostent* h = gethostbyname(addrpos);
      if (h == nullptr) {
        free(in);
        return nullptr;  // invalid host
      }
      memcpy(&address.sin_addr, h->h_addr_list[0], h->h_length);
    }
    free(in);
    address.sin_family = AF_INET;
    address.sin_port = (in_port_t)htons((uint16_t)port);
    return new NetworkDevice(name, address, readOnly, initialSend, udp);
  }
  return new SerialDevice(name, checkDevice, readOnly, initialSend);
}

void Device::close() {
  if (m_fd != -1) {
    ::close(m_fd);
    m_fd = -1;
  }
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

result_t Device::send(symbol_t value) {
  if (!isValid()) {
    return RESULT_ERR_DEVICE;
  }
  if (m_readOnly || write(value) != 1) {
    return RESULT_ERR_SEND;
  }
  if (m_listener != nullptr) {
    m_listener->notifyDeviceData(value, false);
  }
  return RESULT_OK;
}

result_t Device::recv(unsigned int timeout, symbol_t* value) {
  if (!isValid()) {
    return RESULT_ERR_DEVICE;
  }
  if (!available() && timeout > 0) {
    int ret;
    struct timespec tdiff;

    // set select timeout
    tdiff.tv_sec = timeout/1000000;
    tdiff.tv_nsec = (timeout%1000000)*1000;

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
      close();
      return RESULT_ERR_DEVICE;
    }
    if (ret == 0) {
      return RESULT_ERR_TIMEOUT;
    }
  }

  // directly read byte from device
  ssize_t nbytes = read(value);
  if (nbytes == 0) {
    return RESULT_ERR_EOF;
  }
  if (nbytes < 0) {
    close();
    return RESULT_ERR_DEVICE;
  }
  if (m_listener != nullptr) {
    m_listener->notifyDeviceData(*value, true);
  }
  return RESULT_OK;
}


result_t SerialDevice::open() {
  if (m_fd != -1) {
    close();
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

  cfsetspeed(&newSettings, B2400);
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
  if (tcsetattr(m_fd, TCSAFLUSH, &newSettings)) {
    close();
    return RESULT_ERR_DEVICE;
  }

  // set serial device into blocking mode
  fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

  if (m_initialSend && write(ESC) != 1) {
    return RESULT_ERR_SEND;
  }
  return RESULT_OK;
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
  int port;
  if (ioctl(m_fd, TIOCMGET, &port) == -1) {
    close();
  }
}


result_t NetworkDevice::open() {
  if (m_fd != -1) {
    close();
  }
  m_fd = socket(AF_INET, m_udp ? SOCK_DGRAM : SOCK_STREAM, 0);
  if (m_fd < 0) {
    return RESULT_ERR_GENERIC_IO;
  }
  int ret;
  if (m_udp) {
    struct sockaddr_in address = m_address;
    address.sin_addr.s_addr = INADDR_ANY;
    ret = bind(m_fd, (struct sockaddr*)&address, sizeof(address));
  } else {
    int value = 1;
    ret = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&value), sizeof(value));
    value = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<void*>(&value), sizeof(value));
  }
  if (ret >= 0) {
    ret = connect(m_fd, (struct sockaddr*)&m_address, sizeof(m_address));
  }
  if (ret < 0) {
    close();
    return RESULT_ERR_GENERIC_IO;
  }
  if (!m_udp) {
    usleep(25000);  // wait 25ms for potential initial garbage
  }
  int cnt;
  symbol_t buf[MTU];
  while (ioctl(m_fd, FIONREAD, &cnt) >= 0 && cnt > 1) {
    // skip buffered input
    ssize_t read = ::read(m_fd, &buf, MTU);
    if (read <= 0) {
      break;
    }
  }
  if (m_bufSize == 0) {
    m_bufSize = MAX_LEN+1;
    m_buffer = reinterpret_cast<symbol_t*>(malloc(m_bufSize));
    if (!m_buffer) {
      m_bufSize = 0;
    }
  }
  m_bufLen = 0;
  if (m_initialSend && write(ESC) != 1) {
    return RESULT_ERR_SEND;
  }
  return RESULT_OK;
}

void NetworkDevice::close() {
  m_bufLen = 0;  // flush read buffer
  Device::close();
}

void NetworkDevice::checkDevice() {
  int cnt;
  if (ioctl(m_fd, FIONREAD, &cnt) < 0) {
    close();
  }
}

bool NetworkDevice::available() {
  return m_buffer && m_bufLen > 0;
}

ssize_t NetworkDevice::write(symbol_t value) {
  m_bufLen = 0;  // flush read buffer
  return Device::write(value);
}

ssize_t NetworkDevice::read(symbol_t* value) {
  if (available()) {
    *value = m_buffer[m_bufPos];
    m_bufPos = (m_bufPos+1)%m_bufSize;
    m_bufLen--;
    return 1;
  }
  if (m_bufSize > 0) {
    ssize_t size = ::read(m_fd, m_buffer, m_bufSize);
    if (size <= 0) {
      return size;
    }
    *value = m_buffer[0];
    m_bufPos = 1;
    m_bufLen = size-1;
    return size;
  }
  return Device::read(value);
}

}  // namespace ebusd

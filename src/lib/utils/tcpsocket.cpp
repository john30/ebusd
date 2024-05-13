/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2024 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "lib/utils/tcpsocket.h"
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif

namespace ebusd {

TCPSocket::TCPSocket(int sfd, socketaddress* address) : m_sfd(sfd) {
  char ip[17];
  inet_ntop(AF_INET, (struct in_addr*)&(address->sin_addr.s_addr), ip, (socklen_t)sizeof(ip)-1);
  m_ip = ip;
  m_port = (uint16_t)ntohs(address->sin_port);
}

bool TCPSocket::isValid() {
  return fcntl(m_sfd, F_GETFL) != -1;
}


int socketConnect(const char* server, uint16_t port, bool udp, socketaddress* storeAddress, int tcpConnectTimeout,
int tcpKeepAliveInterval) {
  socketaddress localAddress;
  socketaddress* address = storeAddress ? storeAddress : &localAddress;
  memset(reinterpret_cast<char*>(address), 0, sizeof(*address));

  if (inet_aton(server, &address->sin_addr) == 0) {
    struct hostent* he = gethostbyname(server);
    if (he == nullptr) {
      return -1;
    }
    memcpy(&address->sin_addr, he->h_addr_list[0], he->h_length);
  }
  address->sin_family = AF_INET;
  address->sin_port = (in_port_t)htons(port);

  int sfd = socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
  if (sfd < 0) {
    return -1;
  }
  int ret;
  if (udp) {
    struct sockaddr_in bindAddress = *address;
    bindAddress.sin_addr.s_addr = INADDR_ANY;
    ret = bind(sfd, (struct sockaddr*)&bindAddress, sizeof(bindAddress));
    if (ret >= 0) {
      ret = ::connect(sfd, (struct sockaddr*)address, sizeof(*address));
    }
    if (ret < 0) {
      close(sfd);
      return -1;
    }
    return sfd;
  }
  int value = 1;
  ret = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&value), sizeof(value));
  if (ret < 0) {
    close(sfd);
    return -1;
  }
  if (tcpKeepAliveInterval > 0) {
    value = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPALIVE");
    }
#ifndef TCP_KEEPIDLE
  #ifdef TCP_KEEPALIVE
    #define TCP_KEEPIDLE TCP_KEEPALIVE
  #else
    #define TCP_KEEPIDLE 4
  #endif
#endif
#ifndef TCP_KEEPINTVL
  #define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
  #define TCP_KEEPCNT 6
#endif
    value = tcpKeepAliveInterval+1;  // send keepalive after interval + 1 seconds of silence
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPIDLE");
    }
    value = tcpKeepAliveInterval;  // send keepalive in given interval
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPINTVL");
    }
    value = 2;  // drop connection after 2 failed keep alive sends
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPCNT");
    }
#ifdef TCP_USER_TIMEOUT
    value = (2+tcpKeepAliveInterval*3)*1000;  // 1 second higher than keepalive timeout
    if (setsockopt(sfd, IPPROTO_TCP, TCP_USER_TIMEOUT, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
       perror("setsockopt USER_TIMEOUT");
    }
#endif
  }
  if (tcpConnectTimeout > 0 && fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) {  // set non-blocking
    close(sfd);
    return -1;
  }
  ret = ::connect(sfd, (struct sockaddr*)address, sizeof(*address));
  if (ret != 0) {
    if (ret < 0 && (tcpConnectTimeout <= 0 || errno != EINPROGRESS)) {
      close(sfd);
      return -1;
    }
    if (tcpConnectTimeout > 0) {
#if defined(HAVE_PPOLL) || defined(HAVE_PSELECT)
      struct timespec tdiff;
      tdiff.tv_sec = tcpConnectTimeout;
      tdiff.tv_nsec = 0;
#else
      struct timeval tdiff;
      tdiff.tv_sec = tcpConnectTimeout;
      tdiff.tv_usec = 0;
#endif
#ifdef HAVE_PPOLL
      nfds_t nfds = 1;
      struct pollfd fds[nfds];
      memset(fds, 0, sizeof(fds));
      fds[0].fd = sfd;
      fds[0].events = POLLIN|POLLOUT;
      ret = ppoll(fds, nfds, &tdiff, nullptr);
      if (ret == 1 && fds[0].revents & POLLERR) {
        ret = -1;
      }
#else
      fd_set readfds, writefds, exceptfds;
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_ZERO(&exceptfds);
      FD_SET(sfd, &readfds);
      FD_SET(sfd, &writefds);
      FD_SET(sfd, &exceptfds);
#ifdef HAVE_PSELECT
      ret = pselect(sfd + 1, &readfds, &writefds, &exceptfds, &tdiff, nullptr);
#else
      ret = select(sfd + 1, &readfds, &writefds, &exceptfds, &tdiff);
#endif
      if (ret >= 1 && FD_ISSET(sfd, &exceptfds)) {
        ret = -1;
      }
#endif
      if (ret == -1 || ret == 0) {
        close(sfd);
        return -1;
      }
      if (fcntl(sfd, F_SETFL, 0) < 0) {  // set blocking again
        close(sfd);
        return -1;
      }
    }
  }
  return sfd;
}


TCPSocket* TCPSocket::connect(const string& server, const uint16_t& port, int timeout) {
  socketaddress address;
  int sfd = socketConnect(server.c_str(), port, false, &address, timeout);
  if (sfd < 0) {
    return nullptr;
  }
  TCPSocket* s = new TCPSocket(sfd, &address);
  if (timeout > 0) {
    s->setTimeout(timeout);
  }
  return s;
}


int TCPServer::start() {
  if (m_listening) {
    return 0;
  }
  m_lfd = socket(AF_INET, SOCK_STREAM, 0);
  socketaddress address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_port = (in_port_t)htons(m_port);

  if (!m_address.empty() && inet_pton(AF_INET, m_address.c_str(), &address.sin_addr) != 1) {
    address.sin_addr.s_addr = INADDR_ANY;
  }
  int value = 1;
  setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  int result = bind(m_lfd, (struct sockaddr*)&address, sizeof(address));
  if (result != 0) {
    return result;
  }
  result = listen(m_lfd, 5);
  if (result != 0) {
    return result;
  }
  m_listening = true;
  return result;
}

TCPSocket* TCPServer::newSocket() {
  if (!m_listening) {
    return nullptr;
  }
  socketaddress address;
  socklen_t len = sizeof(address);
  memset(&address, 0, sizeof(address));

  int sfd = accept(m_lfd, (struct sockaddr*)&address, &len);
  if (sfd < 0) {
    return nullptr;
  }
  return new TCPSocket(sfd, &address);
}

}  // namespace ebusd

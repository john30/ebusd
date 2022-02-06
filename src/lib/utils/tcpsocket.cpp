/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2022 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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
#include <string.h>
#include <errno.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstdlib>

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


TCPSocket* TCPClient::connect(const string& server, const uint16_t& port, int timeout) {
  socketaddress address;
  int ret;

  memset(reinterpret_cast<char*>(&address), 0, sizeof(address));

  if (inet_addr(server.c_str()) == INADDR_NONE) {
    struct hostent* he;

    he = gethostbyname(server.c_str());
    if (he == nullptr) {
      return nullptr;
    }
    memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
  } else {
    ret = inet_aton(server.c_str(), &address.sin_addr);
    if (ret == 0) {
      return nullptr;
    }
  }

  address.sin_family = AF_INET;
  address.sin_port = (in_port_t)htons(port);

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    return nullptr;
  }
#ifndef HAVE_PPOLL
#ifndef HAVE_PSELECT
  timeout = 0;
#endif
#endif
  if (timeout > 0 && fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) {  // set non-blocking
    close(sfd);
    return nullptr;
  }
  ret = ::connect(sfd, (struct sockaddr *) &address, sizeof(address));
  if (ret != 0) {
    if (ret < 0 && (timeout <= 0 || errno != EINPROGRESS)) {
      close(sfd);
      return nullptr;
    }
    if (timeout > 0) {
      struct timespec tdiff;
      tdiff.tv_sec = timeout;
      tdiff.tv_nsec = 0;
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
      ret = pselect(sfd + 1, &readfds, &writefds, &exceptfds, &tdiff, nullptr);
      if (ret >= 1 && FD_ISSET(sfd, &exceptfds)) {
        ret = -1;
      }
#endif
      if (ret == -1 || ret == 0) {
        close(sfd);
        return nullptr;
      }
    }
  }
  if (timeout > 0 && fcntl(sfd, F_SETFL, 0) < 0) {  // set blocking again
    close(sfd);
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

  if (m_address.size() > 0) {
    inet_pton(AF_INET, m_address.c_str(), &(address.sin_addr));
  } else {
    address.sin_addr.s_addr = INADDR_ANY;
  }
  int optval = 1;
  setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  int result = bind(m_lfd, (struct sockaddr*) &address, sizeof(address));
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

  int sfd = accept(m_lfd, (struct sockaddr*) &address, &len);
  if (sfd < 0) {
    return nullptr;
  }
  return new TCPSocket(sfd, &address);
}

}  // namespace ebusd

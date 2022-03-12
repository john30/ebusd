/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2022 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "ebusd/network.h"
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstring>
#include "lib/utils/log.h"

namespace ebusd {

int Connection::m_ids = 0;

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

bool NetMessage::add(const char* request) {
  if (request && request[0]) {
    string add = request;
    add.erase(remove(add.begin(), add.end(), '\r'), add.end());
    m_request.append(add);
  }
  size_t pos = m_request.find(m_isHttp ? "\n\n" : "\n");
  if (pos != string::npos) {
    if (m_isHttp) {
      pos = m_request.find("\n");
      m_request.resize(pos);  // reduce to first line
      // typical first line: GET /ehp/outsidetemp HTTP/1.1
      pos = m_request.rfind(" HTTP/");
      if (pos != string::npos) {
        m_request.resize(pos);  // remove "HTTP/x.x" suffix
      }
      pos = 0;
      while ((pos=m_request.find('%', pos)) != string::npos && pos+2 <= m_request.length()) {
        unsigned int value1, value2;
        if (sscanf("%1x%1x", m_request.c_str()+pos+1, &value1, &value2) < 2) {
          break;
        }
        m_request[pos] = static_cast<char>(((value1&0x0f) << 4) | (value2&0x0f));
        m_request.erase(pos+1, 2);
      }
    } else if (pos+1 == m_request.length()) {
      m_request.resize(pos);  // reduce to complete lines
    }
    return true;
  }
  return m_request.length() == 0 && isListeningMode();
}


void Connection::run() {
  int ret;
  struct timespec tdiff;

  // set timeout
  tdiff.tv_sec = 2;
  tdiff.tv_nsec = 0;
  int notifyFD = m_notify.notifyFD();
  int sockFD = m_socket->getFD();

#ifdef HAVE_PPOLL
  nfds_t nfds = 2;
  struct pollfd fds[nfds];

  memset(fds, 0, sizeof(fds));

  fds[0].fd = notifyFD;
  fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

  fds[1].fd = sockFD;
  fds[1].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
#else
#ifdef HAVE_PSELECT
  int maxfd = (notifyFD > sockFD) ? notifyFD : sockFD;
  fd_set checkfds, exceptfds;

  FD_ZERO(&checkfds);
  FD_SET(notifyFD, &checkfds);
  FD_SET(sockFD, &checkfds);

  FD_ZERO(&exceptfds);
  FD_SET(notifyFD, &exceptfds);
  FD_SET(sockFD, &exceptfds);
#endif
#endif

  bool closed = false;
  NetMessage message(m_isHttp);

  while (!closed) {
#ifdef HAVE_PPOLL
    // wait for new fd event
    ret = ppoll(fds, nfds, &tdiff, nullptr);
#else
#ifdef HAVE_PSELECT
    // set readfds to inital checkfds
    fd_set readfds = checkfds;
    // wait for new fd event
    ret = pselect(maxfd + 1, &readfds, nullptr, &exceptfds, &tdiff, nullptr);
#endif
#endif
    bool newData = false;
    if (ret != 0) {
#ifdef HAVE_PPOLL
      // new data from notify
      if (ret < 0 || (ret > 0 && ((fds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLRDHUP))
          || (fds[1].revents & (POLLERR | POLLHUP))))) {
        break;
      }
      // new data from socket
      newData = fds[1].revents & POLLIN;
      closed = fds[1].revents & POLLRDHUP;
#else
#ifdef HAVE_PSELECT
      // new data from notify
      if (ret < 0 || FD_ISSET(notifyFD, &readfds) || FD_ISSET(notifyFD, &exceptfds)) {
        break;
      }
      // new data from socket
      newData = FD_ISSET(sockFD, &readfds);
      closed = FD_ISSET(sockFD, &exceptfds);
#endif
#endif
    }

    if (newData || message.isListeningMode()) {
      char data[256];

      if (!m_socket->isValid()) {
        break;
      }
      if (newData) {
        ssize_t datalen = m_socket->recv(data, sizeof(data)-1);

        // remove closed socket
        if (datalen <= 0) {
          break;
        }
        data[datalen] = '\0';
      } else {
        data[0] = '\0';
      }

      // decode client data
      if (message.add(data)) {
        m_netQueue->push(&message);

        // wait for result
        logDebug(lf_network, "[%05d] wait for result", getID());
        string result;
        message.getResult(&result);

        if (!m_socket->isValid()) {
          break;
        }
        m_socket->send(result.c_str(), result.size());
      }

      if (message.isDisconnect() || !m_socket->isValid()) {
        break;
      }
    }
  }

  if (m_socket) {
    delete m_socket;
    m_socket = nullptr;
  }
  time(&m_endedAt);
  logInfo(lf_network, "[%05d] connection closed", getID());
}


Network::Network(const bool local, const uint16_t port, const uint16_t httpPort, Queue<NetMessage*>* netQueue)
  : Thread(), m_netQueue(netQueue), m_listening(false) {
  m_tcpServer = new TCPServer(port, local ? "127.0.0.1" : "0.0.0.0");

  if (m_tcpServer != nullptr && m_tcpServer->start() == 0) {
    m_listening = true;
  }
  if (httpPort > 0) {
    m_httpServer = new TCPServer(httpPort, "0.0.0.0");
    m_httpServer->start();
  } else {
    m_httpServer = nullptr;
  }
}

Network::~Network() {
  stop();
  NetMessage* netMsg;
  while ((netMsg = m_netQueue->pop()) != nullptr) {
    netMsg->setResult("ERR: shutdown", "", nullptr, 0, true);
  }
  while (!m_connections.empty()) {
    Connection* connection = m_connections.back();
    m_connections.pop_back();
    connection->stop();
    connection->join();
    delete connection;
  }

  if (m_tcpServer != nullptr) {
    delete m_tcpServer;
  }
  if (m_httpServer != nullptr) {
    delete m_httpServer;
  }
  join();
}

void Network::run() {
  if (!m_listening) {
    return;
  }
  int ret;
  struct timespec tdiff;

  // set timeout
  tdiff.tv_sec = 1;
  tdiff.tv_nsec = 0;
#ifdef HAVE_PPOLL
  int socketCount = m_httpServer ? 2 : 1;
  nfds_t nfds = 1+socketCount;
  struct pollfd fds[nfds];

  memset(fds, 0, sizeof(fds));

  fds[0].fd = m_notify.notifyFD();
  fds[0].events = POLLIN;

  fds[1].fd = m_tcpServer->getFD();
  fds[1].events = POLLIN;

  if (m_httpServer) {
    fds[2].fd = m_httpServer->getFD();
    fds[2].events = POLLIN;
  }
#else
#ifdef HAVE_PSELECT
  int maxfd;
  fd_set checkfds;

  FD_ZERO(&checkfds);
  FD_SET(m_notify.notifyFD(), &checkfds);
  FD_SET(m_tcpServer->getFD(), &checkfds);
  if (m_httpServer) {
    FD_SET(m_httpServer->getFD(), &checkfds);
  }

  maxfd = (m_notify.notifyFD() > m_tcpServer->getFD()) ?
    m_notify.notifyFD() : m_tcpServer->getFD();
  if (m_httpServer && m_httpServer->getFD() > maxfd) {
    maxfd = m_httpServer->getFD();
  }
#endif
#endif
  int cleanupCnt = 0;
  while (true) {
    if (++cleanupCnt > 10) {
      cleanConnections();
    }
#ifdef HAVE_PPOLL
    // wait for new fd event
    ret = ppoll(fds, nfds, &tdiff, nullptr);
#else
#ifdef HAVE_PSELECT
    // set readfds to inital checkfds
    fd_set readfds = checkfds;
    // wait for new fd event
    ret = pselect(maxfd + 1, &readfds, nullptr, nullptr, &tdiff, nullptr);
#endif
#endif
    if (ret == 0) {
      cleanConnections();
      cleanupCnt = 0;
      continue;
    }
    bool newData = false, isHttp = false;
#ifdef HAVE_PPOLL
    // new data from notify
    if (fds[0].revents & POLLIN) {
      return;
    }
    // new data from socket
    if (fds[1].revents & POLLIN) {
      newData = true;
    } else if (m_httpServer && fds[2].revents & POLLIN) {
      newData = isHttp = true;
    }
#else
#ifdef HAVE_PSELECT
    // new data from notify
    if (FD_ISSET(m_notify.notifyFD(), &readfds)) {
      return;
    }
    // new data from socket
    if (FD_ISSET(m_tcpServer->getFD(), &readfds)) {
      newData = true;
    } else if (m_httpServer && FD_ISSET(m_httpServer->getFD(), &readfds)) {
      newData = isHttp = true;
    }
#endif
#endif
    if (newData) {
      TCPSocket* socket = (isHttp ? m_httpServer : m_tcpServer)->newSocket();
      if (socket == nullptr) {
        continue;
      }
      Connection* connection = new Connection(socket, isHttp, m_netQueue);
      string ip = socket->getIP();
      connection->start("connection");
      m_connections.push_back(connection);
      logInfo(lf_network, "[%05d] %s connection opened %s", connection->getID(), isHttp ? "HTTP" : "client",
          ip.c_str());
    }
  }
}

void Network::cleanConnections() {
  auto it = m_connections.begin();
  time_t endBefore;
  time(&endBefore);
  endBefore += 5;  // after 5 seconds grace period
  while (it != m_connections.end()) {
    Connection* connection = *it;
    if (connection && !connection->isRunning() && connection->endedBefore(&endBefore)) {
      it = m_connections.erase(it);
      delete connection;
      logDebug(lf_network, "dead connection removed - %d", m_connections.size());
    } else {
      it++;
    }
  }
}

}  // namespace ebusd

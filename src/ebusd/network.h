/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2025 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#ifndef EBUSD_NETWORK_H_
#define EBUSD_NETWORK_H_

#include <string>
#include <cstdio>
#include <algorithm>
#include <list>
#include "ebusd/request.h"
#include "lib/ebus/datatype.h"
#include "lib/utils/tcpsocket.h"
#include "lib/utils/queue.h"
#include "lib/utils/notify.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** \file ebusd/network.h
 * The TCP and HTTP client request handling.
 */


/**
 * Instance of a connected client, either TCP or HTTP.
 */
class Connection : public Thread {
 public:
  /**
   * Constructor.
   * @param socket the @a TCPSocket for communication.
   * @param isHttp whether this is a HTTP message.
   * @param requestQueue the reference to the @a Request @a Queue.
   */
  Connection(TCPSocket* socket, const bool isHttp, Queue<Request*>* requestQueue)
    : Thread(), m_isHttp(isHttp), m_socket(socket), m_requestQueue(requestQueue), m_endedAt(0) {
    m_id = ++m_ids;
  }

  virtual ~Connection() {
    if (m_socket) {
      delete m_socket;
      m_socket = nullptr;
    }
  }
  /**
   * endless loop for connection instance.
   */
  virtual void run();

  /**
   * Stop this connection.
   */
  virtual void stop() { m_notify.notify(); Thread::stop(); }

  /**
   * Return the ID of this connection.
   * @return the ID of this connection.
   */
  int getID() { return m_id; }

  /**
   * Return whether this connection has ended before the specified time.
   * @param time the time to check against.
   * @return true when this connection has ended before the specified time.
   */
  bool endedBefore(const time_t* time) const { return m_endedAt > 0 && time && m_endedAt < *time; }

 private:
  /** whether this is a HTTP connection. */
  const bool m_isHttp;

  /** the @a TCPSocket for communication. */
  TCPSocket* m_socket;

  /** the reference to the @a Request @a Queue. */
  Queue<Request*>* m_requestQueue;

  /** notification object for shutdown procedure. */
  Notify m_notify;

  /** the ID of this connection. */
  int m_id;

  /** the time when this connected ended, or 0 if not yet. */
  time_t m_endedAt;

  /** the IF of the last opened connection. */
  static int m_ids;
};

/**
 * Handler for all TCP and HTTP client connections and registry of active connections.
 */
class Network : public Thread {
 public:
  /**
   * create a network instance and listening for incoming connections.
   * @param local true to accept connections only for local host.
   * @param port the port to listen for command line connections.
   * @param httpPort the port to listen for HTTP connections, or 0.
   * @param requestQueue the reference to the @a Request @a Queue.
   */
  Network(const bool local, const uint16_t port, const uint16_t httpPort, Queue<Request*>* requestQueue);

  /**
   * destructor.
   */
  ~Network();

  /**
   * endless loop for network instance.
   */
  virtual void run();

  /**
   * shutdown network subsystem.
   */
  void stop() const { m_notify.notify(); usleep(100000); }


 private:
  /** the list of active @a Connection instances. */
  list<Connection*> m_connections;

  /** the reference to the @a Request @a Queue. */
  Queue<Request*>* m_requestQueue;

  /** the command line @a TCPServer instance. */
  TCPServer* m_tcpServer;

  /** the HTTP @a TCPServer instance, or nullptr. */
  TCPServer* m_httpServer;

  /** @a Notify object for shutdown procedure. */
  Notify m_notify;

  /** true if this instance is listening. */
  bool m_listening;

  /**
   * clean inactive connections from container.
   */
  void cleanConnections();
};

}  // namespace ebusd

#endif  // EBUSD_NETWORK_H_

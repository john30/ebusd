/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2021 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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
#include "lib/ebus/datatype.h"
#include "lib/utils/tcpsocket.h"
#include "lib/utils/queue.h"
#include "lib/utils/notify.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** \file ebusd/network.h
 * The TCP and HTTP client request handling.
 */

/** Forward declaration for @a Connection. */
class Connection;

/** the possible client modes. */
enum ClientMode {
  cm_normal,  //!< normal mode
  cm_listen,  //!< listening mode
  cm_direct,  //!< direct mode
};

/**
 * Combination of client settings.
 */
struct ClientSettings {
  ClientMode mode;         //!< the current client mode
  OutputFormat format;     //!< the output format settings for listen mode
  bool listenWithUnknown;  //!< include unknown messages in listen mode
  bool listenOnlyUnknown;  //!< only print unknown messages in listen mode
};

/**
 * Class for data/message transfer between @a Connection and @a MainLoop.
 */
class NetMessage {
 public:
  /**
   * Constructor.
   * @param isHttp whether this is a HTTP message.
   */
  explicit NetMessage(bool isHttp)
    : m_isHttp(isHttp), m_resultSet(false), m_disconnect(false), m_listenSince(0) {
    m_settings.mode = cm_normal;
    m_settings.format = OF_NONE;
    m_settings.listenWithUnknown = false;
    m_settings.listenOnlyUnknown = false;
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
  }

  /**
   * Destructor.
   */
  ~NetMessage() {
    m_resultSet = true;
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);
  }


 private:
  /**
   * Hidden copy constructor.
   * @param src the object to copy from.
   */
  NetMessage(const NetMessage& src);


 public:
  /**
   * Add request data received from the client.
   * @param request the request data from the client.
   * @return true when the request is complete and the response shall be prepared.
   */
  bool add(const char* request);

  /**
   * Return whether this is a HTTP message.
   * @return whether this is a HTTP message.
   */
  bool isHttp() const { return m_isHttp; }

  /**
   * Return the request string.
   * @return the request string.
   */
  const string& getRequest() const { return m_request; }

  /**
   * Return the current user name.
   * @return the current user name.
   */
  const string& getUser() const { return m_user; }

  /**
   * Wait for the result being set and return the result string.
   * @param result the variable in which to store the result string.
   */
  void getResult(string* result) {
    pthread_mutex_lock(&m_mutex);

    if (!m_resultSet) {
      pthread_cond_wait(&m_cond, &m_mutex);
    }
    m_request.clear();
    *result = m_result;
    m_result.clear();
    m_resultSet = false;
    pthread_mutex_unlock(&m_mutex);
  }

  /**
   * Set the result string and notify the waiting thread.
   * @param result the result string.
   * @param user the new user name.
   * @param settings the new client settings.
   * @param listenUntil the end time to which to updates were added (exclusive).
   * @param disconnect true when the client shall be disconnected.
   */
  void setResult(const string& result, const string& user, ClientSettings* settings, time_t listenUntil,
      bool disconnect) {
    pthread_mutex_lock(&m_mutex);
    m_result = result;
    m_user = user;
    m_disconnect = disconnect;
    if (settings) {
      m_settings = *settings;
    }
    m_listenSince = listenUntil;
    m_resultSet = true;
    pthread_cond_signal(&m_cond);
    pthread_mutex_unlock(&m_mutex);
  }

  /**
   * Return the client settings.
   * @param listenSince set listening to the specified start time from which to add updates (inclusive).
   * @return the client settings.
   */
  ClientSettings getSettings(time_t* listenSince = nullptr) {
    if (listenSince) {
      *listenSince = m_listenSince;
    }
    return m_settings;
  }

  /**
   * Return whether this instance is in one of the listening modes.
   * @return whether this instance is in one of the listening modes.
   */
  bool isListeningMode() { return m_settings.mode == cm_listen || m_settings.mode == cm_direct; }

  /**
   * Return whether the client shall be disconnected.
   * @return true when the client shall be disconnected.
   */
  bool isDisconnect() { return m_disconnect; }


 private:
  /** whether this is a HTTP message. */
  const bool m_isHttp;

  /** the request string. */
  string m_request;

  /** the current user name. */
  string m_user;

  /** whether the result was already set. */
  bool m_resultSet;

  /** the result string. */
  string m_result;

  /** set to true when the client shall be disconnected. */
  bool m_disconnect;

  /** mutex variable for exclusive lock. */
  pthread_mutex_t m_mutex;

  /** condition variable for exclusive lock. */
  pthread_cond_t m_cond;

  /** the client settings. */
  ClientSettings m_settings;

  /** start timestamp of listening update. */
  time_t m_listenSince;
};

/**
 * class connection which handle client and baseloop communication.
 */
class Connection : public Thread {
 public:
  /**
   * Constructor.
   * @param socket the @a TCPSocket for communication.
   * @param isHttp whether this is a HTTP message.
   * @param netQueue the reference to the @a NetMessage @a Queue.
   */
  Connection(TCPSocket* socket, const bool isHttp, Queue<NetMessage*>* netQueue)
    : Thread(), m_isHttp(isHttp), m_socket(socket), m_netQueue(netQueue) {
    m_id = ++m_ids;
  }

  virtual ~Connection() { if (m_socket) delete m_socket; }
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


 private:
  /** whether this is a HTTP connection. */
  const bool m_isHttp;

  /** the @a TCPSocket for communication. */
  TCPSocket* m_socket;

  /** the reference to the @a NetMessage @a Queue. */
  Queue<NetMessage*>* m_netQueue;

  /** notification object for shutdown procedure. */
  Notify m_notify;

  /** the ID of this connection. */
  int m_id;

  /** the IF of the last opened connection. */
  static int m_ids;
};

/**
 * class network which listening on tcp socket for incoming connections.
 */
class Network : public Thread {
 public:
  /**
   * create a network instance and listening for incoming connections.
   * @param local true to accept connections only for local host.
   * @param port the port to listen for command line connections.
   * @param httpPort the port to listen for HTTP connections, or 0.
   * @param netQueue the reference to the @a NetMessage @a Queue.
   */
  Network(const bool local, const uint16_t port, const uint16_t httpPort, Queue<NetMessage*>* netQueue);

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

  /** the reference to the @a NetMessage @a Queue. */
  Queue<NetMessage*>* m_netQueue;

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

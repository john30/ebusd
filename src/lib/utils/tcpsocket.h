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

#ifndef LIB_UTILS_TCPSOCKET_H_
#define LIB_UTILS_TCPSOCKET_H_

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdint.h>
#include <string>

/** typedef for referencing @a sockaddr_in within namespace. */
typedef struct sockaddr_in socketaddress;

namespace ebusd {

/** \file lib/utils/tcpsocket.h */

using std::string;

#ifdef __MACH__
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif
#endif

/**
 * Connect a TCP or UDP socket.
 * @param server the server name or ip address to connect to.
 * @param port the port number.
 * @param udp true for UDP, false for TCP.
 * @param storeAddress optional pointer to where the socket address will be stored.
 * @param tcpConnectTimeout the TCP connect timeout in seconds, or 0.
 * @param tcpKeepAliveInterval optional interval in seconds for sending TCP keepalive.
 * @return the connected socket file descriptor on success, or -1 on error.
 */
int socketConnect(const char* server, uint16_t port, bool udp, socketaddress* storeAddress = nullptr,
int tcpConnectTimeout = 0, int tcpKeepAliveInterval = 0);


/**
 * Class for low level TCP socket operations (open, close, send, receive).
 */
class TCPSocket {
  friend class TCPServer;

 private:
  /**
   * Constructor.
   * @param sfd the file descriptor of tcp socket.
   * @param address struct which holds the ip address.
   */
  TCPSocket(int sfd, socketaddress* address);

 public:
  /**
   * Destructor.
   */
  ~TCPSocket() { close(m_sfd); }

  /**
   * initiate a tcp socket connection to a listening server.
   * @param server the server name or ip address to connect.
   * @param port the tcp port.
   * @param timeout the connect, send, and receive timeout in seconds, or 0.
   * @return pointer to an opened tcp socket.
   */
  static TCPSocket* connect(const string& server, const uint16_t& port, int timeout = 0);

  /**
   * Write bytes to opened file descriptor.
   * @param buffer data to send.
   * @param len number of bytes to send.
   * @return number of written bytes or -1 if an error has occured.
   */
  ssize_t send(const char* buffer, size_t len) { return ::send(m_sfd, buffer, len, MSG_NOSIGNAL); }

  /**
   * Read bytes from opened file descriptor.
   * @param buffer for received bytes.
   * @param len size of the receive buffer.
   * @return number of read bytes or -1 if an error has occured.
   */
  ssize_t recv(char* buffer, size_t len) { return ::recv(m_sfd, buffer, len, 0); }

  /**
   * Return the TCP port.
   * @return the TCP port.
   */
  uint16_t getPort() const { return m_port; }

  /**
   * Return the IP address.
   * @return the IP address.
   */
  const string& getIP() const { return m_ip; }

  /**
   * Return the file descriptor.
   * @return the file descriptor.
   */
  int getFD() const { return m_sfd; }

  /**
   * Return whether the file descriptor is valid.
   * @return true if file descriptor is valid.
   */
  bool isValid();

  /**
   * Set the timeout for @a send and @a recv.
   * @param timeout the timeout in seconds.
   */
  void setTimeout(int timeout) {
    struct timeval t;
    t.tv_usec = 0;
    t.tv_sec = timeout;
    setsockopt(m_sfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    setsockopt(m_sfd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
  }

 private:
  /** the file descriptor of the socket */
  int m_sfd;

  /** the port of the socket */
  uint16_t m_port;

  /** the IP address of the socket */
  string m_ip;
};

/**
 * class for a TCP based network server.
 */
class TCPServer {
 public:
  /**
   * creates a new instance of a listening tcp server.
   * @param port the tcp port.
   * @param address the ip address.
   */
  TCPServer(const uint16_t port, const string address)
    : m_lfd(0), m_port(port), m_address(address), m_listening(false) {}

  /**
   * destructor.
   */
  ~TCPServer() { if (m_lfd > 0) {close(m_lfd);} }

  /**
   * start listening of tcp socket.
   * @return result of low level functions.
   */
  int start();

  /**
   * accept an incoming tcp connection and create a local tcp socket for communication.
   * @return pointer to an opened tcp socket.
   */
  TCPSocket* newSocket();

  /**
   * returns the file descriptor.
   * @return the file descriptor.
   */
  int getFD() const { return m_lfd; }


 private:
  /** file descriptor from listening tcp socket */
  int m_lfd;

  /** listening tcp port */
  uint16_t m_port;

  /** listening tcp socket ip address */
  string m_address;

  /** true if object is already listening */
  bool m_listening;
};

}  // namespace ebusd

#endif  // LIB_UTILS_TCPSOCKET_H_

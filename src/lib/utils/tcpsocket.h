/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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
 * Class for low level tcp socket operations. (open, close, send, receive).
 */
class TCPSocket {
 public:
  /** grant access for friend class TCPClient */
  friend class TCPClient;

  /** grant access for friend class TCPServer */
  friend class TCPServer;

  /**
   * destructor.
   */
  ~TCPSocket() { close(m_sfd); }

  /**
   * write bytes to opened file descriptor.
   * @param buffer data to send.
   * @param len number of bytes to send.
   * @return number of written bytes or -1 if an error has occured.
   */
  ssize_t send(const char* buffer, size_t len) { return ::send(m_sfd, buffer, len, MSG_NOSIGNAL); }

  /**
   * read bytes from opened file descriptor.
   * @param buffer for received bytes.
   * @param len size of the receive buffer.
   * @return number of read bytes or -1 if an error has occured.
   */
  ssize_t recv(char* buffer, size_t len) { return ::recv(m_sfd, buffer, len, 0); }

  /**
   * returns the tcp port.
   * @return the tcp port.
   */
  uint16_t getPort() const { return m_port; }

  /**
   * returns the ip address.
   * @return the ip address.
   */
  string getIP() const { return m_ip; }

  /**
   * returns the file descriptor.
   * @return the file descriptor.
   */
  int getFD() const { return m_sfd; }

  /**
   * returns status of file descriptor.
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
    setsockopt(m_sfd, SO_RCVTIMEO, SO_REUSEADDR, &t, sizeof(t));
    setsockopt(m_sfd, SO_SNDTIMEO, SO_REUSEADDR, &t, sizeof(t));
  }

 private:
  /** file descriptor from tcp socket */
  int m_sfd;

  /** port of tcp socket */
  uint16_t m_port;

  /** ip address of tcp socket */
  string m_ip;

  /**
   * private constructor, limited access only for friend classes.
   * @param sfd the file descriptor of tcp socket.
   * @param address struct which holds the ip address.
   */
  TCPSocket(int sfd, socketaddress* address);
};

/**
 * class to initiate a tcp socket connection to a listening server.
 */
class TCPClient {
 public:
  /**
   * initiate a tcp socket connection to a listening server.
   * @param server the server name or ip address to connect.
   * @param port the tcp port.
   * @return pointer to an opened tcp socket.
   */
  TCPSocket* connect(const string& server, const uint16_t& port);
};

/**
 * class for a tcp based network server.
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

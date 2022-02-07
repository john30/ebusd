/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2018-2022 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_UTILS_HTTPCLIENT_H_
#define LIB_UTILS_HTTPCLIENT_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <cstdint>
#include <string>
#include "lib/utils/tcpsocket.h"

#ifdef HAVE_SSL
#  include <openssl/ssl.h>
#  include <openssl/bio.h>
#  include <openssl/err.h>
#endif

/** typedef for referencing @a sockaddr_in within namespace. */
typedef struct sockaddr_in socketaddress;

namespace ebusd {

/** \file lib/utils/httpclient.h */

using std::string;
using std::ifstream;

#ifdef HAVE_SSL
class SSLSocket {
 private:
  /**
   * Constructor.
   * @param ctx the SSL_CTX for cleanup, or nullptr.
   * @param bio the BIO instance, or nullptr.
   * @param until the system time until the socket is allowed to be used.
   */
  SSLSocket(SSL_CTX *ctx, BIO *bio, time_t until) : m_ctx(ctx), m_bio(bio), m_until(until) {}

 public:
  /**
   * Destructor.
   */
  virtual ~SSLSocket();

  /**
   * Connect to the host on the specified port.
   * @param host the host name or ip address to connect to.
   * @param port the port number.
   * @param https true for HTTPS, false for HTTP.
   * @param timeout the connect, send, and receive timeout in seconds, or 0 for blocking mode.
   * @return the connected SSLSocket, or nullptr on error.
   */
  static SSLSocket* connect(const string& server, const uint16_t& port, const bool https, int timeout = 0);

  /**
   * Write bytes to the socket.
   * @param data the data to send.
   * @param len number of bytes to send.
   * @return number of bytes written, or -1 on error.
   */
  ssize_t send(const char* data, size_t len);

  /**
   * Read bytes from the socket.
   * @param data the buffer for the received bytes.
   * @param len size of the buffer.
   * @return number of bytes read, or -1 on error.
   */
  ssize_t recv(char* data, size_t len);

  /**
   * Return whether the socket is still valid.
   * @return true if the socket is still valid.
   */
  bool isValid();

 private:
  /** the SSL_CTX for cleanup, or nullptr. */
  SSL_CTX *m_ctx;

  /** the BIO instance for communication. */
  BIO *m_bio;

  /** the system time until the socket is allowed to be used. */
  const time_t m_until;
};

#define SocketClass SSLSocket

#else // HAVE_SSL

#define SocketClass TCPSocket

#endif // HAVE_SSL

/**
 * Helper class for handling HTTP client requests.
 */
class HttpClient {
 public:
  /**
   * Constructor.
   */
  HttpClient() : m_socket(nullptr), m_port(0), m_timeout(0), m_bufferSize(0), m_buffer(nullptr) {}

  /**
   * Destructor.
   */
  ~HttpClient() {
    disconnect();
    if (m_buffer) {
      free(m_buffer);
      m_buffer = nullptr;
    }
  }

  /**
   * Initialize HttpClient.
   */
  static void initialize();

  /**
   * Parse an HTTP URL.
   * @param url the URL to parse.
   * @param proto the extracted protocol.
   * @param host the extracted host name.
   * @param port the extracted port (or default).
   * @param uri the extracted URI starting with '/'.
   * @return true on success, false on failure.
   */
  static bool parseUrl(const string& url, string* proto, string* host, uint16_t* port, string* uri);

  /**
   * Connect to the specified server.
   * @param host the host name to connect to.
   * @param port the port to connect to.
   * @param https true for HTTPS, false for HTTP.
   * @param timeout the timeout in seconds, defaults to 5 seconds.
   * @param userAgent the optional user agent to send in the request header.
   * @return true on success, false on connect failure.
   */
  bool connect(const string& host, uint16_t port, const bool https = false, const string& userAgent = "", int timeout = 5);

  /**
   * Re-connect to the last specified server.
   * @return true on success, false on connect failure.
   */
  bool reconnect();

  /**
   * Ensure the client is connected to the last specified server.
   * @return true if still connected or connection was re-established successfully, false on connect failure.
   */
  bool ensureConnected();

  /**
   * Disconnect from the server.
   */
  void disconnect();

  /**
   * Execute a GET request.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @param time optional pointer to a @a time_t value for storing the modification time of the file, or nullptr.
   * @return true on success, false on error.
   */
  bool get(const string& uri, const string& body, string* response, time_t* time = nullptr);

  /**
   * Execute a POST request.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @return true on success, false on error.
   */
  bool post(const string& uri, const string& body, string* response);

  /**
   * Execute an arbitrary request.
   * @param method the method string.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @param time optional pointer to a @a time_t value for storing the modification time of the file, or nullptr.
   * @return true on success, false on error.
   */
  bool request(const string& method, const string& uri, const string& body, string* response, time_t* time = nullptr);

 private:
  /**
   * Read from the connected socket until the specified delimiter is found or the specified number of bytes was received.
   * @param delim the delimiter to find, or empty for reading the specified number of bytes.
   * @param length the maximum number of bytes to receive.
   * @param result the string to append the read data to and in which to find the delimiter.
   * @return the position of the delimiter if delimiter was set or the number of bytes received, or string::npos if not found.
   */
  size_t readUntil(const string& delim, const size_t length, string* result);

 private:
  /** the currently connected socket. */
  SocketClass* m_socket;

  /** the name of the host last successfully connected to. */
  string m_host;

  /** the port last successfully connected to. */
  uint16_t m_port;

  /** the timeout in seconds. */
  int m_timeout;

  /** the optional user agent to send in the request header. */
  string m_userAgent;

  /** the size of the @a m_buffer. */
  size_t m_bufferSize;

  /** the buffer for preparing/receiving data. */
  char* m_buffer;
};

}  // namespace ebusd

#endif  // LIB_UTILS_HTTPCLIENT_H_

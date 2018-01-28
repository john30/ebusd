/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2018 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_UTILS_HTTP_H_
#define LIB_UTILS_HTTP_H_

#include <unistd.h>
#include <cstdint>
#include <string>
#include "lib/utils/tcpsocket.h"


/** typedef for referencing @a sockaddr_in within namespace. */
typedef struct sockaddr_in socketaddress;

namespace ebusd {

/** \file lib/utils/http.h */

using std::string;
using std::ifstream;

/**
 * Helper class for handling HTTP client requests.
 */
class HttpClient {

 public:
  /**
   * Constructor.
   */
  HttpClient() : m_socket(nullptr), m_bufferSize(0), m_buffer(nullptr) {}

  /**
   * Destructor.
   */
  ~HttpClient() {
    if (m_socket) {
      delete m_socket;
      m_socket = nullptr;
    }
    if (m_buffer) {
      free(m_buffer);
      m_buffer = nullptr;
    }
  }

  /**
   * Connect to the specified server.
   * @param host the host name to connect to.
   * @param port the port to connect to.
   * @param timeout the timeout in seconds, defaults to 5 seconds.
   * @param userAgent the optional user agent to send in the request header.
   * @return true on success, false on connect failure.
   */
  bool connect(const string& host, uint16_t port, const string& userAgent = "", int timeout = 5);

  /**
   * Execute a GET request.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @return true on success, false on error.
   */
  bool get(const string& uri, const string& body, string& response);

  /**
   * Execute a POST request.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @return true on success, false on error.
   */
  bool post(const string& uri, const string& body, string& response);

  /**
   * Execute an arbitrary request.
   * @param uri the URI string.
   * @param body the optional body to send.
   * @param response the response body from the server (or the HTTP header on error).
   * @return true on success, false on error.
   */
  bool request(const string& method, const string& uri, const string& body, string& response);

private:
  TCPClient m_client;
  string m_host;
  string m_userAgent;
  TCPSocket* m_socket;
  size_t m_bufferSize;
  char* m_buffer;
};

}  // namespace ebusd

#endif  //LIB_UTILS_HTTP_H_

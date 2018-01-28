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

#include "lib/utils/httpclient.h"
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <ios>

namespace ebusd {

using std::string;
using std::ostringstream;
using std::dec;
using std::hex;

bool HttpClient::connect(const string& host, const uint16_t port, const string& userAgent, const int timeout) {
  if (m_socket) {
    delete m_socket;
    m_socket = nullptr;
  }
  TCPSocket* socket = m_client.connect(host, port);
  if (!socket) {
    return false;
  }
  socket->setTimeout(timeout);
  m_socket = socket;
  m_host = host;
  m_userAgent = userAgent;
  return true;
}

bool HttpClient::get(const string& uri, const string& body, string& response) {
  return request("GET", uri, body, response);
}

bool HttpClient::post(const string& uri, const string& body, string& response) {
  return request("POST", uri, body, response);
}

bool HttpClient::request(const string& method, const string& uri, const string& body, string& response) {
  if (!m_socket) {
    response = "not connected";
    return false;
  }
  ostringstream ostr;
  ostr << method << " " << uri << " HTTP/1.0\r\n"
       << "Host: " << m_host << "\r\n";
  if (!m_userAgent.empty()) {
    ostr << "User-Agent: " << m_userAgent << "\r\n";
  }
  if (body.empty()) {
    ostr << "\r\n";
  } else {
    ostr << "Content-Type: application/json; charset=utf-8\r\n"
         << "Content-Length: " << dec << body.length() << "\r\n"
         << "\r\n"
         << body;
  }
  string str = ostr.str();
  size_t len = str.size();
  const char* cstr = str.c_str();
  for (size_t pos = 0; pos < len; ) {
    ssize_t sent = m_socket->send(cstr + pos, len - pos);
    if (sent < 0) {
      response = "send error";
      return false;
    }
    pos += sent;
  }
  if (!m_buffer) {
    m_buffer = (char *) malloc(1024);
    if (!m_buffer) {
      response = "memory allocation";
      return false;
    }
    m_bufferSize = 1024;
  }
  ssize_t received = m_socket->recv(m_buffer, m_bufferSize);
  if (received <= 0) {
    response = "receive error";
    return false;
  }
  string result = string(m_buffer, 0, static_cast<unsigned>(received)); // expect "HTTP/1.1 200 OK"
  size_t pos = result.find(' ');
  if (result.substr(0, 5) != "HTTP/" || pos == string::npos || pos > 8) {
    response = "receive error (headers)";
    return false;
  }
  if (result.substr(pos+1, 6) != "200 OK") {
    size_t endpos = result.find("\r\n", pos+1);
    response = "receive error: " + result.substr(pos+1, endpos == string::npos ? endpos : endpos-pos-1);
    return false;
  }
  pos = result.find("\r\n\r\n");
  while (pos == string::npos && result.length() < 256*1024) {
    received = m_socket->recv(m_buffer, m_bufferSize);
    if (received < 0) {
      response = "receive error";
      return false;
    }
    if (received == 0) {
      break;
    }
    size_t oldLength = result.length();
    result += string(m_buffer, 0, static_cast<unsigned>(received));
    pos = result.find("\r\n\r\n", oldLength - 3);
  }
  if (pos == string::npos) {
    response = "receive error (headers)";
    return false;
  }
  response = result.substr(pos+4);
  return true;
}

}  // namespace ebusd

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

namespace ebusd {

using std::string;
using std::ostringstream;
using std::dec;
using std::hex;

bool HttpClient::parseUrl(const string& url, string& proto, string& host, uint16_t& port, string& uri) {
  size_t hostPos = url.find("://");
  if (hostPos == string::npos) {
    return false;
  }
  proto = url.substr(0, hostPos);
  hostPos += 3;
  if (proto != "http") {
    return false;
  }
  size_t pos = url.find('/', hostPos);
  if (pos == hostPos) {
    return false;
  }
  if (pos == string::npos) {
    host = url.substr(hostPos);
    uri = "/";
  } else {
    host = url.substr(hostPos, pos - hostPos);
    uri = url.substr(pos);
    if (uri[uri.length()-1] != '/') {
      uri += "/";
    }
  }
  pos = host.find(':');
  if (pos == 0) {
    return false;
  }
  port = 80;
  if (pos != string::npos) {
    char* strEnd = NULL;
    unsigned long value = strtoul(host.c_str()+pos+1, &strEnd, 10);
    if (strEnd == NULL || *strEnd != '\0' || value < 1 || value > 65535) {
      return false;
    }
    port = static_cast<uint16_t>(value);
    host = host.substr(0, pos);
  }
  return true;
}

bool HttpClient::connect(const string& host, const uint16_t port, const string& userAgent, const int timeout) {
  disconnect();
  m_socket = m_client.connect(host, port);
  if (!m_socket) {
    return false;
  }
  m_socket->setTimeout(timeout);
  m_host = host;
  m_port = port;
  m_timeout = timeout;
  m_userAgent = userAgent;
  return true;
}

bool HttpClient::reconnect() {
  disconnect();
  if (m_host.empty() || !m_port) {
    return false;
  }
  m_socket = m_client.connect(m_host, m_port);
  if (!m_socket) {
    return false;
  }
  m_socket->setTimeout(m_timeout);
  return true;
}

bool HttpClient::ensureConnected() {
  if (m_socket && m_socket->isValid()) {
    return true;
  }
  return reconnect();
}

void HttpClient::disconnect() {
  if (m_socket) {
    delete m_socket;
    m_socket = nullptr;
  }
}

bool HttpClient::get(const string& uri, const string& body, string& response) {
  return request("GET", uri, body, response);
}

bool HttpClient::post(const string& uri, const string& body, string& response) {
  return request("POST", uri, body, response);
}

bool HttpClient::request(const string& method, const string& uri, const string& body, string& response) {
  if (!ensureConnected()) {
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
      disconnect();
      response = "send error";
      return false;
    }
    pos += sent;
  }
  string result;
  size_t pos = readUntil(" ", 4 * 1024, result); // max 4k headers
  if (pos == string::npos || pos > 8 || result.substr(0, 5) != "HTTP/") {
    disconnect();
    response = "receive error (headers)";
    return false;
  }
  if (result.substr(pos+1, 6) != "200 OK") {
    disconnect();
    size_t endpos = result.find("\r\n", pos+1);
    response = "receive error: " + result.substr(pos+1, endpos == string::npos ? endpos : endpos-pos-1);
    return false;
  }
  pos = readUntil("\r\n\r\n", 4 * 1024, result); // max 4k headers
  if (pos == string::npos) {
    disconnect();
    response = "receive error (headers)";
    return false;
  }
  string headers = result.substr(0, pos+2); // including final \r\n
  response = result.substr(pos+4);
  pos = headers.find("Content-Length: "); // 16 chars
  if (pos == string::npos) {
    disconnect();
    return true;
  }
  char* strEnd = NULL;
  unsigned long length = strtoul(headers.c_str()+pos+16, &strEnd, 10);
  if (strEnd == NULL || *strEnd != '\r') {
    disconnect();
    response = "invalid content length ";
    return false;
  }
  pos = readUntil("", length, response);
  disconnect();
  return pos == length;
}

size_t HttpClient::readUntil(const string& delim, const size_t length, string& result) {
  if (!m_buffer) {
    m_buffer = (char*)malloc(1024);
    if (!m_buffer) {
      return string::npos;
    }
    m_bufferSize = 1024;
  }
  bool findDelim = !delim.empty();
  size_t pos = findDelim ? result.find(delim) : string::npos;
  while (pos == string::npos && result.length() < length) {
    ssize_t received = m_socket->recv(m_buffer, m_bufferSize);
    if (received < 0) {
      return string::npos;
    }
    if (received == 0) {
      break;
    }
    size_t oldLength = result.length();
    result += string(m_buffer, 0, static_cast<unsigned>(received));
    if (findDelim) {
      pos = result.find(delim, oldLength - (delim.length() - 1));
    }
  }
  return findDelim ? pos : result.length();
}

}  // namespace ebusd

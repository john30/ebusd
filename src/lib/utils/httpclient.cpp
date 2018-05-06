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

bool HttpClient::parseUrl(const string& url, string* proto, string* host, uint16_t* port, string* uri) {
  size_t hostPos = url.find("://");
  if (hostPos == string::npos) {
    return false;
  }
  *proto = url.substr(0, hostPos);
  hostPos += 3;
  if (*proto != "http") {
    return false;
  }
  size_t pos = url.find('/', hostPos);
  if (pos == hostPos) {
    return false;
  }
  if (pos == string::npos) {
    *host = url.substr(hostPos);
    *uri = "/";
  } else {
    *host = url.substr(hostPos, pos - hostPos);
    *uri = url.substr(pos);
    if ((*uri)[uri->length()-1] != '/') {
      *uri += "/";
    }
  }
  pos = host->find(':');
  if (pos == 0) {
    return false;
  }
  *port = 80;
  if (pos != string::npos) {
    char* strEnd = nullptr;
    unsigned long value = strtoul(host->c_str()+pos+1, &strEnd, 10);
    if (strEnd == nullptr || *strEnd != '\0' || value < 1 || value > 65535) {
      return false;
    }
    *port = static_cast<uint16_t>(value);
    *host = host->substr(0, pos);
  }
  return true;
}

bool HttpClient::connect(const string& host, const uint16_t port, const string& userAgent, const int timeout) {
  disconnect();
  m_socket = m_client.connect(host, port, timeout);
  if (!m_socket) {
    return false;
  }
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
  m_socket = m_client.connect(m_host, m_port, m_timeout);
  if (!m_socket) {
    return false;
  }
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

bool HttpClient::get(const string& uri, const string& body, string* response, time_t* time) {
  return request("GET", uri, body, response, time);
}

bool HttpClient::post(const string& uri, const string& body, string* response) {
  return request("POST", uri, body, response);
}

const int indexToMonth[] = {
  -1, -1,  2, 12, -1, -1,  1, -1,  // 0-7
  -1, -1, -1, -1,  7, -1,  6,  8,  // 8-15
   9,  5,  3, -1, 10, -1, 11, -1,  // 16-23
  -1, -1,  4, -1, -1, -1, -1, -1,  // 24-31
};

bool HttpClient::request(const string& method, const string& uri, const string& body, string* response, time_t* time) {
  if (!ensureConnected()) {
    *response = "not connected";
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
      *response = "send error";
      return false;
    }
    pos += sent;
  }
  string result;
  size_t pos = readUntil(" ", 4 * 1024, &result);  // max 4k headers
  if (pos == string::npos || pos > 8 || result.substr(0, 5) != "HTTP/") {
    disconnect();
    *response = "receive error (headers)";
    return false;
  }
  if (result.substr(pos+1, 6) != "200 OK") {
    disconnect();
    size_t endpos = result.find("\r\n", pos+1);
    *response = "receive error: " + result.substr(pos+1, endpos == string::npos ? endpos : endpos-pos-1);
    return false;
  }
  pos = readUntil("\r\n\r\n", 4 * 1024, &result);  // max 4k headers
  if (pos == string::npos) {
    disconnect();
    *response = "receive error (headers)";
    return false;
  }
  string headers = result.substr(0, pos+2);  // including final \r\n
  const char* hdrs = headers.c_str();
  *response = result.substr(pos+4);
  if (time) {
    pos = headers.find("\r\nLast-Modified: ");
    if (pos != string::npos && headers.substr(pos+42, 4) == " GMT") {
      // Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT
      struct tm t;
      pos += strlen("\r\nLast-Modified: ") + 5;
      char* strEnd = nullptr;
      t.tm_mday = (int)strtol(hdrs + pos, &strEnd, 10);
      if (strEnd != hdrs + pos + 2 || t.tm_mday < 1 || t.tm_mday > 31) {
        t.tm_mday = -1;
      }
      t.tm_mon = indexToMonth[((hdrs[pos+4]&0x10)>>1) | (hdrs[pos+5]&0x17)] - 1;
      strEnd = nullptr;
      t.tm_year = (int)strtol(hdrs + pos + 7, &strEnd, 10);
      if (strEnd != hdrs + pos + 11 || t.tm_year < 1970 || t.tm_year >= 3000) {
        t.tm_year = -1;
      } else {
        t.tm_year -= 1900;
      }
      strEnd = nullptr;
      t.tm_hour = (int)strtol(hdrs + pos + 12, &strEnd, 10);
      if (strEnd != hdrs + pos + 14 || t.tm_hour > 23) {
        t.tm_hour = -1;
      }
      strEnd = nullptr;
      t.tm_min = (int)strtol(hdrs + pos + 15, &strEnd, 10);
      if (strEnd != hdrs + pos + 17 || t.tm_min > 59) {
        t.tm_min = -1;
      }
      strEnd = nullptr;
      t.tm_sec = (int)strtol(hdrs + pos + 18, &strEnd, 10);
      if (strEnd != hdrs + pos + 20 || t.tm_sec > 59) {
        t.tm_sec = -1;
      }
      if (t.tm_mday > 0 && t.tm_mon >= 0 && t.tm_year >= 0 && t.tm_hour >= 0 && t.tm_min >=0 && t.tm_sec >= 0) {
        *time = timegm(&t);
      }
    }
  }
  pos = headers.find("\r\nContent-Length: ");
  if (pos == string::npos) {
    disconnect();
    return true;
  }
  char* strEnd = nullptr;
  unsigned long length = strtoul(hdrs + pos + strlen("\r\nContent-Length: "), &strEnd, 10);
  if (strEnd == nullptr || *strEnd != '\r') {
    disconnect();
    *response = "invalid content length ";
    return false;
  }
  pos = readUntil("", length, response);
  disconnect();
  return pos == length;
}

size_t HttpClient::readUntil(const string& delim, const size_t length, string* result) {
  if (!m_buffer) {
    m_buffer = (char*)malloc(1024);
    if (!m_buffer) {
      return string::npos;
    }
    m_bufferSize = 1024;
  }
  bool findDelim = !delim.empty();
  size_t pos = findDelim ? result->find(delim) : string::npos;
  while (pos == string::npos && result->length() < length) {
    ssize_t received = m_socket->recv(m_buffer, m_bufferSize);
    if (received < 0) {
      return string::npos;
    }
    if (received == 0) {
      break;
    }
    size_t oldLength = result->length();
    *result += string(m_buffer, 0, static_cast<unsigned>(received));
    if (findDelim) {
      pos = result->find(delim, oldLength - (delim.length() - 1));
    }
  }
  return findDelim ? pos : result->length();
}

}  // namespace ebusd

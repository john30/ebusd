/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2018-2024 John Baier <ebusd@ebusd.eu>
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
#include <csignal>
#include <algorithm>
#ifdef HAVE_SSL
#if OPENSSL_VERSION_NUMBER < 0x10101000L
#include <sys/stat.h>
#endif
#endif  // HAVE_SSL
#include "lib/utils/log.h"

#if defined(__GNUC__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

namespace ebusd {

using std::string;
using std::ostringstream;
using std::dec;

#ifdef HAVE_SSL

#if OPENSSL_VERSION_NUMBER < 0x10101000L
//  default CA path
#define DEFAULT_CAFILE "/etc/ssl/certs/ca-certificates.crt"

//  default CA path
#define DEFAULT_CAPATH "/etc/ssl/certs"
#endif

// the time slice to sleep between repeated SSL reads/writes
#define SLEEP_NANOS 20000

bool checkError(const char* call) {
  unsigned long err = ERR_get_error();
  if (err) {
    const char *const str = ERR_reason_error_string(err);
    logError(lf_network, "HTTP %s: %ld=%s", call, err, str);
    return true;
  }
  return false;
}

bool isError(const char* call, bool result) {
  if (checkError(call)) {
    return true;
  }
  if (!result) {
    logError(lf_network, "HTTP %s: invalid result", call);
    return true;
  }
  return false;
}

bool isError(const char* call, long result, long expected) {
  if (checkError(call)) {
    return true;
  }
  if (result != expected) {
    logError(lf_network, "HTTP %s: invalid result %d", call, result);
    return true;
  }
  return false;
}


SSLSocket::~SSLSocket() {
  BIO_free_all(m_bio);
}

ssize_t SSLSocket::send(const char* data, size_t len) {
  time_t now = 0;
  do {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    size_t part = 0;
    int res = BIO_write_ex(m_bio, data, len, &part);
    if (res == 1) {
      return static_cast<signed>(part);
    }
#else
    int res = BIO_write(m_bio, data, static_cast<int>(len));
    if (res > 0) {
      return static_cast<ssize_t>(res);
    }
#endif
    if (!BIO_should_retry(m_bio) && now > 0) {  // always repeat on first failure
      if (isError("send", true)) {
        return -1;
      }
      return 0;
    }
    if ((now=time(nullptr)) > m_until) {
      logError(lf_network, "HTTP send: timed out after %d sec", now-m_until);
      break;
    }
    usleep(SLEEP_NANOS);
  } while (true);
  return -1;  // timeout
}

ssize_t SSLSocket::recv(char* data, size_t len) {
  time_t now = 0;
  do {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    size_t part = 0;
    int res = BIO_read_ex(m_bio, data, len, &part);
    if (res == 1) {
      return static_cast<signed>(part);
    }
#else
    int res = BIO_read(m_bio, data, static_cast<int>(len));
    if (res > 0) {
      return static_cast<ssize_t>(res);
    }
#endif
    if (!BIO_should_retry(m_bio) && now > 0) {  // always repeat on first failure
      if (isError("recv", true)) {
        return -1;
      }
      return 0;
    }
    if ((now=time(nullptr)) > m_until) {
      logError(lf_network, "HTTP recv: timed out after %d sec", now-m_until);
      break;
    }
    usleep(SLEEP_NANOS);
  } while (true);
  return -1;  // timeout
}

bool SSLSocket::isValid() {
  return time(nullptr) < m_until && !BIO_eof(m_bio);
}

void sslInfoCallback(const SSL *ssl, int type, int val) {
  if (!needsLog(lf_network, (val == 0) ? ll_error : ll_debug)) {
    return;
  }
  logWrite(lf_network,
    (val == 0) ? ll_error : ll_debug,
    "SSL state %d=%s: type 0x%x=%s%s%s%s%s%s%s%s%s val %d=%s",
    SSL_get_state(ssl),
    SSL_state_string_long(ssl),
    type,
    (type & SSL_CB_LOOP) ? "loop," : "",
    (type & SSL_CB_EXIT) ? "exit," : "",
    (type & SSL_CB_READ) ? "read," : "",
    (type & SSL_CB_WRITE) ? "write," : "",
    (type & SSL_CB_ALERT) ? "alert," : "",
    (type & SSL_ST_ACCEPT) ? "accept," : "",
    (type & SSL_ST_CONNECT) ? "connect," : "",
    (type & SSL_CB_HANDSHAKE_START) ? "start," : "",
    (type & SSL_CB_HANDSHAKE_DONE) ? "done," : "",
    val,
    (type & SSL_CB_ALERT) ? SSL_alert_desc_string_long(val) : "?");
}

int bioInfoCallback(MAYBE_UNUSED BIO *bio, int state, int res) {
  logDebug(lf_network,
    "SSL BIO state %d res %d",
    state,
    res);
  return 1;
}

SSLSocket* SSLSocket::connect(const string& host, const uint16_t& port, bool https, int timeout, const char* caFile,
                              const char* caPath) {
  ostringstream ostr;
  ostr << host << ':' << static_cast<unsigned>(port);
  const string hostPort = ostr.str();
  time_t until = time(nullptr) + 1 + (timeout <= 5 ? 5 : timeout);  // at least 5 seconds, 1 extra for rounding
  if (!https) {
    do {
      BIO *bio = BIO_new_connect(static_cast<const char*>(hostPort.c_str()));
      if (isError("connect", bio != nullptr)) {
        break;
      }
      BIO_set_nbio(bio, 1);  // set non-blocking
      return new SSLSocket(bio, until);
    } while (false);
    return nullptr;
  }
  BIO *bio = nullptr;
  static SSL_CTX *ctx = nullptr;
  static int sslContextInitTries = 0;
  do {
    // const SSL_METHOD *method = TLS_client_method();
    static bool verifyPeer = true;
    if (ctx == nullptr) {  // according to openssl manpage, ctx is global and should be created once only
      if (sslContextInitTries > 2) {  // give it up to 3 tries to initialize the context
        break;
      }
      sslContextInitTries++;
      const SSL_METHOD *method = SSLv23_method();
      if (isError("method", method != nullptr)) {
        break;
      }
      ctx = SSL_CTX_new(method);
      if (isError("ctx_new", ctx != nullptr)) {
        break;
      }
      SSL_CTX_set_info_callback(ctx, sslInfoCallback);
      verifyPeer = !caFile || strcmp(caFile, "#") != 0;
      SSL_CTX_set_verify(ctx, verifyPeer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
      if (verifyPeer) {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
        SSL_CTX_set_default_verify_paths(ctx);
#else
        struct stat stat_buf = {};
        if (!caFile && stat(DEFAULT_CAFILE, &stat_buf) == 0) {
          caFile = DEFAULT_CAFILE;  // use default CA file
        }
        if (!caPath && stat(DEFAULT_CAPATH, &stat_buf) == 0) {
          caPath = DEFAULT_CAPATH;  // use default CA path
        }
#endif
        if ((caFile || caPath) && isError("verify_loc", SSL_CTX_load_verify_locations(ctx, caFile, caPath), 1)) {
          SSL_CTX_free(ctx);
          ctx = nullptr;
          break;
        }
      }
      SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
    }
    bio = BIO_new_ssl_connect(ctx);
    if (isError("new_ssl_conn", bio != nullptr)) {
      // in this case the ctx seems to be in an invalid state and it never comes out of that again on its own
      SSL_CTX_free(ctx);
      ctx = nullptr;
      break;
    }
    BIO_set_info_callback(bio, bioInfoCallback);
    BIO_set_conn_ip_family(bio, 4);  // force IPv4 to circumvent docker IPv6 routing issues
    if (isError("conn_hostname", BIO_set_conn_hostname(bio, hostPort.c_str()), 1)) {
      break;
    }
    isError("set_nbio", BIO_set_nbio(bio, 1), 1);  // set non-blocking
    SSL *ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    if (isError("get_ssl", ssl != nullptr)) {
      break;
    }
    SSL_clear_mode(ssl, SSL_MODE_AUTO_RETRY);
    const char *hostname = host.c_str();
    if (isError("tls_host", SSL_set_tlsext_host_name(ssl, hostname), 1)) {
      break;
    }
    long res = BIO_do_connect(bio);
    time_t now = 0;
    while (res <= 0 && (BIO_should_retry(bio) || now == 0)) {  // always repeat on first failure
      if ((now=time(nullptr)) > until) {
        break;
      }
      usleep(SLEEP_NANOS);
      res = BIO_do_connect(bio);
    }
    if (res <= 0 && now > until) {
      logError(lf_network, "HTTP connect: timed out after %d sec", now-until);
      break;
    }
    if (isError("connect", res, 1)) {
      break;
    }
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (isError("peer_cert", cert != nullptr)) {
      break;
    }
    X509_free(cert);  // decrement reference count incremented by above call
    if (verifyPeer && isError("verify", SSL_get_verify_result(ssl), X509_V_OK)) {
      break;
    }
    // check hostname
    X509_NAME *sname = X509_get_subject_name(cert);
    if (isError("get_subject", sname != nullptr)) {
      break;
    }
    char peerName[64];
    if (isError("subject name", X509_NAME_get_text_by_NID(sname, NID_commonName, peerName, sizeof(peerName)) > 0)) {
      break;
    }
    if (strcmp(peerName, hostname) != 0) {
      char* dotpos = strchr(const_cast<char*>(hostname), '.');
      if (peerName[0] == '*' && peerName[1] == '.' && dotpos
        && strcmp(peerName+2, dotpos+1) == 0) {
        // wildcard matches
      } else if (dotpos && strcmp(peerName, dotpos+1) == 0) {
        // hostname matches
      } else if (isError("subject", 1, 0)) {
        break;
      }
    }
    return new SSLSocket(bio, until);
  } while (false);
  if (bio) {
    BIO_free_all(bio);
  }
  return nullptr;
}

bool HttpClient::s_initialized = false;
const char* HttpClient::s_caFile = nullptr;
const char* HttpClient::s_caPath = nullptr;

void HttpClient::initialize(const char* caFile, const char* caPath) {
  if (s_initialized) {
    return;
  }
  s_initialized = true;
  s_caFile = caFile;
  s_caPath = caPath;
  SSL_library_init();
  SSL_load_error_strings();
  signal(SIGPIPE, SIG_IGN);  // needed to avoid SIGPIPE when writing to a closed pipe
}
#else  // HAVE_SSL
void HttpClient::initialize(const char* caFile, const char* caPath) {
  // empty
}
#endif  // HAVE_SSL

bool HttpClient::parseUrl(const string& url, string* proto, string* host, uint16_t* port, string* uri) {
  size_t hostPos = url.find("://");
  if (hostPos == string::npos) {
    return false;
  }
  *proto = url.substr(0, hostPos);
  hostPos += 3;
  bool isSsl = *proto == "https";
#ifdef HAVE_SSL
  if (!isSsl && *proto != "http") {
    return false;
  }
#else  // HAVE_SSL
  if (*proto != "http") {
    return false;
  }
#endif  // HAVE_SSL
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
  *port = isSsl ? 443 : 80;
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

bool HttpClient::connect(const string& host, const uint16_t port, bool https, const string& userAgent,
                         const int timeout) {
  initialize();
  disconnect();
#ifdef HAVE_SSL
  m_socket = SSLSocket::connect(host, port, https, timeout, s_caFile, s_caPath);
  m_https = https;
#else  // HAVE_SSL
  if (https) {
    return false;
  }
  m_socket = TCPSocket::connect(host, port, timeout);
#endif  // HAVE_SSL
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
#ifdef HAVE_SSL
  m_socket = SSLSocket::connect(m_host, m_port, m_https, m_timeout, s_caFile, s_caPath);
#else  // HAVE_SSL
  m_socket = TCPSocket::connect(m_host, m_port, m_timeout);
#endif  // HAVE_SSL
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

bool HttpClient::get(const string& uri, const string& body, string* response, bool* repeatable,
time_t* time, bool* jsonString) {
  return request("GET", uri, body, response, repeatable, time, jsonString);
}

bool HttpClient::post(const string& uri, const string& body, string* response, bool* repeatable) {
  return request("POST", uri, body, response, repeatable);
}

const int indexToMonth[] = {
  -1, -1,  2, 12, -1, -1,  1, -1,  // 0-7
  -1, -1, -1, -1,  7, -1,  6,  8,  // 8-15
   9,  5,  3, -1, 10, -1, 11, -1,  // 16-23
  -1, -1,  4, -1, -1, -1, -1, -1,  // 24-31
};

bool HttpClient::request(const string& method, const string& uri, const string& body, string* response,
bool* repeatable, time_t* time, bool* jsonString) {
  if (!ensureConnected()) {
    *response = "not connected";
    if (repeatable) {
      *repeatable = true;
    }
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
      if (repeatable) {
        *repeatable = true;
      }
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
  transform(headers.begin(), headers.end(), headers.begin(), ::tolower);
  const char* hdrs = headers.c_str();
  *response = result.substr(pos+4);
#if defined(HAVE_TIME_H) && defined(HAVE_TIMEGM)
  if (time) {
    pos = headers.find("\r\nlast-modified: ");
    if (pos != string::npos && headers.substr(pos+42, 4) == " gmt") {
      // Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT
      struct tm t;
      pos += strlen("\r\nlast-modified: ") + 5;
      char* strEnd = nullptr;
      t.tm_mday = static_cast<int>(strtol(hdrs + pos, &strEnd, 10));
      if (strEnd != hdrs + pos + 2 || t.tm_mday < 1 || t.tm_mday > 31) {
        t.tm_mday = -1;
      }
      t.tm_mon = indexToMonth[((hdrs[pos+4]&0x10)>>1) | (hdrs[pos+5]&0x17)] - 1;
      strEnd = nullptr;
      t.tm_year = static_cast<int>(strtol(hdrs + pos + 7, &strEnd, 10));
      if (strEnd != hdrs + pos + 11 || t.tm_year < 1970 || t.tm_year >= 3000) {
        t.tm_year = -1;
      } else {
        t.tm_year -= 1900;
      }
      strEnd = nullptr;
      t.tm_hour = static_cast<int>(strtol(hdrs + pos + 12, &strEnd, 10));
      if (strEnd != hdrs + pos + 14 || t.tm_hour > 23) {
        t.tm_hour = -1;
      }
      strEnd = nullptr;
      t.tm_min = static_cast<int>(strtol(hdrs + pos + 15, &strEnd, 10));
      if (strEnd != hdrs + pos + 17 || t.tm_min > 59) {
        t.tm_min = -1;
      }
      strEnd = nullptr;
      t.tm_sec = static_cast<int>(strtol(hdrs + pos + 18, &strEnd, 10));
      if (strEnd != hdrs + pos + 20 || t.tm_sec > 59) {
        t.tm_sec = -1;
      }
      if (t.tm_mday > 0 && t.tm_mon >= 0 && t.tm_year >= 0 && t.tm_hour >= 0 && t.tm_min >=0 && t.tm_sec >= 0) {
        *time = timegm(&t);
      }
    }
  }
#endif
  bool isJson = headers.find("\r\ncontent-type: application/json") != string::npos;
  pos = headers.find("\r\ncontent-length: ");
  bool noLength = pos == string::npos;
  if (noLength && !isJson) {
    disconnect();
    if (jsonString) {
      *jsonString = false;
    }
    return true;
  }
  char* strEnd = nullptr;
  unsigned long length = 4*1024;  // default max length
  if (!noLength) {
    length = strtoul(hdrs + pos + strlen("\r\ncontent-length: "), &strEnd, 10);
    if (strEnd == nullptr || *strEnd != '\r') {
      disconnect();
      *response = "invalid content length ";
      return false;
    }
  }
  pos = readUntil("", length, response);
  disconnect();
  if (noLength ? pos < 1 : pos != length) {
    return false;
  }
  if (noLength) {
    length = pos;
  }
  if (jsonString && isJson && *jsonString && length >= 2 && response->at(0) == '"') {
    // check for inline conversion of JSON to string expecting a single string to de-escape
    pos = length;
    while (pos > 1 && (response->at(pos-1) == '\r' || response->at(pos-1) == '\n')) {
      pos--;
    }
    if (pos > 2 && response->at(pos-1) == '"') {
      response->erase(pos-1);
      response->erase(0, 1);
      size_t from = 0;
      while ((pos = response->find_first_of("\\", from)) != string::npos) {
        response->erase(pos, 1);  // pos is now pointing at the char behind the backslash
        switch (response->at(pos)) {
          case 'r':
            response->erase(pos, 1);  // removed
            from = pos;
            continue;
          case 'n':
            (*response)[pos] = '\n';  // replaced
            from = pos+1;
            break;
          default:
            from = pos+1;
            break;  // kept
        }
      }
      isJson = false;
    }
  }
  if (jsonString) {
    *jsonString = isJson;
  }
  return true;
}

size_t HttpClient::readUntil(const string& delim, size_t length, string* result) {
  if (!m_buffer) {
    m_buffer = reinterpret_cast<char*>(malloc(1024+1));  // 1 extra for final terminator
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
    if (static_cast<unsigned>(received) > m_bufferSize) {
      return string::npos;  // error in recv
    }
    m_buffer[received] = 0;
    size_t oldLength = result->length();
    *result += m_buffer;
    if (findDelim) {
      pos = result->find(delim, oldLength - (delim.length() - 1));
    }
  }
  return findDelim ? pos : result->length();
}

}  // namespace ebusd

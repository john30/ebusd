/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023-2025 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ebusd/request.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include "lib/utils/log.h"

namespace ebusd {

RequestImpl::RequestImpl(bool isHttp)
  : Request(), m_isHttp(isHttp), m_resultSet(false), m_disconnect(false), m_listenSince(0) {
  m_mode.listenMode = lm_none;
  m_mode.format = OF_NONE;
  m_mode.listenWithUnknown = false;
  m_mode.listenOnlyUnknown = false;
  pthread_mutex_init(&m_mutex, nullptr);
  pthread_cond_init(&m_cond, nullptr);
}

RequestImpl::~RequestImpl() {
  m_resultSet = true;
  pthread_mutex_destroy(&m_mutex);
  pthread_cond_destroy(&m_cond);
}

bool RequestImpl::add(const char* request) {
  if (request && request[0]) {
    string add = request;
    add.erase(remove(add.begin(), add.end(), '\r'), add.end());
    m_request.append(add);
  }
  size_t pos = m_request.find(m_isHttp ? "\n\n" : "\n");
  if (pos != string::npos) {
    if (m_isHttp) {
      pos = m_request.find("\n");
      m_request.resize(pos);  // reduce to first line
      // typical first line: GET /ehp/outsidetemp HTTP/1.1
      pos = m_request.rfind(" HTTP/");
      if (pos != string::npos) {
        m_request.resize(pos);  // remove "HTTP/x.x" suffix
      }
      pos = 0;
      while ((pos=m_request.find('%', pos)) != string::npos && pos+2 <= m_request.length()) {
        unsigned int value1, value2;
        if (sscanf("%1x%1x", m_request.c_str()+pos+1, &value1, &value2) < 2) {
          break;
        }
        m_request[pos] = static_cast<char>(((value1&0x0f) << 4) | (value2&0x0f));
        m_request.erase(pos+1, 2);
      }
    } else if (pos+1 == m_request.length()) {
      m_request.resize(pos);  // reduce to complete lines
    }
    return true;
  }
  return m_request.length() == 0 && m_mode.listenMode != lm_none;
}

void RequestImpl::split(vector<string>* args) {
  string token, previous;
  istringstream stream(m_request);
  char escaped = 0;

  char delim = ' ';
  while (getline(stream, token, delim)) {
    if (!m_isHttp) {
      if (escaped) {
        args->pop_back();
        if (token.length() > 0 && token[token.length()-1] == escaped) {
          token.erase(token.length() - 1, 1);
          escaped = 0;
        }
        token = previous + " " + token;
      } else if (token.length() == 0) {  // allow multiple space chars for a single delimiter
        continue;
      } else if (token[0] == '"' || token[0] == '\'') {
        escaped = token[0];
        token.erase(0, 1);
        if (token.length() > 0 && token[token.length()-1] == escaped) {
          token.erase(token.length() - 1, 1);
          escaped = 0;
        }
      }
    }
    args->push_back(token);
    previous = token;
    if (m_isHttp) {
      delim = (args->size() == 1) ? '?' : '\n';
    }
  }
}

bool RequestImpl::waitResponse(string* result) {
  pthread_mutex_lock(&m_mutex);

  if (!m_resultSet) {
    pthread_cond_wait(&m_cond, &m_mutex);
  }
  m_request.clear();
  *result = m_result;
  m_result.clear();
  m_resultSet = false;
  pthread_mutex_unlock(&m_mutex);
  return m_disconnect;
}

void RequestImpl::setResult(const string& result, const string& user, RequestMode* mode, time_t listenUntil,
      bool disconnect) {
  pthread_mutex_lock(&m_mutex);
  m_result = result;
  m_user = user;
  m_disconnect = disconnect;
  if (mode) {
    m_mode = *mode;
  }
  m_listenSince = listenUntil;
  m_resultSet = true;
  pthread_cond_signal(&m_cond);
  pthread_mutex_unlock(&m_mutex);
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2026 John Baier <ebusd@ebusd.eu>
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

#include "ebusd/messagestream.h"
#include <cstddef>

namespace ebusd {

/** the number of lines kept for a connection that falls behind. */
#define MESSAGE_STREAM_SIZE 256

void MessageStream::add(const string& line) {
  pthread_mutex_lock(&m_mutex);
  if (!m_subscribers.empty()) {
    m_lines.push_back(line);
    if (m_lines.size() > MESSAGE_STREAM_SIZE) {
      m_lines.pop_front();
    }
    m_nextSeq++;
    for (const auto notify : m_subscribers) {
      notify->notify();
    }
  }
  pthread_mutex_unlock(&m_mutex);
}

uint64_t MessageStream::subscribe(const Notify* notify) {
  pthread_mutex_lock(&m_mutex);
  m_subscribers.push_back(notify);
  uint64_t ret = m_nextSeq;
  pthread_mutex_unlock(&m_mutex);
  return ret;
}

void MessageStream::unsubscribe(const Notify* notify) {
  pthread_mutex_lock(&m_mutex);
  m_subscribers.remove(notify);
  pthread_mutex_unlock(&m_mutex);
}

uint64_t MessageStream::read(uint64_t* seq, string* output) {
  pthread_mutex_lock(&m_mutex);
  uint64_t firstSeq = m_nextSeq - m_lines.size();
  uint64_t lost = 0;
  if (*seq < firstSeq) {
    lost = firstSeq - *seq;
    *seq = firstSeq;
  }
  for (auto it = m_lines.begin() + static_cast<ptrdiff_t>(*seq - firstSeq); it != m_lines.end(); it++) {
    output->append(*it);
    output->append("\n");
  }
  *seq = m_nextSeq;
  pthread_mutex_unlock(&m_mutex);
  return lost;
}

}  // namespace ebusd

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

#ifndef EBUSD_MESSAGESTREAM_H_
#define EBUSD_MESSAGESTREAM_H_

#include <pthread.h>
#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include "lib/utils/notify.h"

namespace ebusd {

/** \file ebusd/messagestream.h
 * Stream of the messages seen on the bus for clients in direct stream mode.
 */

using std::deque;
using std::list;
using std::string;

/**
 * The most recent messages seen on the bus, waking the subscribed connections as soon as one is added.
 */
class MessageStream {
 public:
  MessageStream() : m_nextSeq(0) {
    pthread_mutex_init(&m_mutex, nullptr);
  }

  ~MessageStream() {
    pthread_mutex_destroy(&m_mutex);
  }

  /**
   * Add a line and notify all subscribers.
   * @param line the message as in direct mode: the master part in hex, followed by the slave part if any.
   */
  void add(const string& line);

  /**
   * Subscribe for being notified on each added line.
   * @param notify the @a Notify to trigger.
   * @return the sequence number of the next line to read.
   */
  uint64_t subscribe(const Notify* notify);

  /**
   * Unsubscribe from being notified.
   * @param notify the @a Notify passed to @a subscribe.
   */
  void unsubscribe(const Notify* notify);

  /**
   * Append all lines starting at the given sequence number.
   * @param seq the sequence number of the first line to append, updated to the next one to read.
   * @param output the string to append the lines to.
   * @return the number of lines no longer available since the client fell behind.
   */
  uint64_t read(uint64_t* seq, string* output);

 private:
  /** mutex variable for exclusive lock. */
  pthread_mutex_t m_mutex;

  /** the most recent lines, the last one having sequence number @a m_nextSeq - 1. */
  deque<string> m_lines;

  /** the sequence number of the next line to add. */
  uint64_t m_nextSeq;

  /** the subscribed @a Notify instances. */
  list<const Notify*> m_subscribers;
};

}  // namespace ebusd

#endif  // EBUSD_MESSAGESTREAM_H_

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_UTILS_QUEUE_H_
#define LIB_UTILS_QUEUE_H_

#include <pthread.h>
#include <errno.h>
#include <list>
#include "lib/utils/clock.h"

namespace ebusd {

/** \file lib/utils/queue.h */

using std::list;

/**
 * Thread safe template class for queuing items.
 * @param T the item type.
 */
template <typename T>
class Queue {
 public:
  /**
   * Constructor.
   */
  Queue() {
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
  }

  /**
   * Destructor.
   */
  ~Queue() {
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);
  }


 private:
  /**
   * Hidden copy constructor.
   * @param src the object to copy from.
   */
  Queue(const Queue& src);


 public:
  /**
   * Add an item to the end of queue.
   * @param item the item to add, or nullptr for notifying only.
   */
  void push(T item) {
    pthread_mutex_lock(&m_mutex);
    if (item) {
      m_queue.push_back(item);
    }
    pthread_cond_broadcast(&m_cond);
    pthread_mutex_unlock(&m_mutex);
  }

  /**
   * Remove the first item from the queue optionally waiting for the queue being non-empty.
   * @param timeout the maximum time in seconds to wait for the queue being filled, or 0 for no wait.
   * @return the item, or nullptr if no item is available within the specified time.
   */
  T pop(int timeout = 0) {
    T item;
    pthread_mutex_lock(&m_mutex);
    if (timeout > 0 && m_queue.empty()) {
      struct timespec t;
      clockGettime(&t);
      t.tv_sec += timeout;
      pthread_cond_timedwait(&m_cond, &m_mutex, &t);
    }
    if (m_queue.empty()) {
      item = nullptr;
    } else {
      item = m_queue.front();
      m_queue.pop_front();
    }
    pthread_mutex_unlock(&m_mutex);
    return item;
  }

  /**
   * Remove the specified item from the queue optionally waiting for it to appear in the queue.
   * @param item the item to remove and optionally wait for.
   * @param wait true to wait for the item to appear in the queue.
   * @return whether the item was removed.
   */
  bool remove(T item, bool wait = false) {
    bool result = false;
    pthread_mutex_lock(&m_mutex);
    struct timespec t;
    while (true) {
      clockGettime(&t);
      t.tv_sec++;  // check thread death every second
      size_t oldSize = m_queue.size();
      if (oldSize > 0) {
        m_queue.remove(item);
        if (m_queue.size() != oldSize) {
          result = true;
          break;
        }
      }
      if (!wait) {
        break;
      }
      int ret = pthread_cond_timedwait(&m_cond, &m_mutex, &t);
      if (ret != 0 && ret != ETIMEDOUT) {
        break;
      }
    }
    pthread_mutex_unlock(&m_mutex);
    return result;
  }

  /**
   * Return the first item in the queue without removing it.
   * @return the item, or nullptr if no item is available.
   */
  T peek() {
    T item;
    pthread_mutex_lock(&m_mutex);
    if (m_queue.empty()) {
      item = nullptr;
    } else {
      item = m_queue.front();
    }
    pthread_mutex_unlock(&m_mutex);
    return item;
  }


 private:
  /** the queue itself */
  list<T> m_queue;

  /** mutex variable for exclusive lock */
  pthread_mutex_t m_mutex;

  /** condition variable for exclusive lock */
  pthread_cond_t m_cond;
};

}  // namespace ebusd

#endif  // LIB_UTILS_QUEUE_H_

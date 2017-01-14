/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#ifndef LIB_UTILS_THREAD_H_
#define LIB_UTILS_THREAD_H_

#include <pthread.h>

/** \file thread.h */

/**
 * wrapper class for pthread.
 */
class Thread {
 public:
  /**
   * constructor.
   */
  Thread() : m_threadid(0), m_started(false), m_running(false), m_stopped(false) {}

  /**
   * virtual destructor.
   */
  virtual ~Thread();

  /**
   * Thread entry helper for pthread_create.
   * @param arg pointer to the @a Thread.
   * @return NULL.
   */
  static void* runThread(void* arg);

  /**
   * Return whether this @a Thread is still running and not yet stopped.
   * @return true if this @a Thread is till running and not yet stopped.
   */
  virtual bool isRunning() { return m_running && !m_stopped; }

  /**
   * Create the native thread and set its name.
   * @param name the thread name to show in the process list.
   * @return whether the thread was started.
   */
  virtual bool start(const char* name);

  /**
   * Notify the thread that it shall stop.
   */
  virtual void stop() { m_stopped = true; }

  /**
   * Join the thread.
   * @return whether the thread was joined.
   */
  virtual bool join();

  /**
   * Get the thread id.
   * @return the thread id.
   */
  pthread_t self() { return m_threadid; }


 protected:
  /**
   * Thread entry method to be overridden by derived class.
   */
  virtual void run() = 0;


 private:
  /**
   * Enter the Thread loop by calling run().
   */
  void enter();

  /** own thread id */
  pthread_t m_threadid;

  /** Whether the thread was started. */
  bool m_started;

  /** Whether the thread is still running (i.e. in @a run() ). */
  bool m_running;

  /** Whether the thread was stopped by @a stop() or @a join(). */
  bool m_stopped;
};


/**
 * A @a Thread that can be waited on.
 */
class WaitThread : public Thread {
 public:
  /**
   * Constructor.
   */
  WaitThread();

  /**
   * Destructor.
   */
  virtual ~WaitThread();

  // @copydoc
  virtual void stop();

  // @copydoc
  virtual bool join();

  /**
   * Wait for the specified amount of time.
   * @param seconds the number of seconds to wait.
   * @return true if this @a WaitThread is still running and not yet stopped.
   */
  bool Wait(int seconds);


 private:
  /** the mutex for waiting. */
  pthread_mutex_t m_mutex;

  /** the condition for waiting. */
  pthread_cond_t m_cond;
};

#endif  // LIB_UTILS_THREAD_H_

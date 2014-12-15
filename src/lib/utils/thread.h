/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBUTILS_THREAD_H_
#define LIBUTILS_THREAD_H_

#include <pthread.h>

/** \file thread.h */

/**
 * @brief wrapper class for pthread.
 */
class Thread
{

public:
	/**
	 * @brief constructor.
	 */
	Thread() : m_threadid(0), m_started(false), m_running(false), m_stopped(false), m_detached(false) {}

	/**
	 * @brief virtual destructor.
	 */
	virtual ~Thread();

	/**
	 * @brief Thread entry helper for pthread_create.
	 * @param arg pointer to the @a Thread.
	 * @return NULL.
	 */
	static void* runThread(void* arg);

	/**
	 * @brief Return whether this @a Thread is still running and not yet stopped.
	 * @return true if this @a Thread is till running and not yet stopped.
	 */
	virtual bool isRunning() { return m_running == true && m_stopped == false; }

	/**
	 * @brief Create the native thread and set its name.
	 * @param name the thread name to show in the process list.
	 * @return whether the thread was started.
	 */
	virtual bool start(const char* name);

	/**
	 * @brief Notify the thread that it shall stop.
	 */
	virtual void stop() { m_stopped = true; }

	/**
	 * @brief Join the thread.
	 * @return whether the thread was joined.
	 */
	virtual bool join();

	/**
	 * @brief Detach the thread.
	 * @return whether the thread was detached.
	 */
	virtual bool detach();

	/**
	 * @brief Get the thread id.
	 * @return the thread id.
	 */
	pthread_t self() {return m_threadid; }

	/**
	 * @brief Thread entry method to be overridden by derived class.
	 */
	virtual void run() = 0;

private:

	/**
	 * @brief Enter the Thread loop by calling run().
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

	/** Whether the thread was detached */
	bool m_detached;

};

#endif // LIBUTILS_THREAD_H_

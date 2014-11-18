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

/**
 * @brief wrapper class for pthread.
 */
class Thread
{

public:
	/**
	 * @brief constructor.
	 */
	Thread() : m_threadid(0), m_running(false), m_detached(false) {}

	/**
	 * @brief virtual destructor.
	 */
	virtual ~Thread();

	/**
	 * @brief create the thread and set name for process list.
	 * @param name the thread name which show in process list.
	 * @return value of thread creating.
	 */
	int start(const char* name);

	/**
	 * @brief join the thread.
	 * @return value of thread joining.
	 */
	int join();

	/**
	 * @brief detach the thread.
	 * @return value of thread detaching.
	 */
	int detach();

	/**
	 * @brief return the thread id.
	 * @return own thread id.
	 */
	pthread_t self() {return m_threadid; }

	/**
	 * @brief virtul function which must be implemented in derived class.
	 * @return void pointer.
	 */
	virtual void* run() = 0;

private:
	/** own thread id */
	pthread_t m_threadid;

	/** true if thread is running */
	bool m_running;

	/** true if thread is detached */
	bool m_detached;

};

#endif // LIBUTILS_THREAD_H_

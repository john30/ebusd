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

class Thread
{

public:
	Thread() : m_threadid(0), m_running(false), m_detached(false) {}
	virtual ~Thread();

	int start(const char* name);
	int join();
	int detach();
	pthread_t self() {return m_threadid; }

	virtual void* run() = 0;

private:
	pthread_t m_threadid;
	bool m_running;
	bool m_detached;

};

#endif // LIBUTILS_THREAD_H_

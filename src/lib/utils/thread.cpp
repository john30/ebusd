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

#include "config.h"
#include "thread.h"

static void* runThread(void* arg)
{
	return ((Thread *)arg)->run();
}

Thread::~Thread()
{
	if (m_running == true && m_detached == false)
		pthread_detach(m_threadid);

	if (m_running == true)
		pthread_cancel(m_threadid);
}

int Thread::start(const char* name)
{

	int result = pthread_create(&m_threadid, NULL, runThread, this);

	if (result == 0) {

#ifdef HAVE_PTHREAD_SETNAME_NP
		pthread_setname_np(m_threadid, name);
#endif

		m_running = true;
	}

	return result;
}

int Thread::join()
{
	int result = -1;

	if (m_running == true) {
		result = pthread_join(m_threadid, NULL);

		if (result == 0)
			m_detached = false;

	}

	return result;
}

int Thread::detach()
{
	int result = -1;

	if (m_running == true && m_detached == false) {
		result = pthread_detach(m_threadid);

		if (result == 0)
			m_detached = true;

	}

	return result;
}


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

#ifndef LIBUTILS_WQUEUE_H_
#define LIBUTILS_WQUEUE_H_

#include <list>
#include <pthread.h>

template <typename T> class WQueue
{

public:
	WQueue()
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	~WQueue()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	void add(T item)
	{
		pthread_mutex_lock(&m_mutex);

		m_queue.push_back(item);

		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	T remove()
	{
		pthread_mutex_lock(&m_mutex);

		while (m_queue.size() == 0)
			pthread_cond_wait(&m_cond, &m_mutex);

		T item = m_queue.front();
		m_queue.pop_front();

		pthread_mutex_unlock(&m_mutex);

		return item;
	}

	T next()
	{
		pthread_mutex_lock(&m_mutex);

		while (m_queue.size() == 0)
			pthread_cond_wait(&m_cond, &m_mutex);

		T item = m_queue.front();

		pthread_mutex_unlock(&m_mutex);

		return item;
	}

	int size()
	{
		pthread_mutex_lock(&m_mutex);

		int size = m_queue.size();

		pthread_mutex_unlock(&m_mutex);

		return size;
	}

private:
	std::list<T> m_queue;
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;

};

#endif // LIBUTILS_WQUEUE_H_

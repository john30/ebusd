/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
 */

#ifndef WQUEUE_H_
#define WQUEUE_H_

#include <pthread.h>
#include <list>

template <typename T> class WQueue
{

public:
	WQueue()
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_condv, NULL);
	}
	
	~WQueue()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_condv);
	}
	
	void add(T item)
	{
		pthread_mutex_lock(&m_mutex);
		
		m_queue.push_back(item);
		
		pthread_cond_signal(&m_condv);
		pthread_mutex_unlock(&m_mutex);
	}
	
	T remove()
	{
		pthread_mutex_lock(&m_mutex);
		
		while (m_queue.size() == 0)
			pthread_cond_wait(&m_condv, &m_mutex);

		T item = m_queue.front();
		m_queue.pop_front();
		
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
	pthread_cond_t m_condv;
	
};

#endif // WQUEUE_H_

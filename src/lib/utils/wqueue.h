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

using namespace std;

/**
 * @brief queue class template for all kinds data types with exclusiv lock.
 */
template <typename T>
class WQueue
{

public:
	/**
	 * @brief constructs a new instance.
	 */
	WQueue()
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	/**
	 * @brief destructor.
	 */
	~WQueue()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	/**
	 * @brief add a new item to the end of queue.
	 * @param item to add.
	 */
	void add(T item)
	{
		pthread_mutex_lock(&m_mutex);

		m_queue.push_back(item);

		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	/**
	 * @brief remove the first item from queue.
	 * @return the item.
	 */
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

	/**
	 * @brief return the first item from queue without remove.
	 * @return the item.
	 */
	T next()
	{
		pthread_mutex_lock(&m_mutex);

		while (m_queue.size() == 0)
			pthread_cond_wait(&m_cond, &m_mutex);

		T item = m_queue.front();

		pthread_mutex_unlock(&m_mutex);

		return item;
	}

	/**
	 * @brief the number of entries inside queue.
	 * @return the size.
	 */
	int size()
	{
		pthread_mutex_lock(&m_mutex);

		int size = m_queue.size();

		pthread_mutex_unlock(&m_mutex);

		return size;
	}

private:
	/** the queue itself */
	list<T> m_queue;

	/** mutex variable for exclusive lock */
	pthread_mutex_t m_mutex;

	/** condition variable for exclusive lock */
	pthread_cond_t m_cond;

};

#endif // LIBUTILS_WQUEUE_H_

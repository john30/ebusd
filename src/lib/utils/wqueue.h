/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>,
 * John Baier 2014-2015 <ebusd@johnm.de>
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

/** \file wqueue.h */

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

private:
	/**
	 * @brief Hidden copy constructor.
	 * @param src the object to copy from.
	 */
	WQueue(const WQueue& src);

public:

	/**
	 * @brief add a new item to the end of queue.
	 * @param item to add.
	 */
	void add(T item)
	{
		pthread_mutex_lock(&m_mutex);

		m_queue.push_back(item);

		pthread_cond_broadcast(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	/**
	 * @brief remove the first item from queue.
	 * @param wait true to wait for an item to be added to the queue, false to return NULL if no item is available.
	 * @return the item, or NULL if no item is available and wait was false.
	 */
	T remove(bool wait=true)
	{
		pthread_mutex_lock(&m_mutex);

		T item;
		if (wait) {
			while (m_queue.size() == 0)
				pthread_cond_wait(&m_cond, &m_mutex);
			item = m_queue.front();
			m_queue.pop_front();
		}
		else if (m_queue.size() > 0) {
			item = m_queue.front();
			m_queue.pop_front();
		}
		else
			item = NULL;

		pthread_mutex_unlock(&m_mutex);

		return item;
	}

	/**
	 * @brief Remove the specified item from the queue.
	 * @param item the item to remove.
	 * @return whether the item was removed.
	 */
	bool remove(T item)
	{
		pthread_mutex_lock(&m_mutex);

		size_t oldSize = m_queue.size();
		if (oldSize > 0)
			m_queue.remove(item);
		size_t newSize = m_queue.size();

		pthread_mutex_unlock(&m_mutex);
		return newSize != oldSize;
	}

	/**
	 * @brief Wait for the specified item to appear in the queue and remove it from the queue.
	 * @param item the item to wait for and remove.
	 * @return whether the item was removed.
	 */
	bool waitRemove(T item)
	{
		pthread_mutex_lock(&m_mutex);

		do {
			size_t oldSize = m_queue.size();
			if (oldSize > 0) {
				m_queue.remove(item);
				if (m_queue.size() != oldSize)
					break;
			}

			pthread_cond_wait(&m_cond, &m_mutex);
		} while (true);

		pthread_mutex_unlock(&m_mutex);
		return true;
	}

	/**
	 * @brief return the first item from the queue without remove.
	 * @param wait whether to wait for an entry if the queue is empty.
	 * @return the item, or NULL if no item is available and wait was false.
	 */
	T next(bool wait=true)
	{
		pthread_mutex_lock(&m_mutex);

		T item;
		if (wait) {
			while (m_queue.size() == 0)
				pthread_cond_wait(&m_cond, &m_mutex);
			item = m_queue.front();
		}
		else if (m_queue.size() > 0)
			item = m_queue.front();
		else
			item = NULL;

		pthread_mutex_unlock(&m_mutex);

		return item;
	}

	/**
	 * @brief the number of entries inside queue.
	 * @return the size.
	 */
	size_t size()
	{
		pthread_mutex_lock(&m_mutex);

		size_t size = m_queue.size();

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

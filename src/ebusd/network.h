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

#ifndef NETWORK_H_
#define NETWORK_H_

#include "tcpsocket.h"
#include "wqueue.h"
#include "notify.h"
#include "thread.h"
#include <string>

/** forward declaration for class connection */
class Connection;

/**
 * @brief class for data/message transfer between connection and baseloop.
 */
class NetMessage
{

public:
	/**
	 * @brief constructs a new instance with message and source client address.
	 * @param data from client.
	 */
	NetMessage(const std::string data) : m_data(data)
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	/**
	 * @brief destructor.
	 */
	~NetMessage()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	/**
	 * @brief copy constructor.
	 * @param src message object for copy.
	 */
	NetMessage(const NetMessage& src) : m_data(src.m_data) {}

	/**
	 * @brief get the data string.
	 * @return the data string.
	 */
	std::string getData() const { return m_data; }

	/**
	 * @brief get the result string.
	 * @return the result string.
	 */
	std::string getResult() const { return m_result; }

	/**
	 * @brief set the result string.
	 * @return the result string.
	 */
	void setResult(const std::string result) { m_result = result; }

	/**
	 * @brief wait on notification.
	 */
	void waitSignal()
	{
		pthread_mutex_lock(&m_mutex);

		while (m_result.size() == 0)
			pthread_cond_wait(&m_cond, &m_mutex);

		pthread_mutex_unlock(&m_mutex);
	}

	/**
	 * @brief send notification.
	 */
	void sendSignal()
	{
		pthread_mutex_lock(&m_mutex);
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

private:
	/** the data string */
	std::string m_data;

	/** the result string */
	std::string m_result;

	/** mutex variable for exclusive lock */
	pthread_mutex_t m_mutex;

	/** condition variable for exclusive lock */
	pthread_cond_t m_cond;

};

/**
 * @brief class connection which handle client and baseloop communication.
 */
class Connection : public Thread
{

public:
	/**
	 * @brief create a new connection instance.
	 * @param socket the tcp socket for communication.
	 * @param netQueue the remote queue for network messages.
	 */
	Connection(TCPSocket* socket, WQueue<NetMessage*>* netQueue)
		: m_socket(socket), m_netQueue(netQueue), m_running(false)
		{ m_id = ++m_ids; }

	/**
	 * @brief endless loop for connection instance.
	 * @return void pointer.
	 */
	void* run();

	/**
	 * @brief closs active connection.
	 */
	void stop() const { m_notify.notify(); }

	/**
	 * @brief status of connection instance.
	 * @return true if connection is running.
	 */
	bool isRunning() const { return m_running; }

	/**
	 * @brief return own connection id.
	 * @return id of current connection.
	 */
	int getID() { return m_id; }

private:
	/** the tcp socket instance */
	TCPSocket* m_socket;

	/** remote queue for network messages */
	WQueue<NetMessage*>* m_netQueue;

	/** notification object for shutdown procedure */
	Notify m_notify;

	/** true if this instance is running */
	bool m_running;

	/** id of current connection*/
	int m_id;

	/** sumary for opened connections */
	static int m_ids;

};

/**
 * @brief class network which listening on tcp socket for incoming connections.
 */
class Network : public Thread
{

public:
	/**
	 * @brief create a new network instance and listening for incoming connections.
	 * @param local true to accept connections only for local host.
	 * @param netQueue the remote queue for network messages.
	 */
	Network(const bool local, WQueue<NetMessage*>* netQueue);

	/**
	 * @brief destructor.
	 */
	~Network();

	/**
	 * @brief endless loop for network instance.
	 * @return void pointer.
	 */
	void* run();

	/**
	 * @brief shutdown network subsystem.
	 */
	void stop() const { m_notify.notify(); usleep(100000); }

private:
	/** container for active connections */
	std::list<Connection*> m_connections;

	/** remote queue for network messages */
	WQueue<NetMessage*>* m_netQueue;

	/** the tcp server instance */
	TCPServer* m_tcpServer;

	/** notification object for shutdown procedure */
	Notify m_notify;

	/** true if this instance is listening */
	bool m_listening;

	/** true if this instance is running */
	bool m_running;

	/**
	 * @brief clean inactive connections from container.
	 */
	void cleanConnections();

};

#endif // NETWORK_H_


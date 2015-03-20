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

#ifndef NETWORK_H_
#define NETWORK_H_

#include "tcpsocket.h"
#include "wqueue.h"
#include "notify.h"
#include "thread.h"
#include <string>

/** \file network.h */

using namespace std;

/** forward declaration for class connection */
class Connection;

/**
 * class for data/message transfer between connection and baseloop.
 */
class NetMessage
{

public:
	/**
	 * constructs a new instance with data received from the client.
	 * @param data from client.
	 * @param listening whether the client is in listening mode.
	 * @param listenSince start timestamp of listening update.
	 */
	NetMessage(const string data, const bool listening, const time_t listenSince)
		: m_data(data), m_resultSet(false), m_disconnect(false), m_listening(listening), m_listenSince(listenSince)
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	/**
	 * destructor.
	 */
	~NetMessage()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

private:
	/**
	 * Hidden copy constructor.
	 * @param src the object to copy from.
	 */
	NetMessage(const NetMessage& src);

public:

	/**
	 * get the data string.
	 * @return the data string.
	 */
	string getData() const { return m_data; }

	/**
	 * Wait for the result being set and return the result string.
	 * @return the result string.
	 */
	string getResult()
	{
		pthread_mutex_lock(&m_mutex);

		while (!m_resultSet)
			pthread_cond_wait(&m_cond, &m_mutex);

		pthread_mutex_unlock(&m_mutex);

		return m_result;
	}

	/**
	 * Set the result string and notify the waiting thread.
	 * @param result the result string.
	 * @param listening whether the client is in listening mode.
	 * @param listenUntil the end time to which to updates were added (exclusive).
	 * @param disconnect true when the client shall be disconnected.
	 */
	void setResult(const string result, const bool listening, const time_t listenUntil, const bool disconnect)
	{
		m_result = result;
		m_disconnect = disconnect;
		m_listening = listening;
		m_listenSince = listenUntil;
		m_resultSet = true;
		pthread_mutex_lock(&m_mutex);
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	/**
	 * Return whether the client is in listening mode.
	 * @param listenSince set to the start time from which to add updates (inclusive).
	 * @return whether the client is in listening mode.
	 */
	bool isListening(time_t& listenSince) { listenSince = m_listenSince; return m_listening; }

	/**
	 * Return whether the client shall be disconnected.
	 * @return true when the client shall be disconnected.
	 */
	bool isDisconnect() { return m_disconnect; }

private:
	/** the data string */
	string m_data;

	/** whether the result was already set. */
	bool m_resultSet;

	/** the result string */
	string m_result;

	/** set to true when the client shall be disconnected. */
	bool m_disconnect;

	/** mutex variable for exclusive lock */
	pthread_mutex_t m_mutex;

	/** condition variable for exclusive lock */
	pthread_cond_t m_cond;

	/** whether the client is in listening mode */
	bool m_listening;

	/** start timestamp of listening update */
	time_t m_listenSince;

};

/**
 * class connection which handle client and baseloop communication.
 */
class Connection : public Thread
{

public:
	/**
	 * create a new connection instance.
	 * @param socket the tcp socket for communication.
	 * @param netQueue the remote queue for network messages.
	 */
	Connection(TCPSocket* socket, WQueue<NetMessage*>* netQueue)
		: m_socket(socket), m_netQueue(netQueue), m_listening(false)
		{ m_id = ++m_ids; }

	virtual ~Connection() { if (m_socket) delete m_socket; }
	/**
	 * endless loop for connection instance.
	 */
	virtual void run();

	/**
	 * close active connection.
	 */
	virtual void stop() { m_notify.notify(); Thread::stop(); }

	/**
	 * return own connection id.
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

	/** id of current connection */
	int m_id;

	/** sumary for opened connections */
	static int m_ids;

	/** whether the client is in listening mode */
	bool m_listening;

};

/**
 * class network which listening on tcp socket for incoming connections.
 */
class Network : public Thread
{

public:
	/**
	 * create a network instance and listening for incoming connections.
	 * @param local true to accept connections only for local host.
	 * @param port the tcp port to listening.
	 * @param netQueue the remote queue for network messages.
	 */
	Network(const bool local, const uint16_t port, WQueue<NetMessage*>* netQueue);

	/**
	 * destructor.
	 */
	~Network();

	/**
	 * endless loop for network instance.
	 */
	virtual void run();

	/**
	 * shutdown network subsystem.
	 */
	void stop() const { m_notify.notify(); usleep(100000); }

private:
	/** the list of active @a Connection instances. */
	list<Connection*> m_connections;

	/** the @a MainLoop queue for transferring @a NetMessage instances. */
	WQueue<NetMessage*>* m_netQueue;

	/** the @a TCPServer instance. */
	TCPServer* m_tcpServer;

	/** @a Notify object for shutdown procedure. */
	Notify m_notify;

	/** true if this instance is listening */
	bool m_listening;

	/**
	 * clean inactive connections from container.
	 */
	void cleanConnections();

};

#endif // NETWORK_H_


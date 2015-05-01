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
#include <cstdio>
#include <algorithm>

/** \file network.h */

using namespace std;

/** Forward declaration for @a Connection. */
class Connection;

/**
 * Class for data/message transfer between @a Connection and @a MainLoop.
 */
class NetMessage
{

public:
	/**
	 * Constructor.
	 * @param isHttp whether this is a HTTP message.
	 */
	NetMessage(const bool isHttp)
		: m_isHttp(isHttp), m_resultSet(false), m_disconnect(false), m_listening(false), m_listenSince(0)
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	/**
	 * Destructor.
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
	 * Add request data received from the client.
	 * @param request the request data from the client.
	 * @param listening whether the client is in listening mode.
	 * @param listenSince start timestamp of listening update.
	 * @return true when the request is complete and the response shall be prepared.
	 */
	bool add(string request)
	{
		if (request.length()>0) {
			request.erase(remove(request.begin(), request.end(), '\r'), request.end());
			m_request.append(request);
		}
		size_t pos = m_request.find(m_isHttp ? "\n\n" : "\n");
		if (pos!=string::npos) {
			if (m_isHttp) {
				pos = m_request.find("\n");
				m_request.resize(pos); // reduce to first line
				// typical first line: GET /ehp/outsidetemp HTTP/1.1
				pos = m_request.rfind(" HTTP/");
				if (pos!=string::npos) {
					m_request.resize(pos); // remove "HTTP/x.x" suffix
				}
				pos = 0;
				while ((pos=m_request.find('%', pos))!=string::npos && pos+2<=m_request.length()) {
					unsigned int value1, value2;
					if (sscanf("%1x%1x", m_request.c_str()+pos+1, &value1, &value2)<2)
						break;
					m_request[pos] = (char)(((value1&0x0f)<<4)|(value2&0x0f));
					m_request.erase(pos+1, 2);
				}
			} else if (pos+1==m_request.length()) {
				m_request.resize(pos); // reduce to complete lines
			}
			return true;
		}
		return m_request.length()==0 && m_listening;
	}

	/**
	 * Return whether this is a HTTP message.
	 * @return whether this is a HTTP message.
	 */
	bool isHttp() const { return m_isHttp; }

	/**
	 * Return the request string.
	 * @return the request string.
	 */
	string getRequest() const { return m_request; }

	/**
	 * Wait for the result being set and return the result string.
	 * @return the result string.
	 */
	string getResult()
	{
		pthread_mutex_lock(&m_mutex);

		while (!m_resultSet)
			pthread_cond_wait(&m_cond, &m_mutex);

		m_request.clear();
		string result = m_result;
		m_result.clear();
		m_resultSet = false;
		pthread_mutex_unlock(&m_mutex);

		return result;
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
		pthread_mutex_lock(&m_mutex);
		m_result = result;
		m_disconnect = disconnect;
		m_listening = listening;
		m_listenSince = listenUntil;
		m_resultSet = true;
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	/**
	 * Return whether the client is in listening mode.
	 * @param listenSince set to the start time from which to add updates (inclusive).
	 * @return whether the client is in listening mode.
	 */
	bool isListening(time_t* listenSince=NULL) { if (listenSince) *listenSince = m_listenSince; return m_listening; }

	/**
	 * Return whether the client shall be disconnected.
	 * @return true when the client shall be disconnected.
	 */
	bool isDisconnect() { return m_disconnect; }

private:
	/** whether this is a HTTP message. */
	const bool m_isHttp;

	/** the request string. */
	string m_request;

	/** whether the result was already set. */
	bool m_resultSet;

	/** the result string. */
	string m_result;

	/** set to true when the client shall be disconnected. */
	bool m_disconnect;

	/** mutex variable for exclusive lock. */
	pthread_mutex_t m_mutex;

	/** condition variable for exclusive lock. */
	pthread_cond_t m_cond;

	/** whether the client is in listening mode. */
	bool m_listening;

	/** start timestamp of listening update. */
	time_t m_listenSince;

};

/**
 * class connection which handle client and baseloop communication.
 */
class Connection : public Thread
{

public:
	/**
	 * Constructor.
	 * @param socket the @a TCPSocket for communication.
	 * @param isHttp whether this is a HTTP message.
	 * @param netQueue the remote @a WQueue for handling @a NetMessage instances.
	 */
	Connection(TCPSocket* socket, const bool isHttp, WQueue<NetMessage*>* netQueue)
		: m_isHttp(isHttp), m_socket(socket), m_netQueue(netQueue)
		{ m_id = ++m_ids; }

	virtual ~Connection() { if (m_socket) delete m_socket; }
	/**
	 * endless loop for connection instance.
	 */
	virtual void run();

	/**
	 * Stop this connection.
	 */
	virtual void stop() { m_notify.notify(); Thread::stop(); }

	/**
	 * Return the ID of this connection.
	 * @return the ID of this connection.
	 */
	int getID() { return m_id; }

private:
	/** whether this is a HTTP connection. */
	const bool m_isHttp;

	/** the @a TCPSocket for communication. */
	TCPSocket* m_socket;

	/** the remote @a WQueue for handling @a NetMessage instances. */
	WQueue<NetMessage*>* m_netQueue;

	/** notification object for shutdown procedure. */
	Notify m_notify;

	/** the ID of this connection. */
	int m_id;

	/** the IF of the last opened connection. */
	static int m_ids;

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
	 * @param port the port to listen for command line connections.
	 * @param httpPort the port to listen for HTTP connections, or 0.
	 * @param netQueue the remote queue for network messages.
	 */
	Network(const bool local, const uint16_t port, const uint16_t httpPort, WQueue<NetMessage*>* netQueue);

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

	/** the command line @a TCPServer instance. */
	TCPServer* m_tcpServer;

	/** the HTTP @a TCPServer instance, or NULL. */
	TCPServer* m_httpServer;

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


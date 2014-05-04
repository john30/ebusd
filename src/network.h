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

#ifndef NETWORK_H_
#define NETWORK_H_

#include "tcpsocket.h"
#include "wqueue.h"
#include "thread.h"
#include "notify.h"
#include "baseloop.h"
#include <list>
#include <unistd.h>

class Connection : public Thread
{

public:
	Connection(TCPSocket* socket, WQueue<Message*>* data)
		: m_socket(socket), m_data(data), m_running(false) { m_count++; }

	~Connection() { m_count--; }

	void addResult(Message message);
 
	void* run();
	void stop() const { m_notify.notify(); }
	bool isRunning() const { return m_running; }

	pthread_t getID() { return this->self(); }
	int numConnections() const { return m_count; }

private:
	TCPSocket* m_socket;
	WQueue<Message*>* m_data;
	WQueue<Message*> m_result;
	Notify m_notify;
	bool m_running;

	static int m_count;

};

class Network : public Thread
{

public:
	Network(const bool localhost);
	~Network();

	void addQueue(WQueue<Message*>* queue) { m_queue = queue; }
	
	void* run();
	void stop() const { m_notify.notify(); usleep(100000); }

private:
	std::list<Connection*> m_connections;
	WQueue<Message*>* m_queue;
	TCPListener* m_Listener;
	Notify m_notify;
	bool m_listening;
	bool m_running;
	
	void cleanConnections();

};

#endif // NETWORK_H_

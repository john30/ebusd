/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
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

#ifndef NETWORK_HPP__
#define NETWORK_HPP__

#include "tcpsocket.hpp"
#include "wqueue.hpp"
#include "thread.hpp"
#include "notify.hpp"
#include "baseloop.hpp"
#include <unistd.h>
#include <list>

class Connection : public Thread
{

public:
	Connection(TCPSocket* socket, WQueue<Message*>* data)
		: m_socket(socket), m_data(data), m_running(false) {}

	void addResult(Message message);
 
	void* run();
	void stop() const { m_notify.notify(); }
	bool isRunning() const { return m_running; }

private:
	TCPSocket* m_socket;
	WQueue<Message*>* m_data;
	WQueue<Message*> m_result;
	Notify m_notify;
	bool m_running;

};

class Network : public Thread
{

public:
	Network(int port, std::string ip);
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

#endif // NETWORK_HPP__

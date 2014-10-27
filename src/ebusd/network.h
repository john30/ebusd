/*
 * Copyright (C) Roland Jax 2012-2014 <roland.jax@liwest.at>
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

#include "connection.h"

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
	TCPServer* m_Server;
	Notify m_notify;
	bool m_listening;
	bool m_running;

	void cleanConnections();

};

#endif // NETWORK_H_

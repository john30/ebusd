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

#include "network.h"
#include "logger.h"
#include "appl.h"

extern LogInstance& L;
extern Appl& A;


Network::Network(const bool localhost) : m_listening(false), m_running(false)
{
	if (localhost == true)
		m_Server = new TCPServer(A.getParam<int>("p_port"), "127.0.0.1");
	else
		m_Server = new TCPServer(A.getParam<int>("p_port"), "0.0.0.0");

	if (m_Server != NULL && m_Server->start() == 0)
		m_listening = true;

}

Network::~Network()
{
	while (m_connections.empty() == false) {
		Connection* connection = m_connections.back();
		m_connections.pop_back();
		connection->stop();
		connection->join();
		delete connection;
	}

	if (m_running == true)
		stop();

	delete m_Server;
}

void* Network::run()
{
	if (m_listening == false)
		return NULL;

	m_running = true;

	int maxfd;
	fd_set checkfds;
	struct timeval timeout;

	FD_ZERO(&checkfds);
	FD_SET(m_notify.notifyFD(), &checkfds);
	FD_SET(m_Server->getFD(), &checkfds);

	(m_notify.notifyFD() > m_Server->getFD()) ?
		(maxfd = m_notify.notifyFD()) : (maxfd = m_Server->getFD());

	for (;;) {
		fd_set readfds;
		int ret;

		// set select timeout 1 secs
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		// set readfds to inital checkfds
		readfds = checkfds;

		ret = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
		if (ret == 0) {
			cleanConnections();
			continue;
		}

		// new data from notify
		if (FD_ISSET(m_notify.notifyFD(), &readfds)) {
			m_running = false;
			break;
		}

		// new data from socket
		if (FD_ISSET(m_Server->getFD(), &readfds)) {
			TCPSocket* socket = m_Server->newSocket();
			if (socket == NULL)
				continue;

			Connection* connection = new Connection(socket, m_queue);
			if (connection == NULL)
				continue;

			connection->start("connection");
			m_connections.push_back(connection);
			L.log(net, trace, "[%08x] connection opened %s", connection->getID(), socket->getIP().c_str());
		}

	}

	return NULL;
}

void Network::cleanConnections()
{
	std::list<Connection*>::iterator c_it;
	for (c_it = m_connections.begin(); c_it != m_connections.end(); c_it++) {
		if ((*c_it)->isRunning() == false) {
			Connection* connection = *c_it;
			c_it = m_connections.erase(c_it);
			delete connection;
			L.log(net, debug, "dead connection removed - %d", m_connections.size());
		}
	}
}


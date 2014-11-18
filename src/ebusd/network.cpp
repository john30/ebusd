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
#include <poll.h>

extern Logger& L;
extern Appl& A;


Network::Network(const bool localhost, WQueue<NetMessage*>* msgQueue)
	: m_msgQueue(msgQueue), m_listening(false), m_running(false)
{
	if (localhost == true)
		m_tcpServer = new TCPServer(A.getOptVal<int>("port"), "127.0.0.1");
	else
		m_tcpServer = new TCPServer(A.getOptVal<int>("port"), "0.0.0.0");

	if (m_tcpServer != NULL && m_tcpServer->start() == 0)
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

	delete m_tcpServer;
}

void* Network::run()
{
	if (m_listening == false)
		return NULL;

	m_running = true;

	int ret, nfds = 2;
	struct pollfd fds[nfds];
	struct timespec tdiff;

	// set select timeout 1 secs
	tdiff.tv_sec = 1;
	tdiff.tv_nsec = 0;

	memset(fds, 0, sizeof(fds));

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN;

	fds[1].fd = m_tcpServer->getFD();
	fds[1].events = POLLIN;

	for (;;) {
		// wait for new fd event
		ret = ppoll(fds, nfds, &tdiff, NULL);

		if (ret == 0) {
			cleanConnections();
			continue;
		}

		// new data from notify
		if (fds[0].revents & POLLIN) {
			m_running = false;
			break;
		}

		// new data from socket
		if (fds[1].revents & POLLIN) {
			TCPSocket* socket = m_tcpServer->newSocket();
			if (socket == NULL)
				continue;

			Connection* connection = new Connection(socket, m_msgQueue);
			if (connection == NULL)
				continue;

			connection->start("connection");
			m_connections.push_back(connection);
			L.log(net, trace, "[%05d] connection opened %s", connection->getID(), socket->getIP().c_str());
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


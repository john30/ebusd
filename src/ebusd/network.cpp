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
#include <cstring>
#include <poll.h>

extern Logger& L;
extern Appl& A;

int Connection::m_ids = 0;

void* Connection::run()
{
	m_running = true;

	int ret, nfds = 2;
	struct pollfd fds[nfds];
	struct timespec tdiff;

	// set select timeout 10 secs
	tdiff.tv_sec = 10;
	tdiff.tv_nsec = 0;

	memset(fds, 0, sizeof(fds));

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN;

	fds[1].fd = m_socket->getFD();
	fds[1].events = POLLIN;

	for (;;) {
		// wait for new fd event
		ret = ppoll(fds, nfds, &tdiff, NULL);

		if (ret == 0) {
			continue;
		}

		// new data from notify
		if (fds[0].revents & POLLIN)
			break;

		// new data from socket
		if (fds[1].revents & POLLIN) {
			char data[256];
			size_t datalen;

			if (m_socket->isValid() == true)
				datalen = m_socket->recv(data, sizeof(data)-1);
			else
				break;

			// removed closed socket
			if (datalen <= 0 || strcasecmp(data, "QUIT") == 0)
				break;

			// send data
			data[datalen] = '\0';
			NetMessage message(data);
			m_netQueue->add(&message);

			// wait for result
			L.log(net, debug, "[%05d] wait for result", getID());
			message.waitSignal();

			L.log(net, debug, "[%05d] result added", getID());
			std::string result = message.getResult();

			if (m_socket->isValid() == true)
				m_socket->send(result.c_str(), result.size());
			else
				break;

		}

	}

	delete m_socket;
	m_running = false;
	L.log(net, trace, "[%05d] connection closed", getID());

	return NULL;
}


Network::Network(const bool local, WQueue<NetMessage*>* netQueue)
	: m_netQueue(netQueue), m_listening(false), m_running(false)
{
	if (local == true)
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

			Connection* connection = new Connection(socket, m_netQueue);
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


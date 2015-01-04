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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "network.h"
#include "logger.h"
#include "appl.h"
#include <cstring>

#ifdef HAVE_PPOLL
#include <poll.h>
#endif

using namespace std;

extern Logger& L;

int Connection::m_ids = 0;

void Connection::run()
{
	int ret;
	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 2;
	tdiff.tv_nsec = 0;

#ifdef HAVE_PPOLL
	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(fds));

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN;

	fds[1].fd = m_socket->getFD();
	fds[1].events = POLLIN;
#else
#ifdef HAVE_PSELECT
	int maxfd;
	fd_set checkfds;

	FD_ZERO(&checkfds);
	FD_SET(m_notify.notifyFD(), &checkfds);
	FD_SET(m_socket->getFD(), &checkfds);

	(m_notify.notifyFD() > m_socket->getFD()) ?
		(maxfd = m_notify.notifyFD()) : (maxfd = m_socket->getFD());
#endif
#endif

	time_t listenSince = 0;

	for (;;) {

#ifdef HAVE_PPOLL
		// wait for new fd event
		ret = ppoll(fds, nfds, &tdiff, NULL);
#else
#ifdef HAVE_PSELECT
		// set readfds to inital checkfds
		fd_set readfds = checkfds;
		// wait for new fd event
		ret = pselect(maxfd + 1, &readfds, NULL, NULL, &tdiff, NULL);
#endif
#endif

		bool newData = false;
		if (ret != 0) {
#ifdef HAVE_PPOLL
			// new data from notify
			if (fds[0].revents & POLLIN)
				break;

			// new data from socket
			newData = fds[1].revents & POLLIN;
#else
#ifdef HAVE_PSELECT
			// new data from notify
			if (FD_ISSET(m_notify.notifyFD(), &readfds))
				break;

			// new data from socket
			newData = FD_ISSET(m_socket->getFD(), &readfds);
#endif
#endif
		}

		if (newData == true || m_listening == true) {
			char data[256];
			size_t datalen = 0;

			if (newData == true) {
				if (m_socket->isValid() == true)
					datalen = m_socket->recv(data, sizeof(data)-1);
				else
					break;

				// removed closed socket
				if (datalen <= 0 || strncasecmp(data, "QUIT", 4) == 0)
					break;
			}

			// decode client data
			data[datalen] = '\0';
			NetMessage message(data, m_listening, listenSince);
			m_netQueue->add(&message);

			// wait for result
			L.log(net, debug, "[%05d] wait for result", getID());
			message.waitSignal();

			L.log(net, debug, "[%05d] result added", getID());
			string result = message.getResult();

			// remove help sign for Connection::waitSignal()
			result.erase(remove(result.begin(), result.end(), '\r'), result.end());

			if (m_socket->isValid() == false)
				break;

			m_socket->send(result.c_str(), result.size());
			m_listening = message.isListening(listenSince);
		}

	}

	delete m_socket;
	L.log(net, trace, "[%05d] connection closed", getID());
}


Network::Network(const bool local, const int port, WQueue<NetMessage*>* netQueue)
	: m_netQueue(netQueue), m_listening(false)
{
	if (local == true)
		m_tcpServer = new TCPServer(port, "127.0.0.1");
	else
		m_tcpServer = new TCPServer(port, "0.0.0.0");

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

	stop();
	join();

	delete m_tcpServer;
}

void Network::run()
{
	if (m_listening == false)
		return;

	int ret;
	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 1;
	tdiff.tv_nsec = 0;

#ifdef HAVE_PPOLL
	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(fds));

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN;

	fds[1].fd = m_tcpServer->getFD();
	fds[1].events = POLLIN;
#else
#ifdef HAVE_PSELECT
	int maxfd;
	fd_set checkfds;

	FD_ZERO(&checkfds);
	FD_SET(m_notify.notifyFD(), &checkfds);
	FD_SET(m_tcpServer->getFD(), &checkfds);

	(m_notify.notifyFD() > m_tcpServer->getFD()) ?
		(maxfd = m_notify.notifyFD()) : (maxfd = m_tcpServer->getFD());
#endif
#endif

	for (;;) {

#ifdef HAVE_PPOLL
		// wait for new fd event
		ret = ppoll(fds, nfds, &tdiff, NULL);
#else
#ifdef HAVE_PSELECT
		// set readfds to inital checkfds
		fd_set readfds = checkfds;
		// wait for new fd event
		ret = pselect(maxfd + 1, &readfds, NULL, NULL, &tdiff, NULL);
#endif
#endif

		if (ret == 0) {
			cleanConnections();
			continue;
		}

#ifdef HAVE_PPOLL
		// new data from notify
		if (fds[0].revents & POLLIN) {
			return;
		}

		// new data from socket
		if (fds[1].revents & POLLIN) {
#else
#ifdef HAVE_PSELECT
		// new data from notify
		if (FD_ISSET(m_notify.notifyFD(), &readfds)) {
			return;
		}

		// new data from socket
		if (FD_ISSET(m_tcpServer->getFD(), &readfds)) {
#endif
#endif

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
}

void Network::cleanConnections()
{
	list<Connection*>::iterator c_it;
	for (c_it = m_connections.begin(); c_it != m_connections.end(); c_it++) {
		if ((*c_it)->isRunning() == false) {
			Connection* connection = *c_it;
			c_it = m_connections.erase(c_it);
			delete connection;
			L.log(net, debug, "dead connection removed - %d", m_connections.size());
		}
	}
}


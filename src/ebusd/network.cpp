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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "network.h"
#include "log.h"
#include <cstring>

#ifdef HAVE_PPOLL
#include <poll.h>
#endif

using namespace std;

int Connection::m_ids = 0;

void Connection::run()
{
	int ret;
	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 2;
	tdiff.tv_nsec = 0;
	int notifyFD = m_notify.notifyFD();
	int sockFD = m_socket->getFD();

#ifdef HAVE_PPOLL
	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(fds));

	fds[0].fd = notifyFD;
	fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	fds[1].fd = sockFD;
	fds[1].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
#else
#ifdef HAVE_PSELECT
	int maxfd = (notifyFD > sockFD) ? notifyFD : sockFD;
	fd_set checkfds, exceptfds;

	FD_ZERO(&checkfds);
	FD_SET(notifyFD, &checkfds);
	FD_SET(sockFD, &checkfds);

	FD_ZERO(&exceptfds);
	FD_SET(notifyFD, &exceptfds);
	FD_SET(sockFD, &exceptfds);
#endif
#endif

	time_t listenSince = 0;
	bool closed = false;

	while (!closed) {
#ifdef HAVE_PPOLL
		// wait for new fd event
		ret = ppoll(fds, nfds, &tdiff, NULL);
#else
#ifdef HAVE_PSELECT
		// set readfds to inital checkfds
		fd_set readfds = checkfds;
		// wait for new fd event
		ret = pselect(maxfd + 1, &readfds, NULL, &exceptfds, &tdiff, NULL);
#endif
#endif
		bool newData = false;
		if (ret != 0) {
#ifdef HAVE_PPOLL
			// new data from notify
			if (ret < 0 || (fds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLRDHUP)) || (fds[1].revents & (POLLERR | POLLHUP))) {
				break;
			}
			// new data from socket
			newData = fds[1].revents & POLLIN;
			closed = fds[1].revents & POLLRDHUP;
#else
#ifdef HAVE_PSELECT
			// new data from notify
			if (ret < 0 || FD_ISSET(notifyFD, &readfds) || FD_ISSET(notifyFD, &exceptfds))
				break;

			// new data from socket
			newData = FD_ISSET(sockFD, &readfds);
			closed = FD_ISSET(sockFD, &exceptfds);
#endif
#endif
		}

		if (newData || m_listening) {
			char data[256];
			size_t datalen = 0;

			if (newData) {
				if (!m_socket->isValid())
					break;

				datalen = m_socket->recv(data, sizeof(data)-1);

				// remove closed socket
				if (datalen <= 0)
					break;
			}

			// decode client data
			data[datalen] = '\0';
			NetMessage message(data, m_listening, listenSince);
			m_netQueue->add(&message);

			// wait for result
			logDebug(lf_network, "[%05d] wait for result", getID());
			string result = message.getResult();

			if (!m_socket->isValid())
				break;

			m_socket->send(result.c_str(), result.size());
			m_listening = message.isListening(listenSince);

			if (message.isDisconnect())
				break;
		}

	}

	delete m_socket;
	m_socket = NULL;
	logInfo(lf_network, "[%05d] connection closed", getID());
}


Network::Network(const bool local, const int port, WQueue<NetMessage*>* netQueue)
	: m_netQueue(netQueue), m_listening(false)
{
	if (local)
		m_tcpServer = new TCPServer(port, "127.0.0.1");
	else
		m_tcpServer = new TCPServer(port, "0.0.0.0");

	if (m_tcpServer != NULL && m_tcpServer->start() == 0)
		m_listening = true;

}

Network::~Network()
{
	stop();

	while (!m_connections.empty()) {
		Connection* connection = m_connections.back();
		m_connections.pop_back();
		connection->stop();
		connection->join();
		delete connection;
	}
	join();

	if (m_tcpServer != NULL)
		delete m_tcpServer;
}

void Network::run()
{
	if (!m_listening)
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

	while (true) {

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
			logInfo(lf_network, "[%05d] connection opened %s", connection->getID(), socket->getIP().c_str());
		}
	}
}

void Network::cleanConnections()
{
	list<Connection*>::iterator c_it;
	for (c_it = m_connections.begin(); c_it != m_connections.end(); c_it++) {
		if (!(*c_it)->isRunning()) {
			Connection* connection = *c_it;
			c_it = m_connections.erase(c_it);
			delete connection;
			logDebug(lf_network, "dead connection removed - %d", m_connections.size());
		}
	}
}


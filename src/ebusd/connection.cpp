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

#include "connection.h"
#include "logger.h"
#include <cstring>
#include <poll.h>

extern LogInstance& L;

int Connection::m_sum = 0;

void Connection::addResult(Message message)
{
	Message* tmp = new Message(Message(message));
	m_result.add(tmp);
}

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
			m_data->add(new Message(data, this));

			// wait for result
			L.log(net, debug, "[%05d] wait for result", getID());
			Message* message = m_result.remove();

			L.log(net, debug, "[%05d] result added", getID());
			std::string result(message->getData());

			if (m_socket->isValid() == true)
				m_socket->send(result.c_str(), result.size());
			else
				break;

			delete message;

		}

	}

	delete m_socket;
	m_running = false;
	L.log(net, trace, "[%05d] connection closed", getID());

	return NULL;
}


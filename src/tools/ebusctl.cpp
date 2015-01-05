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

#include "appl.h"
#include "tcpsocket.h"
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sstream>

#ifdef HAVE_PPOLL
#include <poll.h>
#endif

using namespace std;

Appl& A = Appl::Instance("Command", "{Args...}");

void define_args()
{
	A.setVersion("ebusctl is part of """PACKAGE_STRING"");

	A.addText(" 'ebusctl' is a tcp socket client for ebusd.\n\n"
		  "   hint: try 'help' for available ebusd commands.\n\n"
		  "Options:\n");

	A.addOption("server", "s", OptVal("localhost"), dt_string, ot_mandatory,
		    "name or ip (localhost)");

	A.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory,
		    "port (8888)");

}

string fetchData(TCPSocket* socket, bool& listening)
{
	char data[1024];
	ssize_t datalen;
	ostringstream ss;
	string message;

	int ret;
	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 0;
	tdiff.tv_nsec = 1E8;

#ifdef HAVE_PPOLL
	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(fds));

	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	fds[1].fd = socket->getFD();
	fds[1].events = POLLIN;
#else
#ifdef HAVE_PSELECT
	int maxfd;
	fd_set checkfds;

	FD_ZERO(&checkfds);
	FD_SET(STDIN_FILENO, &checkfds);
	FD_SET(socket->getFD(), &checkfds);

	(STDIN_FILENO > socket->getFD()) ?
		(maxfd = notify.notifyFD()) : (maxfd = socket->getFD());
#endif
#endif

	while(true) {

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
		bool newInput = false;
		if (ret != 0) {
#ifdef HAVE_PPOLL
			// new data from notify
			newInput = fds[0].revents & POLLIN;

			// new data from socket
			newData = fds[1].revents & POLLIN;
#else
#ifdef HAVE_PSELECT
			// new data from notify
			newInput = FD_ISSET(STDIN_FILENO, &readfds);

			// new data from socket
			newData = FD_ISSET(socket->getFD(), &readfds);
#endif
#endif
		}

			if (newData == true) {
				if (socket->isValid() == true) {
					datalen = socket->recv(data, sizeof(data));

					if (datalen < 0) {
						perror("recv");
						break;
					}

					for (int i = 0; i < datalen; i++)
						ss << data[i];

					if ((ss.str().length() >= 2
					&& ss.str()[ss.str().length()-2] == '\n'
					&& ss.str()[ss.str().length()-1] == '\n')
					|| listening == true)
						break;

				}
				else
					break;
			}
			else if (newInput == true) {
				getline(cin, message);
				message += '\n';

				socket->send(message.c_str(), message.size());

				if (strncasecmp(message.c_str(), "QUIT", 4) == 0
				|| strncasecmp(message.c_str(), "STOP", 4) == 0)
					exit(EXIT_SUCCESS);

				message.clear();
			}

	}

	return ss.str();
}

bool connect(const char* host, int port, bool once)
{

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(host, port);

	if (socket != NULL) {
		do {
			string message;
			bool listening = false;

			if (once == false) {
				cout << host << ": ";
				getline(cin, message);
			}
			else {
				message = A.getCommand();
				for (int i = 0; i < A.numArgs(); i++) {
					message += " ";
					message += A.getArg(i);
				}
			}

			socket->send(message.c_str(), message.size());

			if (strncasecmp(message.c_str(), "QUIT", 4) != 0
			&& strncasecmp(message.c_str(), "STOP", 4) != 0)

				if (strncasecmp(message.c_str(), "L", 1) == 0
				|| strncasecmp(message.c_str(), "LISTEN", 6) == 0) {
					listening = true;
					while (listening) {
						string result(fetchData(socket, listening));
						cout << result;
						if (strncasecmp(result.c_str(), "LISTEN STOPPED", 14) == 0)
							break;
					}
				}
				else
					cout << fetchData(socket, listening);

			else
				break;

		} while (once == false);

		delete socket;

	}
	else
		cout << "error connecting to " << host << ":" << port << endl;

	delete client;

	if (once == false)
		return false;

	return true;
}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	if (A.parseArgs(argc, argv) == false)
		exit(EXIT_FAILURE);

	if (A.missingCommand() == true)
		connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"), false);
	else
		connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"), true);

	exit(EXIT_SUCCESS);
}


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

bool connect(const char* host, int port, bool once)
{

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(host, port);

	if (socket != NULL) {

		do {
			string message;

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

			if (strncasecmp(message.c_str(), "QUIT", 4) != 0 && strncasecmp(message.c_str(), "STOP", 4) != 0) {
				char data[1024];
				ssize_t datalen = 0;

				do {
					if (datalen > 0) {
						data[datalen] = '\0';
						cout << data;
					}

					memset(data, 0, sizeof(data));
					datalen = socket->recv(data, sizeof(data)-1);

					if (datalen < 0) {
						perror("send");
						break;
					}

				} while (data[datalen-2] != '\n' || data[datalen-1] != '\n');

				cout << data;
			}
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


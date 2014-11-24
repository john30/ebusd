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
#include <iostream>
#include <cstdlib>

using namespace std;

Appl& A = Appl::Instance(true, true);

void define_args()
{
	A.setVersion("ebusctl is part of """PACKAGE_STRING"");

	A.addText(" 'ebusctl' is a tcp socket client for ebusd.\n\n"
		  "Command: 'help' show available ebusd commands.\n\n"
		  "Options:\n");

	A.addOption("server", "s", OptVal("localhost"), dt_string, ot_mandatory,
		    "name or ip (localhost)");

	A.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory,
		    "port (8888)");

}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	A.parseArgs(argc, argv);

	if (A.missingCommand() == true) {
		cout << "interactive mode started." << endl;
		exit(EXIT_FAILURE);
	}

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"));

	if (socket != NULL) {
		// build message
		string message(A.getCommand());
		for (int i = 0; i < A.numArgs(); i++) {
			message += " ";
			message += A.getArg(i);
		}

		socket->send(message.c_str(), message.size());

		char data[1024];
		size_t datalen;

		datalen = socket->recv(data, sizeof(data)-1);
		data[datalen] = '\0';

		cout << data;

		delete socket;
	}
	else
		cout << "error connecting to " << A.getOptVal<const char*>("server")
		     << ":" << A.getOptVal<int>("port") << endl;

	delete client;

	exit(EXIT_SUCCESS);
}


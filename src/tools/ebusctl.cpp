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
#include <sstream>

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

enum CommandType {
     ct_open,
     ct_exit,
     ct_help,
     ct_invalid
};

CommandType getCase(const string& item)
{
	if (strcasecmp(item.c_str(), "OPEN") == 0) return ct_open;
	if (strcasecmp(item.c_str(), "EXIT") == 0) return ct_exit;
	if (strcasecmp(item.c_str(), "HELP") == 0) return ct_help;

	return ct_invalid;
}

bool connect(const char* host, int port, bool once=true)
{

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(host, port);

	if (socket != NULL) {

		do {
			string message;

			if (once == false) {
				cout << "online: ";
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
				size_t datalen;

				datalen = socket->recv(data, sizeof(data)-1);
				data[datalen] = '\0';

				cout << data;
			}
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
	A.parseArgs(argc, argv);

	if (A.missingCommand() == true) {
		cout << "interactive mode started." << endl;

		bool running = true;

		do {
			string input, token;
			vector<string> cmd;

			cout << "$: ";
			getline(cin, input);

			// prepare input
			istringstream stream(input);
			while (getline(stream, token, ' ') != 0)
				cmd.push_back(token);

			if (cmd.size() == 0)
				cout << "command missing" << endl;

			switch (getCase(cmd[0])) {
			case ct_invalid:
				cout << "command not found" << endl;
				break;

			case ct_open:
				{
					bool ret = true;
					cout << "connect to..." << endl;
					if (cmd.size() == 1)
						ret = connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"), false);
					else if (cmd.size() == 2)
						ret = connect(cmd[1].c_str(), A.getOptVal<int>("port"), false);
					else if (cmd.size() == 3)
						ret = connect(cmd[1].c_str(), atoi(cmd[2].c_str()), false);
					else
						cout << "open [host [port]]" << endl;

					running = ret;
				}

				break;

			case ct_exit:
				running = false;
				break;

			case ct_help:
				cout << "commands:" << endl
				     << " open - open connection to ebusd   'open [host [port]]'" << endl
				     << " exit - exit ebusctl" << endl
				     << " help - print this page" << endl;
				break;

			default:
				break;
			}

		} while (running == true);

		exit(EXIT_SUCCESS);
	}

	connect(A.getOptVal<const char*>("server"), A.getOptVal<int>("port"));

	exit(EXIT_SUCCESS);
}


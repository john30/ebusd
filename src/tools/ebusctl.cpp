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

#include <argp.h>
#include "tcpsocket.h"
#include <string.h>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sstream>

#ifdef HAVE_PPOLL
#include <poll.h>
#endif

using namespace std;

/** A structure holding all program options. */
struct options
{
	const char* server; //!< ebusd server host (name or ip) [localhost]
	int port; //!< ebusd server port [8888]

	char* const *args; //!< arguments to pass to ebusd
	int argCount; //!< number of arguments to pass to ebusd
};

/** the program options. */
static struct options opt = {
	"localhost", // server
	8888, // port

	NULL, // args
	0 // argCount
};

/** the version string of the program. */
const char *argp_program_version = "ebusctl of """PACKAGE_STRING"";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = ""PACKAGE_BUGREPORT"";

/** the documentation of the program. */
static const char argpdoc[] =
	"Client for acessing "PACKAGE" via TCP.\n"
	"\v"
	"If given, send COMMAND together with CMDOPT options to "PACKAGE".\n"
	"Use 'help' as COMMAND for help on available "PACKAGE" commands.";

/** the description of the accepted arguments. */
static char argpargsdoc[] = "\nCOMMAND [CMDOPT...]";

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
	{NULL,       0,   NULL, 0, "Options:", 1 },
	{"server", 's', "HOST", 0, "Connect to HOST running "PACKAGE" (name or IP) [localhost]", 0 },
	{"port",   'p', "PORT", 0, "Connect to PORT on HOST [8888]", 0 },

	{NULL,       0,   NULL, 0, NULL, 0 },
};

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct options *opt = (struct options*)state->input;
	char* strEnd = NULL;
	switch (key) {
	// Device settings:
	case 's': // --server=localhost
		if (arg == NULL || arg[0] == 0) {
			argp_error(state, "invalid server");
			return EINVAL;
		}
		opt->server = arg;
		break;
	case 'p': // --port=8888
		opt->port = strtol(arg, &strEnd, 10);
		if (strEnd == NULL || *strEnd != 0 || opt->port < 1 || opt->port > 65535) {
			argp_error(state, "invalid port");
			return EINVAL;
		}
		break;
	case ARGP_KEY_ARGS:
		opt->args = state->argv + state->next;
		opt->argCount = state->argc - state->next;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
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

				if (strcasecmp(message.c_str(), "Q") == 0
				|| strcasecmp(message.c_str(), "QUIT") == 0
				|| strcasecmp(message.c_str(), "STOP") == 0)
					exit(EXIT_SUCCESS);

				message.clear();
			}

	}

	return ss.str();
}

void connect(const char* host, int port, char* const *args, int argCount)
{

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(host, port);

	bool once = args != NULL && argCount > 0;
	if (socket != NULL) {
		do {
			string message;
			bool listening = false;

			if (once == false) {
				cout << host << ": ";
				getline(cin, message);
			}
			else {
				for (int i = 0; i < argCount; i++) {
					if (i > 0)
						message += " ";
					bool quote = strchr(args[i], ' ') != NULL && strchr(args[i], '"') == NULL;
					if (quote)
						message += "\"";
					message += args[i];
					if (quote)
						message += "\"";
				}
			}

			socket->send(message.c_str(), message.size());

			if (strcasecmp(message.c_str(), "Q") == 0
			|| strcasecmp(message.c_str(), "QUIT") == 0
			|| strcasecmp(message.c_str(), "STOP") == 0)
				break;

			if (message.length() > 0) {
				if (strcasecmp(message.c_str(), "L") == 0
				|| strcasecmp(message.c_str(), "LISTEN") == 0) {
					listening = true;
					while (listening && cin.eof() == false) {
						string result(fetchData(socket, listening));
						cout << result;
						if (strcasecmp(result.c_str(), "LISTEN STOPPED") == 0)
							break;
					}
				}
				else
					cout << fetchData(socket, listening);
			}

		} while (once == false && cin.eof() == false);

		delete socket;

	}
	else
		cout << "error connecting to " << host << ":" << port << endl;

	delete client;
}

int main(int argc, char* argv[])
{
	struct argp argp = { argpoptions, parse_opt, argpargsdoc, argpdoc, NULL, NULL, NULL };
	setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &opt) != 0)
		return EINVAL;

	connect(opt.server, opt.port, opt.args, opt.argCount);

	exit(EXIT_SUCCESS);
}


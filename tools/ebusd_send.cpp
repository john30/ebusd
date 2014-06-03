/*
 * Copyright (C) Roland Jax 2012-2014 <roland.jax@liwest.at>
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

#include "appl.h"
#include "tcpsocket.h"
#include <iostream>
#include <cstdlib>

Appl& A = Appl::Instance();

void define_args()
{
	A.addArgs("Arg1 Arg2 Arg3 (Arg4)\n\n"
		  "Arg1-4:\n"
		  "  get class cmd (sub)\n"
		  "  set class cmd value\n"
		  "  cyc class cmd (sub)\n"
		  "  hex type value (value: ZZ PB SB NN Dx)", 3);

	A.addItem("p_server", Appl::Param("localhost"), "s", "server",
		  "name or ip (localhost)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_port", Appl::Param(8888), "p", "port",
		  "port (8888)\n",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_help", Appl::Param(false), "h", "help",
		  "print this message",
		  Appl::type_bool, Appl::opt_none);
}

int main(int argc, char* argv[])
{
	// define Arguments and Application variables
	define_args();

	// parse Arguments
	if (A.parseArgs(argc, argv) == false) {
		A.printArgs();
		exit(EXIT_FAILURE);
	}

	// print Help
	if (A.getParam<bool>("p_help") == true) {
		A.printArgs();
		exit(EXIT_SUCCESS);
	}

	// build message
	std::string message(A.getArg(0));
	for (size_t i = 1; i < A.numArg(); i++) {
		message += " ";
		message += A.getArg(i);
	}

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(A.getParam<const char*>("p_server"), A.getParam<int>("p_port"));

	if (socket != NULL) {


		socket->send(message.c_str(), message.size());

		char data[256];
		size_t datalen;

		datalen = socket->recv(data, sizeof(data)-1);
		data[datalen] = '\0';

		std::cout << data;

		delete socket;
	}

	delete client;

	return 0;

}


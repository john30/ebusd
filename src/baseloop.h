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

#ifndef BASELOOP_H_
#define BASELOOP_H_

#include "libebus.h"
#include "network.h"
#include "ebusloop.h"

using namespace libebus;


class BaseLoop
{

public:
	BaseLoop();
	~BaseLoop();

	void start();

	void addMessage(Message* message) { m_queue.add(message); }

private:
	Commands* m_commands;
	EBusLoop* m_ebusloop;
	Network* m_network;

	WQueue<Message*> m_queue;

	enum ClientCommand {
			     get,       // get ebus data
			     set,       // set ebus value
			     cyc,       // fetch cycle data
			     hex,       // send hex value
			     dump,      // change dump state
			     log,	// logger settings
			     config,    // ebus configuration
			     help,      // print commands
			     notfound
			   };

	ClientCommand getCase(const std::string& item)
	{
		if (strcasecmp(item.c_str(), "GET") == 0) return get;
		if (strcasecmp(item.c_str(), "SET") == 0) return set;
		if (strcasecmp(item.c_str(), "CYC") == 0) return cyc;
		if (strcasecmp(item.c_str(), "HEX") == 0) return hex;
		if (strcasecmp(item.c_str(), "DUMP") == 0) return dump;
		if (strcasecmp(item.c_str(), "LOG") == 0) return log;
		if (strcasecmp(item.c_str(), "CONFIG") == 0) return config;
		if (strcasecmp(item.c_str(), "HELP") == 0) return help;

		return notfound;
	}

	std::string decodeMessage(const std::string& data);

};

#endif // BASELOOP_H_

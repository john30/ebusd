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
#include "cycdata.h"

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
	CYCData* m_cycdata;
	EBusLoop* m_ebusloop;
	Network* m_network;

	WQueue<Message*> m_queue;

	enum ClientCommand {
			     get,       // get ebus data
			     set,       // set ebus value
			     cyc,       // fetch cycle data
			     hex,       // send hex value
			     dump,      // change dump state
			     logarea,   // change log area
			     loglevel,  // change log level
			     //~ cfgreload, // reload ebus configuration
			     help,      // print commands
			     notfound
			   };

	ClientCommand getCase(const std::string& item)
	{
		if (strcasecmp(item.c_str(), "get") == 0) return get;
		if (strcasecmp(item.c_str(), "set") == 0) return set;
		if (strcasecmp(item.c_str(), "cyc") == 0) return cyc;
		if (strcasecmp(item.c_str(), "hex") == 0) return hex;
		if (strcasecmp(item.c_str(), "dump") == 0) return dump;
		if (strcasecmp(item.c_str(), "logarea") == 0) return logarea;
		if (strcasecmp(item.c_str(), "loglevel") == 0) return loglevel;
		//~ if (strcasecmp(item.c_str(), "cfgreload") == 0) return cfgreload;
		if (strcasecmp(item.c_str(), "help") == 0) return help;

		return notfound;
	}

	std::string decodeMessage(const std::string& data);

};

#endif // BASELOOP_H_

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
#include "ebusloop.h"
#include "cycdata.h"
#include "wqueue.h"
#include <string>

using namespace libebus;

class Connection;

class Message
{

public:
	Message(const std::string data, void* source = NULL) : m_data(data), m_source(source) {}
	Message(const Message& src) : m_data(src.m_data), m_source(src.m_source) {}

	std::string getData() const { return m_data; }
	void* getSource() const { return m_source; }

private:
	std::string m_data;
	void* m_source;

};

class BaseLoop
{

public:
	BaseLoop(EBusLoop* ebusloop, CYCData* cycdata, Commands* commands)
		: m_ebusloop(ebusloop), m_cycdata(cycdata), m_commands(commands) {}

	void start();

	WQueue<Message*>* getQueue() { return &m_queue; }
	void addMessage(Message* message) { m_queue.add(message); }

private:
	EBusLoop* m_ebusloop;
	CYCData* m_cycdata;
	Commands* m_commands;
	WQueue<Message*> m_queue;

	enum ClientCommand {
			     get,       // get ebus data
			     set,       // set ebus value
			     cyc,       // fetch cycle data
			     hex,       // send hex value
			     dump,      // change dump state
			     logarea,   // change log area
			     loglevel,  // change log level
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
		if (strcasecmp(item.c_str(), "help") == 0) return help;

		return notfound;
	}

	std::string decodeMessage(const std::string& data);

};

#endif // BASELOOP_H_

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

#ifndef BASELOOP_H_
#define BASELOOP_H_

#include "commands.h"
#include "network.h"
#include "busloop.h"

using namespace std;

/** \file baseloop.h */

/** possible client commands */
enum CommandType {
     ct_get,       /*!< get ebus data */
     ct_set,       /*!< set ebus value */
     ct_cyc,       /*!< fetch cycle data */
     ct_hex,       /*!< send hex value */
     ct_scan,      /*!< scan ebus */
     ct_log,       /*!< logger settings */
     ct_raw,       /*!< toggle log raw data */
     ct_dump,      /*!< toggle dump state */
     ct_reload,    /*!< reload ebus configuration */
     ct_help,      /*!< print commands */
     ct_invalid    /*!< invalid */
};

/**
 * @brief class baseloop which handle client messages.
 */
class BaseLoop
{

public:
	/**
	 * @brief construct the baseloop and creates commads, network and busloop subsystems.
	 */
	BaseLoop();

	/**
	 * @brief destructor.
	 */
	~BaseLoop();

	/**
	 * @brief start baseloop instance.
	 */
	void start();

	/**
	 * @brief add a new network message to internal message queue.
	 * @param message the network message.
	 */
	void addMessage(NetMessage* message) { m_netQueue.add(message); }

private:
	/** the commands instance */
	Commands* m_commands;

	/** the busloop instance */
	BusLoop* m_busloop;

	/** the network instance */
	Network* m_network;

	/** queue for network messages */
	WQueue<NetMessage*> m_netQueue;

	/**
	 * @brief compare client command with defined.
	 * @param item the client command to compare.
	 * @return the founded client command type.
	 */
	CommandType getCase(const string& item)
	{
		if (strcasecmp(item.c_str(), "GET") == 0) return ct_get;
		if (strcasecmp(item.c_str(), "SET") == 0) return ct_set;
		if (strcasecmp(item.c_str(), "CYC") == 0) return ct_cyc;
		if (strcasecmp(item.c_str(), "HEX") == 0) return ct_hex;
		if (strcasecmp(item.c_str(), "SCAN") == 0) return ct_scan;
		if (strcasecmp(item.c_str(), "LOG") == 0) return ct_log;
		if (strcasecmp(item.c_str(), "RAW") == 0) return ct_raw;
		if (strcasecmp(item.c_str(), "DUMP") == 0) return ct_dump;
		if (strcasecmp(item.c_str(), "RELOAD") == 0) return ct_reload;
		if (strcasecmp(item.c_str(), "HELP") == 0) return ct_help;

		return ct_invalid;
	}

	/**
	 * @brief decode and execute client message
	 * @param data the data string to decode
	 * @return result string to send back to client
	 */
	string decodeMessage(const string& data);

};

#endif // BASELOOP_H_

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

#include "message.h"
#include "network.h"
#include "bushandler.h"

/** \file baseloop.h */

using namespace std;

/** possible client commands */
enum CommandType {
	ct_read,      //!< read ebus values
	ct_write,     //!< write ebus values
	ct_find,      //!< find values
	ct_listen,    //!< listen for updates to values
	ct_scan,      //!< scan ebus
	ct_log,       //!< logger settings
	ct_raw,       //!< toggle log raw data
	ct_dump,      //!< toggle dump state
	ct_reload,    //!< reload ebus configuration
	ct_help,      //!< print commands
	ct_invalid    //!< invalid
};

/**
 * class baseloop which handle client messages.
 */
class BaseLoop
{

public:
	/**
	 * Construct the base loop and create messaging, network and bus handling subsystems.
	 */
	BaseLoop();

	/**
	 * Destructor.
	 */
	~BaseLoop();

	/**
	 * Load the message definitions.
	 * @return the result code.
	 */
	result_t loadMessages();

	/**
	 * start baseloop instance.
	 */
	void start();

	/**
	 * add a new network message to internal message queue.
	 * @param message the network message.
	 */
	void addMessage(NetMessage* message) { m_netQueue.add(message); }

	/**
	 * Create a log message for a received/sent raw data byte.
	 * @param byte the raw data byte.
	 * @param received true if the byte was received, false if it was sent.
	 */
	static void logRaw(const unsigned char byte, bool received);

private:

	/** the @a DataFieldTemplates instance. */
	DataFieldTemplates* m_templates;

	/** the @a MessageMap instance. */
	MessageMap* m_messages;

	/** the own master address for sending on the bus. */
	unsigned char m_ownAddress;

	/** whether polling the messages is active. */
	bool m_pollActive;

	/** the @a Port instance. */
	Port* m_port;

	/** the @a BusHandler instance. */
	BusHandler* m_busHandler;

	/** the @a Network instance. */
	Network* m_network;

	/** queue for network messages */
	WQueue<NetMessage*> m_netQueue;

	/**
	 * compare client command with defined.
	 * @param item the client command to compare.
	 * @return the founded client command type.
	 */
	CommandType getCase(const string& item)
	{
		const char* str = item.c_str();
		if (strcasecmp(str, "R") == 0 || strcasecmp(str, "READ") == 0) return ct_read;
		if (strcasecmp(str, "W") == 0 || strcasecmp(str, "WRITE") == 0) return ct_write;
		if (strcasecmp(str, "F") == 0 || strcasecmp(str, "FIND") == 0) return ct_find;
		if (strcasecmp(str, "L") == 0 || strcasecmp(str, "LISTEN") == 0) return ct_listen;
		if (strcasecmp(str, "SCAN") == 0) return ct_scan;
		if (strcasecmp(str, "LOG") == 0) return ct_log;
		if (strcasecmp(str, "RAW") == 0) return ct_raw;
		if (strcasecmp(str, "DUMP") == 0) return ct_dump;
		if (strcasecmp(str, "RELOAD") == 0) return ct_reload;
		if (strcasecmp(str, "H") == 0 || strcasecmp(str, "HELP") == 0) return ct_help;

		return ct_invalid;
	}

	/**
	 * Decode and execute client message.
	 * @param data the data string to decode (may be empty).
	 * @param listening set to true when the client is in listening mode.
	 * @return result string to send back to client.
	 */
	string decodeMessage(const string& data, bool& listening);

	/**
	 * Get the updates received since the specified time.
	 * @param since the start time from which to add updates (inclusive).
	 * @param until the end time to which to add updates (exclusive).
	 * @return result string to send back to client.
	 */
	string getUpdates(time_t since, time_t until);

};

#endif // BASELOOP_H_

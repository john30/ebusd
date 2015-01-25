/*
 * Copyright (C) John Baier 2014-2015 <ebusd@johnm.de>
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

#ifndef MAINLOOP_H_
#define MAINLOOP_H_

#include "message.h"
#include "network.h"
#include "bushandler.h"

/** \file mainloop.h */

using namespace std;

/**
 * The main loop handling requests from connected clients.
 */
class MainLoop
{

public:
	/**
	 * Construct the main loop and create network and bus handling components.
	 * @param opt the program options.
	 * @param templates the @a DataFieldTemplates instance.
	 * @param messages the @a MessageMap instance.
	 */
	MainLoop(const struct options opt, DataFieldTemplates* templates, MessageMap* messages);

	/**
	 * Destructor.
	 */
	~MainLoop();

	/**
	 * Run the main loop.
	 */
	void run();

	/**
	 * Add a client @a NetMessage to the queue.
	 * @param message the client @a NetMessage to handle.
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
	unsigned char m_address;

	/** the created @a Port instance. */
	Port* m_port;

	/** the created @a BusHandler instance. */
	BusHandler* m_busHandler;

	/** the created @a Network instance. */
	Network* m_network;

	/** the queue for @a NetMessage instances. */
	WQueue<NetMessage*> m_netQueue;

	/**
	 * Decode and execute client message.
	 * @param data the data string to decode (may be empty).
	 * @param listening set to true when the client is in listening mode.
	 * @return result string to send back to the client.
	 */
	string decodeMessage(const string& data, bool& listening);

	/**
	 * Execute the read command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeRead(vector<string> &args);

	/**
	 * Execute the write command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeWrite(vector<string> &args);

	/**
	 * Execute the find command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeFind(vector<string> &args);

	/**
	 * Execute the listen command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @param listening set to true when the client is in listening mode.
	 * @return the result string.
	 */
	string executeListen(vector<string> &args, bool& listening);

	/**
	 * Execute the scan command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeScan(vector<string> &args);

	/**
	 * Execute the log command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeLog(vector<string> &args);

	/**
	 * Execute the raw command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeRaw(vector<string> &args);

	/**
	 * Execute the dump command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeDump(vector<string> &args);

	/**
	 * Execute the reload command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @return the result string.
	 */
	string executeReload(vector<string> &args);

	/**
	 * Execute the help command.
	 * @return the result string.
	 */
	string executeHelp();

	/**
	 * Get the updates received since the specified time.
	 * @param since the start time from which to add updates (inclusive).
	 * @param until the end time to which to add updates (exclusive).
	 * @return result string to send back to client.
	 */
	string getUpdates(time_t since, time_t until);

};

#endif // MAINLOOP_H_

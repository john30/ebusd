/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
class MainLoop : public Thread
{

public:
	/**
	 * Construct the main loop and create network and bus handling components.
	 * @param opt the program options.
	 * @param device the @a Device instance.
	 * @param messages the @a MessageMap instance.
	 */
	MainLoop(const struct options opt, Device *device, MessageMap* messages);

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
	void addMessage(NetMessage* message) { m_netQueue.push(message); }

private:

	/** the @a Device instance. */
	Device* m_device;

	/** the @a MessageMap instance. */
	MessageMap* m_messages;

	/** the own master address for sending on the bus. */
	const unsigned char m_address;

	/** whether to pick configuration files matching initial scan. */
	const bool m_scanConfig;

	/** the initial address to scan for @a m_scanConfig (@a ESC=none, 0xfe=broadcast ident, @a SYN=full scan, else: single slave address). */
	const unsigned char m_initialScan;

	/** whether to enable the hex command. */
	const bool m_enableHex;

	/** the created @a BusHandler instance. */
	BusHandler* m_busHandler;

	/** the created @a Network instance. */
	Network* m_network;

	/** the @a NetMessage @a Queue. */
	Queue<NetMessage*> m_netQueue;

	/** the path for HTML files served by the HTTP port. */
	string m_htmlPath;

	/**
	 * Decode and execute client message.
	 * @param data the data string to decode (may be empty).
	 * @param connected set to false when the client connection shall be closed.
	 * @param isHttp true for HTTP message.
	 * @param listening set to true when the client is in listening mode.
	 * @param reload set to true when the configuration files were reloaded.
	 * @return result string to send back to the client.
	 */
	string decodeMessage(const string& data, const bool isHttp, bool& connected, bool& listening, bool& reload);

	/**
	 * Parse the hex master message from the remaining arguments.
	 * @param args the arguments passed to the command.
	 * @param argPos the index of the first argument to parse.
	 * @param master the master @a SymbolString to write the data to.
	 * @return the result from parsing the arguments.
	 */
	result_t parseHexMaster(vector<string> &args, size_t argPos, SymbolString& master);

	/**
	 * Prepare the master part for the @a Message, send it to the bus and wait for the answer.
	 * @param message the @a Message instance.
	 * @param inputStr the input @a string from which to read master values (if any).
	 * @param dstAddress the destination address to set, or @a SYN to keep the address defined during construction.
	 * @return the result code.
	 */
	result_t readFromBus(Message* message, string inputStr, const unsigned char dstAddress=SYN);

	/**
	 * Execute the read command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeRead(vector<string> &args);

	/**
	 * Execute the write command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeWrite(vector<string> &args);

	/**
	 * Execute the hex command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeHex(vector<string> &args);

	/**
	 * Execute the find command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeFind(vector<string> &args);

	/**
	 * Execute the listen command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @param listening set to true when the client is in listening mode.
	 * @return the result string.
	 */
	string executeListen(vector<string> &args, bool& listening);

	/**
	 * Execute the state command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeState(vector<string> &args);

	/**
	 * Execute the grab command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeGrab(vector<string> &args);

	/**
	 * Execute the scan command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeScan(vector<string> &args);

	/**
	 * Execute the log command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeLog(vector<string> &args);

	/**
	 * Execute the raw command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeRaw(vector<string> &args);

	/**
	 * Execute the dump command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeDump(vector<string> &args);

	/**
	 * Execute the reload command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeReload(vector<string> &args);

	/**
	 * Execute the info command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @return the result string.
	 */
	string executeInfo(vector<string> &args);

	/**
	 * Execute the quit command.
	 * @param args the arguments passed to the command (starting with the command itself), or empty for help.
	 * @param connected set to false when the client connection shall be closed.
	 * @return the result string.
	 */
	string executeQuit(vector<string> &args, bool& connected);

	/**
	 * Execute the help command.
	 * @return the result string.
	 */
	string executeHelp();

	/**
	 * Execute the HTTP GET command.
	 * @param args the arguments passed to the command (starting with the command itself).
	 * @param connected set to false when the client connection shall be closed.
	 * @return the result string.
	 */
	string executeGet(vector<string> &args, bool& connected);

	/**
	 * Get the updates received since the specified time.
	 * @param since the start time from which to add updates (inclusive).
	 * @param until the end time to which to add updates (exclusive).
	 * @return result string to send back to client.
	 */
	string getUpdates(time_t since, time_t until);

};

#endif // MAINLOOP_H_

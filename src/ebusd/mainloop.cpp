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

#include "mainloop.h"
#include <iomanip>
#include <algorithm>
#include "main.h"
#include "log.h"
#include "data.h"

using namespace std;

MainLoop::MainLoop(const struct options opt, DataFieldTemplates* templates, MessageMap* messages)
	: m_templates(templates), m_messages(messages), m_address(opt.address)
{
	// create Port
	m_port = new Port(opt.device, opt.noDeviceCheck, opt.logRaw, &logRaw, opt.dump, opt.dumpFile, opt.dumpSize);
	m_port->open();

	if (m_port->isOpen() == false)
		logError(lf_bus, "can't open %s", m_port->getDeviceName());

	// create BusHandler
	m_busHandler = new BusHandler(m_port, m_messages,
			m_address, opt.answer,
			opt.acquireRetries, opt.sendRetries,
			opt.acquireTimeout, opt.receiveTimeout,
			opt.numberMasters, opt.pollInterval);
	m_busHandler->start("bushandler");

	// create network
	m_network = new Network(opt.localhost, opt.port, &m_netQueue);
	m_network->start("network");
}

MainLoop::~MainLoop()
{
	if (m_network != NULL) {
		delete m_network;
		m_network = NULL;
	}

	if (m_busHandler != NULL) {
		m_busHandler->stop();
		m_busHandler->join();
		delete m_busHandler;
		m_busHandler = NULL;
	}

	if (m_port != NULL) {
		delete m_port;
		m_port = NULL;
	}

	m_messages->clear();
	m_templates->clear();
}

void MainLoop::run()
{
	for (;;) {
		string result;

		// recv new message from client
		NetMessage* message = m_netQueue.remove();
		string data = message->getData();

		time_t since, until;
		time(&until);
		bool listening = message->isListening(since);
		if (listening == false)
			since = until;

		if (data.length() > 0) {
			data.erase(remove(data.begin(), data.end(), '\r'), data.end());
			data.erase(remove(data.begin(), data.end(), '\n'), data.end());

			logNotice(lf_main, ">>> %s", data.c_str());

			// decode message
			if (strcasecmp(data.c_str(), "STOP") != 0)
				result = decodeMessage(data, listening);
			else
				result = "done";

			logNotice(lf_main, "<<< %s", result.c_str());
			result += "\n\n";
		}
		if (listening == true) {
			result += getUpdates(since, until);
		}

		// send result to client
		message->setResult(result, listening, until);

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0)
			return;
	}
}

void MainLoop::logRaw(const unsigned char byte, bool received) {
	if (received == true)
		logNotice(lf_bus, "<%02x", byte);
	else
		logNotice(lf_bus, ">%02x", byte);
}

string MainLoop::decodeMessage(const string& data, bool& listening)
{
	ostringstream result;

	// prepare data
	string token, previous;
	istringstream stream(data);
	vector<string> args;
	bool escaped = false;

	while (getline(stream, token, ' ') != 0) {
		if (escaped == true) {
			args.pop_back();
			if (token.length() > 0 && token[token.length()-1] == '"') {
				token = token.substr(0, token.length() - 1);
				escaped = false;
			}
			token = previous + " " + token;
		}
		else if (token.length() == 0) // allow multiple space chars for a single delimiter
			continue;
		else if (token[0] == '"') {
			token = token.substr(1);
			if (token.length() > 0 && token[token.length()-1] == '"')
				token = token.substr(0, token.length() - 1);
			else
				escaped = true;
		}
		args.push_back(token);
		previous = token;
	}

	if (args.size() == 0)
		return "command missing";


	const char* str = args[0].c_str();
	if (strcasecmp(str, "R") == 0 || strcasecmp(str, "READ") == 0)
		return executeRead(args);
	if (strcasecmp(str, "W") == 0 || strcasecmp(str, "WRITE") == 0)
		return executeWrite(args);
	if (strcasecmp(str, "F") == 0 || strcasecmp(str, "FIND") == 0)
		return executeFind(args);
	if (strcasecmp(str, "L") == 0 || strcasecmp(str, "LISTEN") == 0)
		return executeListen(args, listening);
	if (strcasecmp(str, "SCAN") == 0)
		return executeScan(args);
	if (strcasecmp(str, "LOG") == 0)
		return executeLog(args);
	if (strcasecmp(str, "RAW") == 0)
		return executeRaw(args);
	if (strcasecmp(str, "DUMP") == 0)
		return executeDump(args);
	if (strcasecmp(str, "RELOAD") == 0)
		return executeReload(args);
	if (strcasecmp(str, "H") == 0 || strcasecmp(str, "HELP") == 0)
		return executeHelp();

	return "command not found";
}

string MainLoop::executeRead(vector<string> &args)
{
	size_t argPos = 1;
	time_t maxAge = 5*60;
	bool verbose = false;
	string clazz;
	while (args.size() > argPos && args[argPos][0] == '-') {
		if (args[argPos] == "-f") {
			maxAge = 0;
		}
		else if (args[argPos] == "-v") {
			verbose = true;
		}
		else if (args[argPos] == "-m") {
			argPos++;
			if (args.size() > argPos) {
				result_t result;
				maxAge = parseInt(args[argPos].c_str(), 10, 0, 24*60*60, result);
				if (result != RESULT_OK) {
					argPos = 0; // print usage
					break;
				}
			}
			else {
				argPos = 0; // print usage
				break;
			}
		}
		else if (args[argPos] == "-c") {
			argPos++;
			if (argPos >= args.size()) {
				argPos = 0; // print usage
				break;
			}
			clazz = args[argPos];
		}
		else {
			argPos = 0; // print usage
			break;
		}
		argPos++;
	}
	if (argPos == 0 || args.size() < argPos + 1 || args.size() > argPos + 2)
		return "usage: 'read [-v] [-f] [-m seconds] [-c class] name [field]'";

	if (args.size() == argPos + 2)
		maxAge = 0; // force refresh to filter single field

	time_t now;
	time(&now);

	Message* updateMessage = NULL;
	if (maxAge > 0 && verbose == false) {
		updateMessage = m_messages->find(clazz, args[argPos], false, true);

		if (updateMessage != NULL && updateMessage->getLastUpdateTime() + maxAge > now)
			return updateMessage->getLastValue(); // TODO switch from last value to last master/slave to support verbose cached/polled values as well
		// else: check poll data or read directly from bus
	}

	Message* message = m_messages->find(clazz, args[argPos], false);

	if (message == NULL) {
		if (updateMessage != NULL)
			return "no data stored";
		else
			return "message not defined";
	}
	if (maxAge > 0 && message->getPollPriority() > 0
			&& message->getLastUpdateTime() + maxAge > now) {
		// get poll data
		return message->getLastValue();
	} // else: read directly from bus

	SymbolString master;
	istringstream input;
	result_t ret = message->prepareMaster(m_address, master, input);
	if (ret != RESULT_OK) {
		logError(lf_main, "prepare read: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	logInfo(lf_main, "read cmd: %s", master.getDataStr().c_str());

	// send message
	SymbolString slave;
	ret = m_busHandler->sendAndWait(master, slave);

	ostringstream result;
	if (ret == RESULT_OK) {
		if (args.size() == argPos + 2)
			ret = message->decode(pt_slaveData, slave, result, false, verbose, args[argPos + 1].c_str());
		else
			ret = message->decode(pt_slaveData, slave, result, false, verbose); // decode data
	}
	if (ret != RESULT_OK) {
		logError(lf_main, "read: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	return result.str();
}

string MainLoop::executeWrite(vector<string> &args)
{
	size_t argPos = 1;
	if (args.size() > argPos && args[argPos] == "-h") {
		argPos++;

		if (args.size() < argPos + 1)
			return "usage: 'write -h ZZPBSBNNDx'";

		ostringstream msg;
		msg << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_address) << setw(0);
		while (argPos < args.size()) {
			if ((args[argPos].length() % 2) != 0) {
				return "invalid hex string";
			}
			msg << args[argPos++];
		}

		SymbolString master(msg.str());
		if (isValidAddress(master[1]) == false)
			return "invalid destination";

		logNotice(lf_main, "write hex cmd: %s", master.getDataStr().c_str());

		// send message
		SymbolString slave;
		result_t ret = m_busHandler->sendAndWait(master, slave);

		if (ret == RESULT_OK) {
			if (master[1] == BROADCAST || isMaster(master[1]))
				return "done";
			return slave.getDataStr();
		}
		logError(lf_main, "write hex: %s", getResultCode(ret));
		return getResultCode(ret);
	}

	if (args.size() != argPos + 3)
		return "usage: 'write class name value[;value]*' or 'write -h ZZPBSBNNDx'";

	Message* message = m_messages->find(args[argPos], args[argPos + 1], true);

	if (message == NULL)
		return "message not defined";

	SymbolString master;
	istringstream input(args[argPos + 2]);
	result_t ret = message->prepareMaster(m_address, master, input);
	if (ret != RESULT_OK) {
		logError(lf_main, "prepare write: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	logInfo(lf_main, "write cmd: %s", master.getDataStr().c_str());

	// send message
	SymbolString slave;
	ret = m_busHandler->sendAndWait(master, slave);

	ostringstream result;
	if (ret == RESULT_OK) {
		if (master[1] == BROADCAST || isMaster(master[1]))
			return "done";

		ret = message->decode(pt_slaveData, slave, result); // decode data
		if (ret == RESULT_OK && result.str().empty() == true)
			return "done";
	}
	if (ret != RESULT_OK) {
		logError(lf_main, "write: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	return result.str();
}

string MainLoop::executeFind(vector<string> &args)
{
	size_t argPos = 1;
	bool verbose = false, withRead = true, withWrite = true, withPassive = true, first = true, onlyWithData = false;
	string clazz;
	while (args.size() > argPos && args[argPos][0] == '-') {
		if (args[argPos] == "-v")
			verbose = true;
		else if (args[argPos] == "-r") {
			if (first == true) {
				first = false;
				withWrite = withPassive = false;
			}
			withRead = true;
		}
		else if (args[argPos] == "-w") {
			if (first == true) {
				first = false;
				withRead = withPassive = false;
			}
			withWrite = true;
		}
		else if (args[argPos] == "-p") {
			if (first == true) {
				first = false;
				withRead = withWrite = false;
			}
			withPassive = true;
		}
		else if (args[argPos] == "-d") {
			onlyWithData = true;
		}
		else if (args[argPos] == "-c") {
			argPos++;
			if (argPos >= args.size()) {
				argPos = 0; // print usage
				break;
			}
			clazz = args[argPos];
		}
		else {
			argPos = 0; // print usage
			break;
		}
		argPos++;
	}
	if (argPos == 0 || args.size() < argPos || args.size() > argPos + 1)
		return "usage: 'find [-v] [-r] [-w] [-p] [-d] [-c class] [name]'";

	deque<Message*> messages;
	if (args.size() == argPos)
		messages = m_messages->findAll(clazz, "", -1, false, withRead, withWrite, withPassive);
	else
		messages = m_messages->findAll(clazz, args[argPos], -1, false, withRead, withWrite, withPassive);

	bool found = false;
	ostringstream result;
	char str[34];
	for (deque<Message*>::iterator it = messages.begin(); it < messages.end();) {
		Message* message = *it++;
		unsigned char dstAddress = message->getDstAddress();
		if (dstAddress == SYN)
			continue;
		time_t lastup = message->getLastUpdateTime();
		if (onlyWithData == true && lastup == 0)
			continue;
		if (found == true)
			result << endl;
		result << message->getClass() << " " << message->getName() << " = ";
		if (lastup == 0)
			result << "no data stored";
		else
			result << message->getLastValue();
		if (verbose == true) {
			if (lastup == 0)
				sprintf(str, "ZZ=%02x", dstAddress);
			else {
				struct tm* td = localtime(&lastup);
				sprintf(str, "ZZ=%02x, lastup=%04d-%02d-%02d %02d:%02d:%02d",
					dstAddress, td->tm_year+1900, td->tm_mon+1, td->tm_mday,
					td->tm_hour, td->tm_min, td->tm_sec);
			}
			result << " [" << str << "]";
		}
		found = true;
	}
	if (found == false)
		return "no message found";

	return result.str();
}

string MainLoop::executeListen(vector<string> &args, bool& listening)
{
	if (args.size() <= 1) {
		if (listening == true)
			return "listen continued";

		listening = true;
		return "listen started";
	}

	if (args.size() != 2 || args[1] != "stop")
		return "usage: 'listen [stop]'";

	listening = false;
	return "listen stopped";
}

string MainLoop::executeScan(vector<string> &args)
{
	if (args.size() <= 1) {
		result_t result = m_busHandler->startScan();
		if (result == RESULT_OK)
			return "scan initiated";

		logError(lf_main, "scan: %s", getResultCode(result));
		return getResultCode(result);
	}

	if (strcasecmp(args[1].c_str(), "FULL") == 0) {
		result_t result = m_busHandler->startScan(true);
		if (result == RESULT_OK)
			return "done";

		logError(lf_main, "full scan: %s", getResultCode(result));
		return getResultCode(result);
	}

	if (strcasecmp(args[1].c_str(), "RESULT") == 0) {
		ostringstream result;
		m_busHandler->formatScanResult(result);
		return result.str();
	}

	return "usage: 'scan'\n"
		   "       'scan full'\n"
		   "       'scan result'";
}

string MainLoop::executeLog(vector<string> &args)
{
	bool result;
	if (args.size() == 3 && strcasecmp(args[1].c_str(), "AREAS") == 0)
		result = setLogFacilities(args[2].c_str());
	else if (args.size() == 3 && strcasecmp(args[1].c_str(), "LEVEL") == 0)
		result = setLogLevel(args[2].c_str());
	else
		return "usage: 'log areas area,area,..' (area: main|network|bus|update|all)\n"
			   "       'log level level'        (level: error|notice|info|debug)";

	if (result == true)
		return "done";

	return "invalid area/level";
}

string MainLoop::executeRaw(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: 'raw'";

	bool enabled = !m_port->getLogRaw();
	m_port->setLogRaw(enabled);

	return enabled ? "raw output enabled" : "raw output disabled";
}

string MainLoop::executeDump(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: 'dump'";

	bool enabled = !m_port->getDumpRaw();
	m_port->setDumpRaw(enabled);

	return enabled ? "dump enabled" : "dump disabled";
}

string MainLoop::executeReload(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: 'reload'";

	// reload commands
	result_t result = loadConfigFiles(m_templates, m_messages);
	if (result == RESULT_OK)
		return "done";

	return getResultCode(result);
}

string MainLoop::executeHelp()
{
	return "commands:\n"
		   " read      - read ebus values            'read [-v] [-f] [-m seconds] [-c class] name [field]'\n"
		   " write     - write ebus values           'write class name value[;value]*' or 'write -h ZZPBSBNNDx'\n"
		   " find      - find ebus values            'find [-v] [-r] [-w] [-p] [-d] [-c class] [name]'\n"
		   " listen    - listen for updates          'listen [stop]'\n"
		   " scan      - scan ebus known addresses   'scan'\n"
		   "           - scan ebus all addresses     'scan full'\n"
		   "           - show scan results           'scan result'\n"
		   " log       - change log areas            'log areas area,area,..' (area: main|network|bus|update|all)\n"
		   "           - change log level            'log level level'        (level: error|notice|info|debug)\n"
		   " raw       - toggle log raw data         'raw'\n"
		   " dump      - toggle dump state           'dump'\n"
		   " reload    - reload ebus configuration   'reload'\n"
		   " stop      - stop daemon                 'stop'\n"
		   " quit      - close connection            'quit'\n"
		   " help      - print this page             'help'";
}

string MainLoop::getUpdates(time_t since, time_t until)
{
	ostringstream result;

	deque<Message*> messages;
	messages = m_messages->findAll("", "", -1, false, true, true, true);

	for (deque<Message*>::iterator it = messages.begin(); it < messages.end();) {
		Message* message = *it++;
		unsigned char dstAddress = message->getDstAddress();
		if (dstAddress == SYN)
			continue;
		time_t lastchg = message->getLastChangeTime();
		if (lastchg < since || lastchg >= until)
			continue;
		result << message->getClass() << " " << message->getName() << " = ";
		result << message->getLastValue() << endl;
	}

	return result.str();
}

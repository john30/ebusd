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

MainLoop::MainLoop(const struct options opt, Device *device, DataFieldTemplates* templates, MessageMap* messages)
	: m_device(device), m_templates(templates), m_messages(messages), m_address(opt.address)
{
	// setup Device
	m_device->setLogRaw(opt.logRaw);
	m_device->setDumpRawFile(opt.dumpFile);
	m_device->setDumpRawMaxSize(opt.dumpSize);
	m_device->setDumpRaw(opt.dump);

	// open Device
	result_t result = m_device->open();
	if (result != RESULT_OK)
		logError(lf_bus, "unable to open %s: %s", m_device->getName(), getResultCode(result));
	else if (!m_device->isValid())
		logError(lf_bus, "device %s not available", m_device->getName());

	// create BusHandler
	m_busHandler = new BusHandler(m_device, m_messages,
			m_address, opt.answer,
			opt.acquireRetries, opt.sendRetries,
			opt.acquireTimeout, opt.receiveTimeout,
			opt.masterCount, opt.generateSyn,
			opt.pollInterval);
	m_busHandler->start("bushandler");

	// create network
	m_network = new Network(opt.localOnly, opt.port, opt.httpPort, &m_netQueue);
	m_network->start("network");
}

MainLoop::~MainLoop()
{
	if (m_network != NULL) {
		delete m_network;
		m_network = NULL;
	}
	if (m_busHandler != NULL) {
		delete m_busHandler;
		m_busHandler = NULL;
	}
	if (m_device != NULL) {
		delete m_device;
		m_device = NULL;
	}

	m_messages->clear();
	m_templates->clear();
}

void MainLoop::run()
{
	bool running = true;

	while (running) {
		string result;

		// pick the next message to handle
		NetMessage* message = m_netQueue.remove();
		string request = message->getRequest();

		time_t since, until;
		time(&until);
		bool listening = message->isListening(&since);
		if (!listening)
			since = until;

		bool connected = true;
		if (request.length() > 0) {
			logDebug(lf_main, ">>> %s", request.c_str());
			result = decodeMessage(request, message->isHttp(), connected, listening, running);

			logDebug(lf_main, "<<< %s", result.c_str());
			if (result.length() == 0)
				result = "\n";
			else
				result += "\n\n";
		}
		if (listening) {
			result += getUpdates(since, until);
		}

		// send result to client
		message->setResult(result, listening, until, !connected);
	}
}

string MainLoop::decodeMessage(const string& data, const bool isHttp, bool& connected, bool& listening, bool& running)
{
	ostringstream result;

	// prepare data
	string token, previous;
	istringstream stream(data);
	vector<string> args;
	bool escaped = false;

	char delim = ' ';
	while (getline(stream, token, delim) != 0) {
		if (isHttp && delim == '/' && token.length() > 0 && token[0] == '?') {
			token.erase(0, 1);
			delim = '&';
		}
		if (escaped) {
			args.pop_back();
			if (token.length() > 0 && token[token.length()-1] == '"') {
				token.erase(token.length() - 1, 1);
				escaped = false;
			}
			token = previous + " " + token;
		}
		else if (token.length() == 0) // allow multiple space chars for a single delimiter
			continue;
		else if (token[0] == '"') {
			token.erase(0, 1);
			if (token.length() > 0 && token[token.length()-1] == '"')
				token.erase(token.length() - 1, 1);
			else
				escaped = true;
		}
		args.push_back(token);
		previous = token;
		if (isHttp)
			delim = '/';
	}

	if (args.size() == 0)
		return executeHelp();

	const char* str = args[0].c_str();
	if (isHttp) {
		if (strcmp(str, "GET") == 0)
			return executeGet(args);
		return "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
	}

	if (args.size() == 2) {
		// check for "CMD -h"
		if (strcasecmp(args[1].c_str(), "-h") == 0 || strcasecmp(args[1].c_str(), "-?") == 0 || strcasecmp(args[1].c_str(), "--help") == 0)
			args.clear(); // empty args is used as command help indicator
		else if (strcasecmp(args[0].c_str(), "H") == 0 || strcasecmp(args[0].c_str(), "HELP") == 0) { // check for "HELP CMD"
			str = args[1].c_str();
			args.clear(); // empty args is used as command help indicator
		}
	}
	if (strcasecmp(str, "R") == 0 || strcasecmp(str, "READ") == 0)
		return executeRead(args);
	if (strcasecmp(str, "W") == 0 || strcasecmp(str, "WRITE") == 0)
		return executeWrite(args);
	if (strcasecmp(str, "F") == 0 || strcasecmp(str, "FIND") == 0)
		return executeFind(args);
	if (strcasecmp(str, "L") == 0 || strcasecmp(str, "LISTEN") == 0)
		return executeListen(args, listening);
	if (strcasecmp(str, "S") == 0 || strcasecmp(str, "STATE") == 0)
		return executeState(args);
	if (strcasecmp(str, "G") == 0 || strcasecmp(str, "GRAB") == 0)
		return executeGrab(args);
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
	if (strcasecmp(str, "STOP") == 0)
		return executeStop(args, running);
	if (strcasecmp(str, "Q") == 0 || strcasecmp(str, "QUIT") == 0)
		return executeQuit(args, connected);
	if (strcasecmp(str, "H") == 0 || strcasecmp(str, "HELP") == 0)
		return executeHelp();
	return "ERR: command not found";
}

string MainLoop::executeRead(vector<string> &args)
{
	size_t argPos = 1;
	time_t maxAge = 5*60;
	bool verbose = false;
	string circuit;
	unsigned char dstAddress = SYN;
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
			circuit = args[argPos];
		}
		else if (args[argPos] == "-d") {
			argPos++;
			if (argPos >= args.size()) {
				argPos = 0; // print usage
				break;
			}
			result_t ret;
			dstAddress = (unsigned char)parseInt(args[argPos].c_str(), 16, 0, 0xff, ret);
			if (ret != RESULT_OK || !isValidAddress(dstAddress) || isMaster(dstAddress))
				return getResultCode(RESULT_ERR_INVALID_ADDR);
		} else {
			argPos = 0; // print usage
			break;
		}
		argPos++;
	}
	if (argPos == 0 || args.size() < argPos + 1 || args.size() > argPos + 2)
		return "usage: read [-v] [-f] [-m SECONDS] [-d ZZ] [-c CIRCUIT] NAME [FIELD[.N]]\n"
			   " Read value(s).\n"
			   "  -v          be verbose (include field names, units, and comments)\n"
			   "  -f          force reading from the bus (same as '-m 0')\n"
			   "  -m SECONDS  only return cached value if age is less than SECONDS [300]\n"
			   "  -d ZZ       override destination address ZZ\n"
			   "  -c CIRCUIT  limit to messages of CIRCUIT\n"
			   "  NAME        the NAME of the message to send\n"
			   "  FIELD       only retrieve the field named FIELD\n"
			   "  N           only retrieve the N'th field named FIELD (0-based)";

	string fieldName;
	signed char fieldIndex = -2;
	if (args.size() == argPos + 2) {
		fieldName = args[argPos + 1];
		fieldIndex = -1;
		size_t pos = fieldName.find_last_of('.');
		if (pos != string::npos) {
			result_t result = RESULT_OK;
			fieldIndex = (char)parseInt(fieldName.substr(pos+1).c_str(), 10, 0, MAX_POS, result);
			if (result == RESULT_OK)
				fieldName = fieldName.substr(0, pos);
		}
	}

	time_t now;
	time(&now);

	ostringstream result;
	Message* message = m_messages->find(circuit, args[argPos], false);

	if (dstAddress==SYN && maxAge > 0) {
		Message* cacheMessage = m_messages->find(circuit, args[argPos], false, true);
		bool hasCache = cacheMessage != NULL;
		if (!hasCache || (message != NULL && message->getLastUpdateTime() > cacheMessage->getLastUpdateTime()))
			cacheMessage = message; // message is newer/better

		if (cacheMessage != NULL && (cacheMessage->getLastUpdateTime() + maxAge > now || (cacheMessage->isPassive() && cacheMessage->getLastUpdateTime() != 0))) {
			result_t ret = cacheMessage->decodeLastData(result, false, verbose?df_verbose:df_standard, fieldIndex==-2 ? NULL : fieldName.c_str(), fieldIndex);
			if (ret != RESULT_OK)
				return getResultCode(ret);

			return result.str();
		}

		if (message == NULL && hasCache)
			return "ERR: no data stored";
		// else: read directly from bus
	}

	if (message == NULL)
		return getResultCode(RESULT_ERR_NOTFOUND);
	if (message->getDstAddress()==SYN && dstAddress==SYN)
		return getResultCode(RESULT_ERR_INVALID_ADDR);

	// read directly from bus
	SymbolString master(true);
	istringstream input;
	result_t ret = message->prepareMaster(m_address, master, input, UI_FIELD_SEPARATOR, dstAddress);
	if (ret != RESULT_OK) {
		logError(lf_main, "prepare read: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	logInfo(lf_main, "read cmd: %s", master.getDataStr().c_str());

	// send message
	SymbolString slave(false);
	ret = m_busHandler->sendAndWait(master, slave);

	if (ret == RESULT_OK) {
		ret = message->decode(pt_slaveData, slave, result, false, verbose?df_verbose:df_standard, fieldIndex==-2 ? NULL : fieldName.c_str(), fieldIndex);
	}
	if (ret < RESULT_OK) {
		logError(lf_main, "read: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	if (ret > RESULT_OK)
		return getResultCode(ret);
	return result.str();
}

string MainLoop::executeWrite(vector<string> &args)
{
	size_t argPos = 1;
	while (args.size() > argPos && args[argPos] == "-h") {
		argPos++;

		if (args.size() < argPos + 1) {
			argPos = 0;
			break;
		}
		ostringstream msg;
		while (argPos < args.size()) {
			if ((args[argPos].length() % 2) != 0) {
				return getResultCode(RESULT_ERR_INVALID_NUM);
			}
			msg << args[argPos++];
		}
		if (msg.str().size() < 4*2) // at least ZZ, PB, SB, NN
			return getResultCode(RESULT_ERR_INVALID_ARG);
		result_t ret;
		unsigned int length = parseInt(msg.str().substr(3*2, 2).c_str(), 16, 0, MAX_POS, ret);
		if (ret == RESULT_OK && (4+length)*2 != msg.str().size())
			return getResultCode(RESULT_ERR_INVALID_ARG);

		SymbolString master(true);
		ret = master.push_back(m_address, false);
		if (ret == RESULT_OK)
			ret = master.parseHex(msg.str());
		if (ret == RESULT_OK && !isValidAddress(master[1]))
			ret = RESULT_ERR_INVALID_ADDR;
		if (ret != RESULT_OK)
			return getResultCode(ret);

		logNotice(lf_main, "write hex cmd: %s", master.getDataStr().c_str());

		// send message
		SymbolString slave(false);
		ret = m_busHandler->sendAndWait(master, slave);

		if (ret == RESULT_OK) {
			if (master[1] == BROADCAST || isMaster(master[1]))
				return getResultCode(RESULT_OK);
			return slave.getDataStr();
		}
		logError(lf_main, "write hex: %s", getResultCode(ret));
		return getResultCode(ret);
	}

	if (args.size() > argPos && args[argPos] == "-c") {
		argPos++;
	}
	if (argPos == 0 || (args.size() != argPos + 3 && args.size() != argPos + 2))
		return "usage: write [-c] CIRCUIT NAME [VALUE[;VALUE]*]\n"
			   "  or:  write -h ZZPBSBNNDx\n"
			   " Write value(s) or hex message.\n"
			   "  CIRCUIT  the CIRCUIT of the message to send\n"
			   "  NAME     the NAME of the message to send\n"
			   "  VALUE    a single field VALUE\n"
			   "  -h       directly write hex message:\n"
			   "    ZZ     destination address\n"
			   "    PB SB  primary/secondary command byte\n"
			   "    NN     number of following data bytes\n"
			   "    Dx     the data byte(s) to send";

	Message* message = m_messages->find(args[argPos], args[argPos + 1], true);

	if (message == NULL)
		return getResultCode(RESULT_ERR_NOTFOUND);

	SymbolString master(true);
	istringstream input(args.size() == argPos + 2 ? "" : args[argPos + 2]); // allow missing values
	result_t ret = message->prepareMaster(m_address, master, input);
	if (ret != RESULT_OK) {
		logError(lf_main, "prepare write: %s", getResultCode(ret));
		return getResultCode(ret);
	}
	logInfo(lf_main, "write cmd: %s", master.getDataStr().c_str());

	// send message
	SymbolString slave(false);
	ret = m_busHandler->sendAndWait(master, slave);

	ostringstream result;
	if (ret == RESULT_OK) {
		if (master[1] == BROADCAST || isMaster(master[1]))
			return getResultCode(RESULT_OK);

		ret = message->decode(pt_slaveData, slave, result); // decode data
		if (ret >= RESULT_OK && result.str().empty())
			return getResultCode(RESULT_OK);
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
	bool verbose = false, configFormat = false, withRead = true, withWrite = false, withPassive = true, first = true, onlyWithData = false;
	string circuit;
	short pb = -1;
	while (args.size() > argPos && args[argPos][0] == '-') {
		if (args[argPos] == "-v")
			verbose = true;
		else if (args[argPos] == "-f")
			configFormat = true;
		else if (args[argPos] == "-r") {
			if (first) {
				first = false;
				withWrite = withPassive = false;
			}
			withRead = true;
		}
		else if (args[argPos] == "-w") {
			if (first) {
				first = false;
				withRead = withPassive = false;
			}
			withWrite = true;
		}
		else if (args[argPos] == "-p") {
			if (first) {
				first = false;
				withRead = withWrite = false;
			}
			withPassive = true;
		}
		else if (args[argPos] == "-d")
			onlyWithData = true;
		else if (args[argPos] == "-i") {
			argPos++;
			if (argPos >= args.size()) {
				argPos = 0; // print usage
				break;
			}
			const char* str = args[argPos].c_str();
			result_t result = RESULT_OK;
			if (strncasecmp(str, "0x", 2) == 0)
				pb = (short)parseInt(str+2, 16, 0, 0xff, result); // hexadecimal
			else
				pb = (short)parseInt(str, 10, 0, 0xff, result); // decimal
			if (result != RESULT_OK) {
				return getResultCode(result);
			}
		}
		else if (args[argPos] == "-c") {
			argPos++;
			if (argPos >= args.size()) {
				argPos = 0; // print usage
				break;
			}
			circuit = args[argPos];
		}
		else {
			argPos = 0; // print usage
			break;
		}
		argPos++;
	}
	if (argPos == 0 || args.size() < argPos || args.size() > argPos + 1)
		return "usage: find [-v] [-r] [-w] [-p] [-d] [-i PB] [-f] [-c CIRCUIT] [NAME]\n"
			   " Find message(s).\n"
			   "  -v         be verbose (append destination address and update time)\n"
			   "  -r         limit to active read messages (default: read + passive)\n"
			   "  -w         limit to active write messages (default: read + passive)\n"
			   "  -p         limit to passive messages (default: read + passive)\n"
			   "  -d         only include messages with actual data\n"
			   "  -i PB      limit to messages with primary command byte PB ('0xPB' for hex)\n"
			   "  -f         list messages in CSV configuration file format\n"
			   "  -c CIRCUIT limit to messages of CIRCUIT (or a part thereof)\n"
			   "  NAME       the NAME of the messages to find (or a part thereof)";

	deque<Message*> messages;
	if (args.size() == argPos)
		messages = m_messages->findAll(circuit, "", pb, false, withRead, withWrite, withPassive);
	else
		messages = m_messages->findAll(circuit, args[argPos], pb, false, withRead, withWrite, withPassive);

	bool found = false;
	ostringstream result;
	char str[31];
	for (deque<Message*>::iterator it = messages.begin(); it < messages.end();) {
		Message* message = *it++;
		time_t lastup = message->getLastUpdateTime();
		if (onlyWithData && lastup == 0)
			continue;
		if (configFormat) {
			if (found)
				result << endl;
			message->dump(result);
		} else {
			unsigned char dstAddress = message->getDstAddress();
			if (dstAddress == SYN)
				continue;
			if (found)
				result << endl;
			result << message->getCircuit() << " " << message->getName() << " = ";
			if (lastup == 0)
				result << "no data stored";
			else
				message->decodeLastData(result, verbose?df_verbose:df_standard);
			if (verbose) {
				if (lastup == 0)
					sprintf(str, "%02x", dstAddress);
				else {
					struct tm* td = localtime(&lastup);
					sprintf(str, "%02x, lastup=%04d-%02d-%02d %02d:%02d:%02d",
						dstAddress, td->tm_year+1900, td->tm_mon+1, td->tm_mday,
						td->tm_hour, td->tm_min, td->tm_sec);
				}
				result << " [ZZ=" << str;
				if (message->isPassive())
					result << ", passive";
				else
					result << ", active";

				if (message->isWrite())
					result << " write]";
				else
					result << " read]";
			}
		}
		found = true;
	}
	if (!found)
		return getResultCode(RESULT_ERR_NOTFOUND);

	return result.str();
}

string MainLoop::executeListen(vector<string> &args, bool& listening)
{
	if (args.size() == 1) {
		if (listening)
			return "listen continued";

		listening = true;
		return "listen started";
	}

	if (args.size() != 2 || args[1] != "stop")
		return "usage: listen [stop]\n"
			   " Listen for updates or stop it.";

	listening = false;
	return "listen stopped";
}

string MainLoop::executeState(vector<string> &args)
{
	if (args.size() == 0)
		return "usage: state\n"
			   " Report bus state.";

	if (m_busHandler->hasSignal()) {
		ostringstream result;
		result << "signal acquired, "
			   << static_cast<unsigned>(m_busHandler->getSymbolRate()) << " symbols/sec ("
			   << static_cast<unsigned>(m_busHandler->getMaxSymbolRate()) << " max), "
			   << static_cast<unsigned>(m_busHandler->getMasterCount()) << " masters";
		return result.str();
	}
	return "no signal";
}

string MainLoop::executeGrab(vector<string> &args)
{
	if (args.size() == 1) {
		m_busHandler->enableGrab();

		return getResultCode(RESULT_OK);
	}

	if (args.size() == 2 && strcasecmp(args[1].c_str(), "STOP") == 0) {
		m_busHandler->enableGrab(false);

		return getResultCode(RESULT_OK);
	}

	if (args.size() == 2 && strcasecmp(args[1].c_str(), "RESULT") == 0) {
		ostringstream result;
		m_busHandler->formatGrabResult(result);
		return result.str();
	}

	return "usage: grab [stop]\n"
		   "  or:  grab result\n"
		   " Grab unknown messages or stop it, or report the seen unknown messages.";
}

string MainLoop::executeScan(vector<string> &args)
{
	if (args.size() == 1) {
		result_t result = m_busHandler->startScan();
		if (result != RESULT_OK)
			logError(lf_main, "scan: %s", getResultCode(result));

		return getResultCode(result);
	}

	if (args.size() == 2 && strcasecmp(args[1].c_str(), "FULL") == 0) {
		result_t result = m_busHandler->startScan(true);
		if (result != RESULT_OK)
			logError(lf_main, "full scan: %s", getResultCode(result));

		return getResultCode(result);
	}

	if (args.size() == 2 && strcasecmp(args[1].c_str(), "RESULT") == 0) {
		ostringstream result;
		m_busHandler->formatScanResult(result);
		return result.str();
	}

	return "usage: scan [full]\n"
		   "  or:  scan result\n"
		   " Scan seen or all slaves, or report scan result.";
}

string MainLoop::executeLog(vector<string> &args)
{
	bool result;
	if ((args.size() == 3 || args.size() == 2) && strcasecmp(args[1].c_str(), "AREAS") == 0)
		result = setLogFacilities(args.size() == 3 ? args[2].c_str() : "");
	else if (args.size() == 3 && strcasecmp(args[1].c_str(), "LEVEL") == 0)
		result = setLogLevel(args[2].c_str());
	else
		return "usage: log areas AREA[,AREA]*\n"
			   "  or:  log level LEVEL\n"
			   " Set log area(s) or log level.\n"
			   "  AREA   the log area to include (main|network|bus|update|all)\n"
			   "  LEVEL  the log level to set (error|notice|info|debug)";

	if (result)
		return getResultCode(RESULT_OK);

	return getResultCode(RESULT_ERR_INVALID_ARG);
}

string MainLoop::executeRaw(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: raw\n"
			   " Toggle logging raw bytes.";

	bool enabled = !m_device->getLogRaw();
	m_device->setLogRaw(enabled);

	return enabled ? "raw output enabled" : "raw output disabled";
}

string MainLoop::executeDump(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: dump\n"
			   " Toggle dumping raw bytes.";

	bool enabled = !m_device->getDumpRaw();
	m_device->setDumpRaw(enabled);

	return enabled ? "dump enabled" : "dump disabled";
}

string MainLoop::executeReload(vector<string> &args)
{
	if (args.size() != 1)
		return "usage: reload\n"
			   " Reload CSV config files.";

	result_t result = loadConfigFiles(m_templates, m_messages);

	return getResultCode(result);
}

string MainLoop::executeStop(vector<string> &args, bool& running)
{
	if (args.size() == 1) {
		running = false;
		return "daemon stopped";
	}

	return "usage: stop\n"
		   " Stop the daemon.";
}

string MainLoop::executeQuit(vector<string> &args, bool& connected)
{
	if (args.size() == 1) {
		connected = false;
		return "connection closed";
	}

	return "usage: quit\n"
		   " Close client connection.";
}

string MainLoop::executeHelp()
{
	return "usage:\n"
		   " read|r   Read value(s):         read [-v] [-f] [-m SECONDS] [-d ZZ] [-c CIRCUIT] NAME [FIELD[.N]]\n"
		   " write|w  Write value(s):        write [-c] CIRCUIT NAME [VALUE[;VALUE]*]\n"
		   "          Write hex message:     write -h ZZPBSBNNDx'\n"
		   " find|f   Find message(s):       find [-v] [-r] [-w] [-p] [-d] [-i PB] [-f] [-c CIRCUIT] [NAME]\n"
		   " listen|l Listen for updates:    listen [stop]\n"
		   " state|s  Report bus state\n"
		   " grab|g   Grab unknown messages: grab [stop]\n"
		   "          Report the messages:   grab result\n"
		   " scan     Scan slaves:           scan [full]\n"
		   "          Report scan result:    scan result\n"
		   " log      Set log area(s):       log areas AREA[,AREA*]\n"
		   "                                   AREA: main|network|bus|update|all\n"
		   "          Set log level:         log level LEVEL\n"
		   "                                   LEVEL: error|notice|info|debug\n"
		   " raw      Toggle logging raw bytes\n"
		   " dump     Toggle dumping raw bytes\n"
		   " reload   Reload CSV config files\n"
		   " stop     Stop the daemon\n"
		   " quit|q   Close connection\n"
		   " help|h   Print help             help [COMMAND]";
}

string MainLoop::executeGet(vector<string> &args)
{
	size_t argPos = 1;
	bool onlyWithData = false;

	deque<Message*> messages;
	if (args.size() >= argPos+2)
		messages = m_messages->findAll(args[argPos], args[argPos+1], -1, false, true, false, true);
	else if (args.size() == argPos+1)
		messages = m_messages->findAll(args[argPos], "", -1, false, true, false, true);
	else
		messages = m_messages->findAll("", "", -1, false, true, false, true);

	bool first = true;
	ostringstream result;
	result << "{";
	string lastCircuit = "";
	result_t ret = RESULT_OK;
	for (deque<Message*>::iterator it = messages.begin(); it < messages.end();) {
		Message* message = *it++;
		time_t lastup = message->getLastUpdateTime();
		if (onlyWithData && lastup == 0)
			continue;
		unsigned char dstAddress = message->getDstAddress();
		if (dstAddress == SYN)
			continue;
		if (message->getCircuit() != lastCircuit) {
			if (lastCircuit.length() > 0)
				result << "\n },";
			lastCircuit = message->getCircuit();
			result << "\n \"" << lastCircuit << "\": {";
			first = true;
		}
		if (first)
			first = false;
		else
			result << ",";
		result << "\n  \"" << message->getName() << "\": {";
		result << "\n   \"lastup\": " << setw(0) << dec << static_cast<unsigned>(lastup);
		if (lastup != 0) {
			result << ",\n   \"zz\": \"" << setfill('0') << setw(2) << hex << static_cast<unsigned>(dstAddress) << "\"";
			result << ",\n   \"fields\": [";
			ret = message->decodeLastData(result, false, df_json);
			if (ret < RESULT_OK)
				break;
			result << "\n   ]";
		}
		result << ",\n   \"passive\": " << (message->isPassive() ? "true" : "false");
		result << ",\n   \"write\": " << (message->isWrite() ? "true" : "false");
		result << "\n  }";
	}
	if (lastCircuit.length() > 0)
		result << "\n }";
	result << "\n}";

	if (ret == RESULT_OK ) {
		string str = result.str();
		result.str("");
		result.clear();
		result << "HTTP/1.0 200 OK\r\n";
		result << "Content-Type: application/json;charset=utf-8\r\n";
		result << "Access-Control-Allow-Origin: *\r\n";
		result << "Content-Length: " << setw(0) << dec << static_cast<unsigned>(str.length()) << "\r\n";
		result << "\r\n";
		result << str;
	} else {
		result.str("");
		result.clear();
		result << "HTTP/1.0 ";
		switch (ret) {
		case RESULT_ERR_NOTFOUND:
			result << "404 Not Found";
			break;
		default:
			result << "500 Internal Server Error";
			break;
		}
		result << "\r\n";
		result << "\r\n";
	}
	return result.str();
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
		result << message->getCircuit() << " " << message->getName() << " = ";
		message->decodeLastData(result);
		result << endl;
	}

	return result.str();
}

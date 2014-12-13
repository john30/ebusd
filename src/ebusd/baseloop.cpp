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

#include "baseloop.h"
#include "logger.h"
#include "appl.h"
#include <dirent.h>
#include <iomanip>

using namespace std;

extern Logger& L;
extern Appl& A;

BaseLoop::BaseLoop()
{
	// create commands DB
	m_templates = new DataFieldTemplates();
	m_messages = new MessageMap();

	string confdir = A.getOptVal<const char*>("ebusconfdir");
	L.log(bas, trace, "ebus configuration dir: %s", confdir.c_str());
	result_t result = m_templates->readFromFile(confdir+"/_types.csv");
	if (result == RESULT_OK)
		L.log(bas, trace, "read templates");
	else
		L.log(bas, error, "error reading templates: %s", getResultCode(result));
	result = readConfigFiles(confdir, ".csv");
	if (result == RESULT_OK)
		L.log(bas, trace, "read config files");
	else
		L.log(bas, error, "error reading config files: %s", getResultCode(result));

	L.log(bas, event, "message DB: %d ", m_messages->size());
	L.log(bas, event, "updates DB: %d ", m_messages->size(true));
	L.log(bas, event, "polling DB: %d ", m_messages->sizePoll());

	m_ownAddress = A.getOptVal<int>("address") & 0xff;
	const bool answer = A.getOptVal<bool>("answer");

	const bool logRaw = A.getOptVal<bool>("lograwdata");

	const bool dumpRaw = A.getOptVal<bool>("dump");
	const char* dumpRawFile = A.getOptVal<const char*>("dumpfile");
	const long dumpRawMaxSize = A.getOptVal<long>("dumpsize");

	const unsigned int busLostRetries = A.getOptVal<unsigned int>("lockretries");
	const unsigned int failedSendRetries = A.getOptVal<unsigned int>("sendretries");
	const unsigned int busAcquireWaitTime = A.getOptVal<unsigned int>("acquiretimeout");
	const unsigned int slaveRecvTimeout = A.getOptVal<unsigned int>("recvtimeout");
	const unsigned int lockCount = A.getOptVal<unsigned int>("lockcounter");
	int pollInterval = A.getOptVal<unsigned int>("pollinterval");
	if (pollInterval <= 0) {
		m_pollActive = false;
		pollInterval = 0;
	} else
		m_pollActive = true;

	// create Port
	m_port = new Port(A.getOptVal<const char*>("device"), A.getOptVal<bool>("nodevicecheck"), logRaw, &BaseLoop::logRaw, dumpRaw, dumpRawFile, dumpRawMaxSize);
	m_port->open();

	if (m_port->isOpen() == false)
		L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

	// create BusHandler
	m_busHandler = new BusHandler(m_port, m_messages,
			m_ownAddress, answer,
			busLostRetries, failedSendRetries,
			busAcquireWaitTime, slaveRecvTimeout,
			lockCount, pollInterval);
	m_busHandler->start("bushandler");

	// create network
	m_network = new Network(A.getOptVal<bool>("localhost"), &m_netQueue);
	m_network->start("network");
}

BaseLoop::~BaseLoop()
{
	if (m_network != NULL)
		delete m_network;

	if (m_busHandler != NULL) {
		m_busHandler->stop();
		m_busHandler->join();
		delete m_busHandler;
	}

	if (m_port != NULL)
		delete m_port;

	if (m_messages != NULL)
		delete m_messages;

	if (m_templates != NULL)
		delete m_templates;
}

result_t BaseLoop::readConfigFiles(const string path, const string extension)
{
	DIR* dir = opendir(path.c_str());

	if (dir == NULL)
		return RESULT_ERR_NOTFOUND;

	dirent* d = readdir(dir);

	while (d != NULL) {
		if (d->d_type == DT_DIR) {
			string fn = d->d_name;

			if (fn != "." && fn != "..") {
				const string p = path + "/" + d->d_name;
				result_t result = readConfigFiles(p, extension);
				if (result != RESULT_OK)
					return result;
			}
		} else if (d->d_type == DT_REG) {
			string fn = d->d_name;

			if (fn.find(extension, (fn.length() - extension.length())) != string::npos
				&& fn != "_types" + extension) {
				const string p = path + "/" + d->d_name;
				result_t result = m_messages->readFromFile(p, m_templates);
				if (result != RESULT_OK)
					return result;
			}
		}

		d = readdir(dir);
	}
	closedir(dir);

	return RESULT_OK;
};

void BaseLoop::start()
{
	for (;;) {
		string result;

		// recv new message from client
		NetMessage* message = m_netQueue.remove();
		string data = message->getData();

		data.erase(remove(data.begin(), data.end(), '\r'), data.end());
		data.erase(remove(data.begin(), data.end(), '\n'), data.end());

		L.log(bas, event, ">>> %s", data.c_str());

		// decode message
		if (strcasecmp(data.c_str(), "STOP") != 0)
			result = decodeMessage(data);
		else
			result = "done";

		L.log(bas, event, "<<< %s", result.c_str());

		// send result to client
		result += '\n';
		message->setResult(result);
		message->sendSignal();

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0)
			return;
	}
}

void BaseLoop::logRaw(const unsigned char byte, bool received) {
	if (received == true) {
		L.log(bus, event, "<%02x", byte);
	} else {
		L.log(bus, event, ">%02x", byte);
	}
}

string BaseLoop::decodeMessage(const string& data)
{
	ostringstream result;

	// prepare data
	string token;
	istringstream stream(data);
	vector<string> args;

	while (getline(stream, token, ' ') != 0)
		args.push_back(token);

	if (args.size() == 0)
		return "command missing";

	size_t argPos = 1;
	bool force = false;

	switch (getCase(args[0])) {
	case ct_invalid:
		result << "command not found";
		break;

	case ct_read: {
		if (args.size() > argPos && args[argPos] == "-f") {
			force = true;
			argPos++;
		}
		if (args.size() < argPos + 1 || args.size() > argPos + 3) {
			result << "usage: 'read [-f] [class] cmd' or 'read [-f] class cmd sub'";
			break;
		}

		Message* updateMessage = NULL;
		if (force == false) {
			if (args.size() == argPos + 1)
				updateMessage = m_messages->find("", args[argPos], false, true);
			else
				updateMessage = m_messages->find(args[argPos], args[argPos + 1], false, true);

			if (updateMessage != NULL) {
				token = updateMessage->getLastValue();
				if (token.empty() == false) {
					result << token;
					break;
				} // else try to read directly from bus
			}
		}

		Message* message;
		if (args.size() == argPos + 1)
			message = m_messages->find("", args[argPos], false);
		else
			message = m_messages->find(args[argPos], args[argPos + 1], false);

		if (message != NULL) {

			if (m_pollActive == true && message->getPollPriority() > 0) {
				// get polldata
				token = message->getLastValue();
				if (token.empty() == false) {
					result << token;
					break;
				} // else: read directly from bus
			}

			SymbolString master;
			istringstream input;
			result_t ret = message->prepareMaster(m_ownAddress, master, input);
			if (ret != RESULT_OK) {
				L.log(bas, error, " prepare read: %s", getResultCode(ret));
				result << getResultCode(ret);
				break;
			}
			L.log(bas, event, " read msg: %s", master.getDataStr().c_str());

			// send message
			SymbolString slave;
			ret = m_busHandler->sendAndWait(master, slave);

			if (ret == RESULT_OK) {
				// TODO reduce to requested variable only
				ret = message->decode(pt_slaveData, slave, result); // decode data
			}
			if (ret != RESULT_OK) {
				L.log(bas, error, " read: %s", getResultCode(ret));
				result << getResultCode(ret);
			}

		} else if (updateMessage != NULL) {
			result << "no data stored";
		} else {
			result << "message not defined";
		}
		break;
	}
	case ct_write: {
		if (args.size() != argPos + 3) {
			result << "usage: 'write class cmd value[;value]*'";
			break;
		}

		Message* message = m_messages->find(args[argPos], args[argPos + 1], true);

		if (message != NULL) {
			SymbolString master;
			istringstream input(args[argPos + 2]);
			result_t ret = message->prepareMaster(m_ownAddress, master, input);
			if (ret != RESULT_OK) {
				L.log(bas, error, " prepare write: %s", getResultCode(ret));
				result << getResultCode(ret);
				break;
			}
			L.log(bas, event, " write msg: %s", master.getDataStr().c_str());

			// send message
			SymbolString slave;
			ret = m_busHandler->sendAndWait(master, slave);

			if (ret == RESULT_OK) {
				if (master[1] == BROADCAST || isMaster(master[1]))
					result << "done";
				else {
					ret = message->decode(pt_slaveData, slave, result); // decode data
					if (ret == RESULT_OK && result.str().empty() == true)
						result << "done";
				}
			}
			if (ret != RESULT_OK) {
				L.log(bas, error, " write: %s", getResultCode(ret));
				result << getResultCode(ret);
			}

		} else {
			result << "message not defined";
		}
		break;
	}
	case ct_hex: {
		if (args.size() < argPos + 1) {
			result << "usage: 'hex value' (value: ZZPBSBNNDx)";
			break;
		}

		ostringstream msg;
		msg << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_ownAddress) << setw(0);
		while (argPos < args.size()) {
			if ((args[argPos].length() % 2) != 0) {
				result << "invalid hex string";
				msg.str("");
				break;
			}
			msg << args[argPos++];
		}
		if (msg.str().length() == 0)
			break;

		SymbolString master(msg.str());
		L.log(bas, event, " hex msg: %s", master.getDataStr().c_str());

		// send message
		SymbolString slave;
		result_t ret = m_busHandler->sendAndWait(master, slave);

		if (ret == RESULT_OK) {
			if (master[1] == BROADCAST || isMaster(master[1]))
				result << "done";
			else
				result << slave.getDataStr();
		}
		if (ret != RESULT_OK) {
			L.log(bas, error, " hex: %s", getResultCode(ret));
			result << getResultCode(ret);
		}
		break;
	}
	case ct_scan: {
		if (args.size() == argPos) {
			result_t ret = m_busHandler->startScan();
			if (ret != RESULT_OK) {
				L.log(bas, error, " scan: %s", getResultCode(ret));
				result << getResultCode(ret);
			}
			else
				result << "scan initiated";
			break;
		}

		if (strcasecmp(args[argPos].c_str(), "FULL") == 0) {
			result_t ret = m_busHandler->startScan(true);
			if (ret != RESULT_OK) {
				L.log(bas, error, " full scan: %s", getResultCode(ret));
				result << getResultCode(ret);
			}
			else
				result << "done";
			break;
		}

		if (strcasecmp(args[argPos].c_str(), "RESULT") == 0) {
			m_busHandler->formatScanResult(result);
			break;
		}

		result << "usage: 'scan'" << endl
		       << "       'scan full'" << endl
		       << "       'scan result'";
		break;
	}
	case ct_log: {
		if (args.size() != 3 ) {
			result << "usage: 'log areas area,area,..' (areas: bas|net|bus|upd|all)" << endl
			       << "       'log level level'        (level: error|event|trace|debug)";
			break;
		}

		// TODO: check for possible areas and level
		if (strcasecmp(args[argPos].c_str(), "AREAS") == 0) {
			L.getSink(0)->setAreas(calcAreas(args[argPos + 1]));
			result << "done";
			break;
		}

		if (strcasecmp(args[argPos].c_str(), "LEVEL") == 0) {
			L.getSink(0)->setLevel(calcLevel(args[argPos + 1]));
			result << "done";
			break;
		}

		result << "usage: 'log areas area,area,..' (areas: bas|net|bus|upd|all)" << endl
		       << "       'log level level'        (level: error|event|trace|debug)";
		break;
	}
	case ct_raw: {
		if (args.size() != 1) {
			result << "usage: 'raw'";
			break;
		}

		bool enabled = !m_port->getLogRaw();
		m_port->setLogRaw(enabled);
		result << (enabled ? "raw output enabled" : "raw output disabled");
		break;
	}
	case ct_dump: {
		if (args.size() != 1) {
			result << "usage: 'dump'";
			break;
		}

		bool enabled = !m_port->getDumpRaw();
		m_port->setDumpRaw(enabled);
		result << (enabled ? "dump enabled" : "dump disabled");
		break;
	}
	/*case ct_reload:
		if (cmd.size() != 1) {
			result << "usage: 'reload'";
			break;
		}

		{
			// create commands DB
			Commands* commands = ConfigCommands(A.getOptVal<const char*>("ebusconfdir"), ft_csv).getCommands();
			L.log(bas, trace, "ebus configuration dir: %s", A.getOptVal<const char*>("ebusconfdir"));
			L.log(bas, event, "commands DB: %d ", m_commands->sizeCmdDB());
			L.log(bas, event, "   cycle DB: %d ", m_commands->sizeCycDB());
			L.log(bas, event, " polling DB: %d ", m_commands->sizePollDB());

			delete m_commands;
			m_commands = commands;
			m_busloop->reload(m_commands);

			result << "done";
			break;
		}*/

	case ct_help:
		result << "commands:" << endl
		       << " read      - read ebus values            'read [-f] [class] cmd [sub]'" << endl
		       << " write     - write ebus values           'write class cmd value[;value]*'" << endl
		       << " hex       - send given hex value        'hex type value'         (value: ZZPBSBNNDx)" << endl << endl
		       << " scan      - scan ebus kown addresses    'scan'" << endl
		       << "           - scan ebus all addresses     'scan full'" << endl
		       << "           - show results                'scan result'" << endl << endl
 		       << " log       - change log areas            'log areas area,area,..' (areas: bas|net|bus|upd|all)" << endl
		       << "           - change log level            'log level level'        (level: error|event|trace|debug)" << endl << endl
		       << " raw       - toggle log raw data         'raw'" << endl
		       << " dump      - toggle dump state           'dump'" << endl << endl
		       << " reload    - reload ebus configuration   'reload'" << endl << endl
		       << " stop      - stop daemon                 'stop'" << endl
		       << " quit      - close connection            'quit'" << endl << endl
		       << " help      - print this page             'help'";
		break;

	default:
		break;
	}

	return result.str();
}


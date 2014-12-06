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

	/*L.log(bas, event, "commands DB: %d ", m_commands->sizeCmdDB());
	L.log(bas, event, "   cycle DB: %d ", m_commands->sizeCycDB());
	L.log(bas, event, " polling DB: %d ", m_commands->sizePollDB());*/

	m_ownAddress = A.getOptVal<int>("address") & 0xff;
	bool answer = A.getOptVal<bool>("answer");

	const bool logRaw = A.getOptVal<bool>("lograwdata");

	const bool dumpRaw = A.getOptVal<bool>("dump");
	const char* dumpRawFile = A.getOptVal<const char*>("dumpfile");
	const long dumpRawMaxSize = A.getOptVal<long>("dumpsize");

	// create Port
	m_port = new Port(A.getOptVal<const char*>("device"), A.getOptVal<bool>("nodevicecheck"), logRaw, &BaseLoop::logRaw, dumpRaw, dumpRawFile, dumpRawMaxSize);
	m_port->open();

	if (m_port->isOpen() == false)
		L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

	// create BusHandler
	m_busHandler = new BusHandler(m_port, m_messages, answer ? m_ownAddress : SYN, answer ? (m_ownAddress+5)&0xff : SYN);
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

void BaseLoop::logRaw(const unsigned char byte) {
	L.log(bus, event, "%02x", byte);
}

string BaseLoop::decodeMessage(const string& data)
{
	ostringstream result;
	string cycdata, polldata;

	// prepare data
	string token;
	istringstream stream(data);
	vector<string> cmd;

	while (getline(stream, token, ' ') != 0)
		cmd.push_back(token);

	if (cmd.size() == 0)
		return "command missing";

	switch (getCase(cmd[0])) {
	case ct_invalid:
		result << "command not found";
		break;

	case ct_get:
		if (cmd.size() < 2 || cmd.size() > 4) {
			result << "usage: 'get [class] cmd' or 'get class cmd sub'";
			break;
		}

		{
			Message* message;
			if (cmd.size() == 2)
				message = m_messages->find("", cmd[1], false);
			else
				message = m_messages->find(cmd[1], cmd[2], false);

			if (message != NULL) {

				/*if (message->getPollPriority() > 0)
					// get polldata
					polldata = m_commands->getPollData(index);
					if (polldata != "") {
						// decode data
						Command* command = new Command(index, (*m_commands)[index], polldata);

						// return result
						result << command->calcResult(cmd);

						delete command;
					} else {
						result << "no data stored";
					}

					break;
				}*/

				SymbolString master;
				istringstream input;
				message->prepareMaster(m_ownAddress, master, input);
				L.log(bas, trace, " msg: %s", master.getDataStr().c_str());

				// send message
				SymbolString slave;
				result_t ret = m_busHandler->sendAndWait(master, slave);

				if (ret == RESULT_OK)
					// decode data
					ret = message->decode(master, slave, result);

				if (ret != RESULT_OK) {
					L.log(bas, error, " %s", getResultCode(ret));
					result << getResultCode(ret);
				}
				else
					result << result.str(); // TODO reduce to requested variable only

			} else {
				result << "ebus command not found";
			}
		}
		break;

	/*case ct_set:
		if (cmd.size() != 4) {
			result << "usage: 'set class cmd value'";
			break;
		}

		index = m_commands->findCommand(data.substr(0, data.find(cmd[3])-1));

		if (index >= 0) {

			string busCommand(A.getOptVal<const char*>("address"));
			busCommand += m_commands->getBusCommand(index);

			// encode data
			Command* command = new Command(index, (*m_commands)[index], cmd[3]);
			string value = command->calcData();
			if (value[0] != '-') {
				busCommand += value;
			} else {
				L.log(bas, error, " %s", value.c_str());
				delete command;
				break;
			}

			transform(busCommand.begin(), busCommand.end(), busCommand.begin(), ::tolower);

			BusMessage* message = new BusMessage(busCommand, false, false);
			L.log(bas, event, " msg: %s", busCommand.c_str());
			// send message
			m_busloop->addMessage(message);
			message->waitSignal();

			if (!message->isErrorResult()) {
				// decode result
				if (message->getType()==broadcast)
					result << "done";
				else if (message->getMessageStr().substr(message->getMessageStr().length()-8) == "00000000") // TODO use getResult()
					result << "done";
				else
					result << "error";

			} else {
				L.log(bas, error, " %s", message->getResultCodeCStr());
				result << message->getResultCodeCStr();
			}

			delete message;
			delete command;

		} else {
			result << "ebus command not found";
		}

		break;*/

	/*case ct_cyc:
		if (cmd.size() < 3 || cmd.size() > 4) {
			result << "usage: 'cyc class cmd (sub)'";
			break;
		}

		index = m_commands->findCommand(data);

		if (index >= 0) {
			// get cycdata
			cycdata = m_commands->getCycData(index);
			if (cycdata != "") {
				// decode data
				Command* command = new Command(index, (*m_commands)[index], cycdata);

				// return result
				result << command->calcResult(cmd);

				delete command;
			} else {
				result << "no data stored";
			}
		} else {
			result << "ebus command not found";
		}

		break;*/

	case ct_hex:
		if (cmd.size() != 2) {
			result << "usage: 'hex value' (value: ZZPBSBNNDx)";
			break;
		}

		{
			cmd[1].erase(remove_if(cmd[1].begin(), cmd[1].end(), ::isspace), cmd[1].end());
			string src;
			ostringstream msg;
			msg << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_ownAddress);
			msg << cmd[1];
			SymbolString master(cmd[1]);
			L.log(bas, trace, " msg: %s", master.getDataStr().c_str());

			// send message
			SymbolString slave;
			result_t ret = m_busHandler->sendAndWait(master, slave);

			if (ret == RESULT_OK)
				// decode data
				result << slave.getDataStr(); // TODO find suitable message?, message->decode(master, slave, result);

			if (ret != RESULT_OK) {
				L.log(bas, error, " %s", getResultCode(ret));
				result << getResultCode(ret);
			}
			else
				result << result.str(); // TODO reduce to requested variable only
		}

		break;

	/*case ct_scan:
		if (cmd.size() == 1) {
			m_busloop->scan();
			result << "done";
			break;
		}

		if (strcasecmp(cmd[1].c_str(), "FULL") == 0) {
			m_busloop->scan(true);
			result << "done";
			break;
		}

		if (strcasecmp(cmd[1].c_str(), "RESULT") == 0) {
			// TODO format scan results
			for (size_t i = 0; i < m_commands->sizeScanDB(); i++)
				result << m_commands->getScanData(i) << endl;

			break;
		}

		result << "usage: 'scan'" << endl
		       << "       'scan full'" << endl
		       << "       'scan result'";
		break;*/

	case ct_log:
		if (cmd.size() != 3 ) {
			result << "usage: 'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << endl
			       << "       'log level level'        (level: error|event|trace|debug)";
			break;
		}

		// TODO: check for possible areas and level
		if (strcasecmp(cmd[1].c_str(), "AREAS") == 0) {
			L.getSink(0)->setAreas(calcAreas(cmd[2]));
			result << "done";
			break;
		}

		if (strcasecmp(cmd[1].c_str(), "LEVEL") == 0) {
			L.getSink(0)->setLevel(calcLevel(cmd[2]));
			result << "done";
			break;
		}

		result << "usage: 'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << endl
		       << "       'log level level'        (level: error|event|trace|debug)";

		break;

	case ct_raw:
		if (cmd.size() != 1) {
			result << "usage: 'raw'";
			break;
		}

		m_port->setLogRaw(!m_port->getLogRaw());
		result << "done";
		break;

	case ct_dump:
		if (cmd.size() != 1) {
			result << "usage: 'dump'";
			break;
		}

		m_port->setDumpRaw(!m_port->getDumpRaw());
		result << "done";
		break;

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
		       << " get       - fetch ebus data             'get class cmd (sub)'" << endl
		       << " set       - set ebus values             'set class cmd value'" << endl
		       << " cyc       - fetch cycle data            'cyc class cmd (sub)'" << endl
		       << " hex       - send given hex value        'hex type value'         (value: ZZPBSBNNDx)" << endl << endl
		       << " scan      - scan ebus kown addresses    'scan'" << endl
		       << "           - scan ebus all addresses     'scan full'" << endl
		       << "           - show results                'scan result'" << endl << endl
 		       << " log       - change log areas            'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << endl
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


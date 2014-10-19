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

#include "baseloop.h"
#include "logger.h"
#include "appl.h"

extern LogInstance& L;
extern Appl& A;

BaseLoop::BaseLoop()
{
	// create Commands DB
	m_commands = ConfigCommands(A.getParam<const char*>("p_ebusconfdir"), CSV).getCommands();
	L.log(bas, trace, "ebus configuration dir: %s", A.getParam<const char*>("p_ebusconfdir"));
	L.log(bas, event, "commands DB: %d ", m_commands->sizeCmdDB());
	L.log(bas, event, "   cycle DB: %d ", m_commands->sizeCycDB());
	L.log(bas, event, " polling DB: %d ", m_commands->sizePolDB());

	// create EBusLoop
	m_ebusloop = new EBusLoop(m_commands);
	m_ebusloop->start("ebusloop");

	// create Network
	m_network = new Network(A.getParam<bool>("p_localhost"));
	m_network->addQueue(&m_queue);
	m_network->start("network");
}

BaseLoop::~BaseLoop()
{
	// free Network
	if (m_network != NULL)
		delete m_network;

	// free EBusLoop
	if (m_ebusloop != NULL) {
		m_ebusloop->stop();
		m_ebusloop->join();
		delete m_ebusloop;
	}

	// free Commands DB
	if (m_commands != NULL)
		delete m_commands;
}

void BaseLoop::start()
{
	for (;;) {
		std::string result;

		// recv new message from client
		Message* message = m_queue.remove();
		std::string data = message->getData();

		data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());
		data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());

		L.log(bas, event, ">>> %s", data.c_str());

		// decode message
		if (strcasecmp(data.c_str(), "STOP") != 0)
			result = decodeMessage(data);
		else
			result = "done";

		L.log(bas, event, "<<< %s", result.c_str());

		// send result to client
		result += '\n';
		Connection* connection = static_cast<Connection*>(message->getSource());
		connection->addResult(Message(result));

		delete message;

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0)
			return;
	}
}

std::string BaseLoop::decodeMessage(const std::string& data)
{
	std::ostringstream result;
	std::string cycdata, polldata;
	int index;

	// prepare data
	std::string token;
	std::istringstream stream(data);
	std::vector<std::string> cmd;

	while (std::getline(stream, token, ' ') != 0)
		cmd.push_back(token);

	if (cmd.size() == 0)
		return "command missing";

	switch (getCase(cmd[0])) {
	case notfound:
		result << "command not found";
		break;

	case get:
		if (cmd.size() < 3 || cmd.size() > 4) {
			result << "usage: 'get class cmd (sub)'";
			break;
		}

		index = m_commands->findCommand(data);

		if (index >= 0) {

			// polling data
			if (strcasecmp(m_commands->getCmdType(index).c_str(), "P") == 0) {
				// get polldata
				polldata = m_commands->getPolData(index);
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
			}

			std::string ebusCommand(A.getParam<const char*>("p_address"));
			ebusCommand += m_commands->getEbusCommand(index);
			std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

			BusCommand* busCommand = new BusCommand(ebusCommand);
			L.log(bas, trace, " msg: %s", ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(busCommand);
			busCommand = m_ebusloop->getBusCommand();

			if (!busCommand->isErrorResult()) {
				// decode data
				Command* command = new Command(index, (*m_commands)[index], busCommand->getResult());

				// return result
				result << command->calcResult(cmd);

				delete command;
			} else {
				L.log(bas, error, " %s", busCommand->getResultCodeCStr());
				result << busCommand->getResultCodeCStr();
			}

			delete busCommand;

		} else {
			result << "ebus command not found";
		}

		break;

	case set:
		if (cmd.size() != 4) {
			result << "usage: 'set class cmd value'";
			break;
		}

		index = m_commands->findCommand(data.substr(0, data.find(cmd[3])-1));

		if (index >= 0) {

			std::string ebusCommand(A.getParam<const char*>("p_address"));
			ebusCommand += m_commands->getEbusCommand(index);

			// encode data
			Command* command = new Command(index, (*m_commands)[index], cmd[3]);
			std::string value = command->calcData();
			if (value[0] != '-') {
				ebusCommand += value;
			} else {
				L.log(bas, error, " %s", value.c_str());
				delete command;
				break;
			}

			std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

			BusCommand* busCommand = new BusCommand(ebusCommand);
			L.log(bas, event, " msg: %s", ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(busCommand);
			busCommand = m_ebusloop->getBusCommand();

			if (!busCommand->isErrorResult()) {
				// decode result
				if (busCommand->getType()==broadcast)
					result << "done";
				else if (busCommand->getResult().substr(busCommand->getResult().length()-8) == "00000000")
					result << "done";
				else
					result << "error";

			} else {
				L.log(bas, error, " %s", busCommand->getResultCodeCStr());
				result << busCommand->getResultCodeCStr();
			}

			delete busCommand;
			delete command;

		} else {
			result << "ebus command not found";
		}

		break;

	case cyc:
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

		break;

	case hex:
		if (cmd.size() != 2) {
			result << "usage: 'hex value' (value: ZZPBSBNNDx)";
			break;
		}

		{
			std::string ebusCommand(A.getParam<const char*>("p_address"));
			cmd[1].erase(std::remove_if(cmd[1].begin(), cmd[1].end(), isspace), cmd[1].end());
			ebusCommand += cmd[1];
			std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

			BusCommand* busCommand = new BusCommand(ebusCommand);
			L.log(bas, trace, " msg: %s", ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(busCommand);
			busCommand = m_ebusloop->getBusCommand();

			if (busCommand->isErrorResult()) {
				L.log(bas, error, " %s", busCommand->getResultCodeCStr());
				result << busCommand->getResultCodeCStr();
			} else {
				result << busCommand->getResult();
			}

			delete busCommand;
		}

		break;

	case dump:
		if (cmd.size() != 2) {
			result << "usage: 'dump state' (state: on|off)";
			break;
		}

		if (strcasecmp(cmd[1].c_str(), "ON") == 0)  m_ebusloop->dump(true);
		if (strcasecmp(cmd[1].c_str(), "OFF") == 0) m_ebusloop->dump(false);
		result << "done";
		break;

	case log:
		if (cmd.size () != 3 ) {
			result << "usage: 'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << std::endl
			       << "       'log level level'        (level: error|event|trace|debug)";
			break;
		}

		// ToDo: check for possible areas and level
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

		result << "usage: 'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << std::endl
		       << "       'log level level'        (level: error|event|trace|debug)";

		break;

	case config:
		if (cmd.size() != 2) {
			result << "usage: 'config list'"  << std::endl
			       << "       'config reload'";
			break;
		}

		// ToDo: ...
		if (strcasecmp(cmd[1].c_str(), "LIST") == 0) {
			result << "not implemented yet";
			break;
		}

		if (strcasecmp(cmd[1].c_str(), "RELOAD") == 0) {
			// create Commands DB
			Commands* commands = ConfigCommands(A.getParam<const char*>("p_ebusconfdir"), CSV).getCommands();
			L.log(bas, trace, "ebus configuration dir: %s", A.getParam<const char*>("p_ebusconfdir"));
			L.log(bas, event, "commands DB: %d ", m_commands->sizeCmdDB());
			L.log(bas, event, "   cycle DB: %d ", m_commands->sizeCycDB());
			L.log(bas, event, " polling DB: %d ", m_commands->sizePolDB());

			delete m_commands;
			m_commands = commands;
			m_ebusloop->newCommands(m_commands);

			result << "done";
			break;
		}

		result << "usage: 'config list'"  << std::endl
		       << "       'config reload'";
		break;

	case help:
		result << "commands:" << std::endl
		       << " get       - fetch ebus data           'get class cmd (sub)'" << std::endl
		       << " set       - set ebus values           'set class cmd value'" << std::endl
		       << " cyc       - fetch cycle data          'cyc class cmd (sub)'" << std::endl
		       << " hex       - send given hex value      'hex type value'         (value: ZZPBSBNNDx)" << std::endl << std::endl
		       << " dump      - change dump state         'dump state'             (state: on|off)" << std::endl << std::endl
		       << " log       - change log areas          'log areas area,area,..' (areas: bas|net|bus|cyc|all)" << std::endl
		       << "           - change log level          'log level level'        (level: error|event|trace|debug)" << std::endl << std::endl
		       << " config    - list ebus configuration   'config list'" << std::endl
		       << "           - reload ebus configuration 'config reload'" << std::endl << std::endl
		       << " stop      - stop daemon" << std::endl
		       << " quit      - close connection" << std::endl << std::endl
		       << " help      - print this page";
		break;

	default:
		break;
	}

	return result.str();
}


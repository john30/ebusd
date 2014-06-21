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
#include "network.h"
#include <algorithm>
#include <sstream>
#include <unistd.h>

extern LogInstance& L;
extern Appl& A;

void BaseLoop::start()
{
	for (;;) {
		// recv new message from client
		Message* message = m_queue.remove();
		std::string data = message->getData();

		data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());
		data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());

		L.log(bas, event, ">>> %s", data.c_str());

		// decode message
		std::string result(decodeMessage(data));

		L.log(bas, event, "<<< %s", result.c_str());

		// send result to client
		result += '\n';
		Connection* connection = static_cast<Connection*>(message->getSource());
		connection->addResult(Message(result));

		delete message;
	}
}

std::string BaseLoop::decodeMessage(const std::string& data)
{
	std::ostringstream result;
	std::string cycdata;
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

			std::string type = m_commands->getType(index);
			std::string ebusCommand(A.getParam<const char*>("p_address"));
			ebusCommand += m_commands->getEbusCommand(index);
			std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

			L.log(bas, trace, " type: %s msg: %s", type.c_str(), ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(new BusCommand(type, ebusCommand));
			BusCommand* busCommand = m_ebusloop->getBusCommand();

			if (busCommand != NULL) {
				if (busCommand->getResult().c_str()[0] != '-') {
					// decode data
					Command* command = new Command(index, (*m_commands)[index], busCommand->getResult());

					// return result
					result << command->calcResult(cmd);

					delete command;
				} else {
					L.log(bas, error, " %s", busCommand->getResult().c_str());
					result << busCommand->getResult();
				}


				delete busCommand;
			} else {
				L.log(bas, error, " -7: receive timeout");
				result << "-7: receive timeout";
			}

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

			std::string type = m_commands->getType(index);
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

			L.log(bas, event, " type: %s msg: %s", type.c_str(), ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(new BusCommand(type, ebusCommand));
			BusCommand* busCommand = m_ebusloop->getBusCommand();

			if (busCommand != NULL) {
				if (busCommand->getResult().c_str()[0] != '-') {
					// decode result
					if (busCommand->getResult().substr(busCommand->getResult().length()-8) == "00000000")
						result << "done";
					else
						result << "error";

				} else {
					L.log(bas, error, " %s", busCommand->getResult().c_str());
					result << busCommand->getResult();
				}

				delete busCommand;
			} else {
				L.log(bas, error, " -7: receive timeout");
				result << "-7: receive timeout";
			}

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
			cycdata = m_cycdata->getData(index);
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
		if (cmd.size() != 3) {
			result << "usage: 'hex type value' (value: ZZPBSBNNDx)";
			break;
		}

		if ((strcasecmp(cmd[1].c_str(), "MS") == 0)
		||  (strcasecmp(cmd[1].c_str(), "MM") == 0)
		||  (strcasecmp(cmd[1].c_str(), "BC") == 0)) {

			std::string type = cmd[1];
			std::string ebusCommand(A.getParam<const char*>("p_address"));
			cmd[2].erase(std::remove_if(cmd[2].begin(), cmd[2].end(), isspace), cmd[2].end());
			ebusCommand += cmd[2];
			std::transform(ebusCommand.begin(), ebusCommand.end(), ebusCommand.begin(), tolower);

			L.log(bas, trace, " type: %s msg: %s", type.c_str(), ebusCommand.c_str());
			// send busCommand
			m_ebusloop->addBusCommand(new BusCommand(type, ebusCommand));
			BusCommand* busCommand = m_ebusloop->getBusCommand();

			if (busCommand != NULL) {
				if (busCommand->getResult().c_str()[0] == '-')
					L.log(bas, error, " %s", busCommand->getResult().c_str());

				result << busCommand->getResult();

				delete busCommand;
			} else {
				L.log(bas, error, " -7: receive timeout");
				result << "-7: receive timeout";
			}

		} else {
			result << "specified message type is incorrect";
		}

		break;

	case dump:
		if (cmd.size() != 2) {
			result << "usage: 'dump state' (state: on|off)";
			break;
		}

		if (cmd[1] == "on")  m_ebusloop->dump(true);
		if (cmd[1] == "off") m_ebusloop->dump(false);
		result << "done";
		break;

	case logarea:
		if (cmd.size() != 2) {
			result << "usage: 'logarea area,area,..' (area: bas|net|bus|cyc|all)";
			break;
		}

		L.getSink(0)->setAreas(calcArea(cmd[1]));
		result << "done";
		break;

	case loglevel:
		if (cmd.size() != 2) {
			result << "usage: 'loglevel level' (level: error|event|trace|debug)";
			break;
		}

		L.getSink(0)->setLevel(calcLevel(cmd[1]));
		result << "done";
		break;

	case help:
		result << "commands:" << std::endl
		       << " get       - fetch ebus data       'get class cmd (sub)'" << std::endl
		       << " set       - set ebus values       'set class cmd value'" << std::endl
		       << " cyc       - fetch cycle data      'cyc class cmd (sub)'" << std::endl
		       << " hex       - send given hex value  'hex type value' (value: ZZPBSBNNDx)" << std::endl
		       << " dump      - change dump state     'dump state' (state: on|off)" << std::endl
		       << " logarea   - change log area       'logarea area,area,..' (area: bas|net|bus|cyc|all)" << std::endl
		       << " loglevel  - change log level      'loglevel level' (level: error|event|trace|debug)" << std::endl
		       << " quit      - close connection" << std::endl
		       << " help      - print this page";
		break;

	default:
		break;
	}

	return result.str();
}


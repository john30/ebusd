/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
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
		connection->addResult(Message(result.c_str()));

		delete message;
	}
}

std::string BaseLoop::decodeMessage(const std::string& data)
{
	std::ostringstream result;
	std::string value;
	int index;
	BusCommand* busCommand;
	
	// prepare data
	std::string token;
	std::istringstream stream(data);
	std::vector<std::string> cmd;
	
	// split stream
	while (std::getline(stream, token, ' ') != 0)
		cmd.push_back(token);

	if (cmd.size() == 0)
		return "command missing";
	
	switch (getCase(cmd[0])) {
	case notfound:
		result << "command not found";
		break;
		
	case get:
	case set:
		if (cmd.size() < 3) {
			result << "format: type class cmd [sub]";
			break;
		}
		
		index = m_commands->findCommand(data);

		if (index >= 0) {

			std::string type = m_commands->getType(index);
			std::string cmd(A.getParam<const char*>("p_address"));
			cmd += m_commands->getEbusCommand(index);
			std::transform(cmd.begin(), cmd.end(), cmd.begin(), tolower);
			
			L.log(bas, trace, " type: %s msg: %s", type.c_str(), cmd.c_str());
			// send BusCommand
			m_ebusloop->addBusCommand(new BusCommand(type, cmd));
			busCommand = m_ebusloop->getBusCommand();
			
			// decode BusCommand
			result << busCommand->getResult().c_str();
			delete busCommand;

		} else {
			result << "ebus command not found";
		}
		
		break;
		
	case cyc:
		if (cmd.size() < 3) {
			result << "format: type class cmd [sub]";
			break;
		}
		
		index = m_commands->findCommand(data);
		
		if (index >= 0) {
			value = m_cycdata->getData(index);
			if (value != "") {
				// decode CYC Data
				result << value.c_str();
			} else {
				result << "no data stored";
			}
		} else {
			result << "ebus command not found";
		}
		
		break;
		
	case dump:
		if (cmd[1] == "on")  m_ebusloop->dump(true);
		if (cmd[1] == "off") m_ebusloop->dump(false);
		result << "done";
		break;

	case log:
		if (cmd[1] == "error") L.getSink(0)->setLevel(error);
		if (cmd[1] == "event") L.getSink(0)->setLevel(event);
		if (cmd[1] == "trace") L.getSink(0)->setLevel(trace);
		if (cmd[1] == "debug") L.getSink(0)->setLevel(debug);
		result << "done";
		break;
	
	default:
		break;
	}

	return result.str();
}


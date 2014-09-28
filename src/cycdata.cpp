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

#include "cycdata.h"
#include "logger.h"
#include <sstream>
#include <iomanip>

extern LogInstance& L;

CYCData::CYCData(EBusLoop* ebusloop, Commands* commands)
	: m_ebusloop(ebusloop), m_commands(commands), m_stop(false)
{
	for (size_t index = 0; index < commands->size(); index++) {
		if (strcasecmp((*m_commands)[index][0].c_str(),"cyc") == 0) {
			Command* cmd = new Command(index, (*m_commands)[index]);
			m_cycDB.insert(pair_t(index, cmd));
		}
	}

}

CYCData::~CYCData()
{
	for (mapCI_t iter = m_cycDB.begin(); iter != m_cycDB.end(); ++iter)
		delete iter->second;
}

void* CYCData::run()
{
	bool skipfirst = false;

	for (;;) {
		std::string data = m_ebusloop->getData();

		if (skipfirst == true) {
			L.log(cyc, trace, "%s", data.c_str());

			int index = findData(data);

			if (index >= 0) {
				std::string tmp;
				tmp += (*m_commands)[index][0];
				tmp += " ";
				tmp += (*m_commands)[index][1];
				tmp += " ";
				tmp += (*m_commands)[index][2];
				L.log(cyc, event, " [%d] %s", index, tmp.c_str());
				storeData(index, data);
			}
		}

		skipfirst = true;

		if (m_stop == true)
			return NULL;
	}

	return NULL;
}

std::string CYCData::getData(int index)
{
	mapCI_t iter = m_cycDB.find(index);
	if (iter != m_cycDB.end())
		return iter->second->getData();
	else
		return "";
}

int CYCData::findData(const std::string& data) const
{
	// no commands defined
	if (m_cycDB.size() == 0) {
		L.log(cyc, debug, " no commands defined");
		return -2;
	}

	// search skipped - string too short
	if (data.length() < 10) {
		L.log(cyc, debug, " search skipped - string too short");
		return -3;
	}

	// prepare string for searching command
	std::string search(data.substr(2, 8 + strtol(data.substr(8,2).c_str(), NULL, 16) * 2));

	std::size_t index;
	mapCI_t i = m_cycDB.begin();

	// walk through commands
	for (index = 0; i != m_cycDB.end(); i++, index++) {
		cmd_t cmd = i->second->getCommand();
		// prepare string for defined command
		std::string command(cmd[5]);
		command += cmd[6];
		std::stringstream sstr;
		sstr << std::setw(2) << std::hex << std::setfill('0') << cmd[7];
		command += sstr.str();
		command += cmd[8];

		// skip wrong search string length
		if (command.length() > search.length())
			continue;

		if (strcasecmp(command.c_str(), search.substr(0,command.length()).c_str()) == 0)
			return i->first;

	}

	// command not found
	L.log(cyc, debug, " command not found");
	return -1;
}

void CYCData::storeData(int index, std::string data)
{
	mapCI_t iter = m_cycDB.find(index);

	if (iter != m_cycDB.end()) {
		iter->second->setData(data);
		L.log(cyc, debug, " [%d] data saved", index);
	}
}


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

#include "cycdata.h"
#include "logger.h"

extern LogInstance& L;

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
			
			int index = m_commands->findData(data);
			
			if (index >= 0) {
				std::string tmp;
				tmp += (*m_commands)[index][0];
				tmp += " ";
				tmp += (*m_commands)[index][1];
				tmp += " ";
				tmp += (*m_commands)[index][2];
				L.log(cyc, trace, " [%d] %s", index, tmp.c_str());
				storeData(index, data);
			}
		}
		
		skipfirst = true;
		
		if (m_stop == true)
			return NULL;
	}

	return NULL;
}

void CYCData::storeData(int index, std::string data)
{
	mapCI_t iter = m_cycDB.find(index);

	if (iter != m_cycDB.end()) {
		iter->second->setData(data);
		L.log(cyc, debug, " [%d] %s -> replaced", index, data.c_str());
	} else {
		Command* cmd = new Command(index, (*m_commands)[index], data);
		m_cycDB.insert(pair_t(index, cmd));
		L.log(cyc, debug, " [%d] %s -> inserted", index, data.c_str());
	}

	L.log(cyc, debug, " cycDB entries: %d", m_cycDB.size());
}

std::string CYCData::getData(int index)
{
	mapCI_t iter = m_cycDB.find(index);
	if (iter != m_cycDB.end())
		return iter->second->getData();
	else
		return "";
}


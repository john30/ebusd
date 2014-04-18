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

#ifndef CYCDATA_H_
#define CYCDATA_H_

#include "libebus.h"
#include "ebusloop.h"
#include "thread.h"
#include <string>

using namespace libebus;

typedef std::map<int, Command*> map_t;
typedef map_t::const_iterator mapCI_t;
typedef std::pair<int, Command*> pair_t;

class CYCData : public Thread
{

public:
	CYCData(EBusLoop* ebusloop, Commands* commands);
	~CYCData();
	
	void* run();
	void stop() { m_stop = true; }

	std::string getData(int index);
	
private:
	EBusLoop* m_ebusloop;
	Commands* m_commands;
	map_t m_cycDB;
	bool m_stop;

	int findData(const std::string& data) const;
	void storeData(int index, std::string data);
	
};

#endif // CYCDATA_H_

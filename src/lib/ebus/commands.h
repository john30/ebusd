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

#ifndef LIBEBUS_COMMANDS_H_
#define LIBEBUS_COMMANDS_H_

#include "command.h"
#include <string>
#include <vector>
#include <map>

using namespace std;

typedef vector<cmd_t> cmdDB_t;
typedef cmdDB_t::const_iterator cmdDBCI_t;

typedef map<int, Command*> map_t;
typedef map_t::const_iterator mapCI_t;
typedef pair<int, Command*> pair_t;

class Commands
{

public:
	Commands() : m_pollIndex(-1) {}
	~Commands();

	void addCommand(const cmd_t& command);
	void printCommands() const;

	size_t sizeCmdDB() const { return m_cmdDB.size(); }
	size_t sizeCycDB() const { return m_cycDB.size(); }
	size_t sizePollDB() const { return m_pollDB.size(); }
	size_t sizeScanDB() const { return m_scanDB.size(); }

	cmd_t const& operator[](const size_t& index) const { return m_cmdDB[index]; }

	int findCommand(const string& data) const;

	string getCmdType(const int index) const { return string(m_cmdDB.at(index)[0]); }
	string getBusCommand(const int index) const;

	int storeCycData(const string& data) const;
	string getCycData(int index) const;

	int nextPollCommand();
	void storePollData(const string& data) const;
	string getPollData(const int index) const;

	void storeScanData(const string& data);
	string getScanData(const int index) const { return m_scanDB[index]; }

private:
	cmdDB_t m_cmdDB;
	map_t m_cycDB;
	map_t m_pollDB;
	size_t m_pollIndex;
	vector<string> m_scanDB;

	void printCommand(const cmd_t& command) const;

};

#endif // LIBEBUS_COMMANDS_H_


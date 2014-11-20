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

#ifndef LIBEBUS_COMMAND_H_
#define LIBEBUS_COMMAND_H_

#include <string>
#include <vector>

typedef std::vector<std::string> cmd_t;
typedef cmd_t::const_iterator cmdCI_t;

class Command
{

public:
	Command(int index, cmd_t command) : m_index(index), m_command(command) {}
	Command(int index, cmd_t command, std::string data)
		: m_index(index), m_command(command), m_data(data) {}

	cmd_t getCommand() const { return m_command; }
	void setData(const std::string& data) { m_data = data; }
	std::string getData() const { return m_data; }
	std::string calcData();

	std::string calcResult(const cmd_t& cmd);

private:
	int m_index;
	cmd_t m_command;
	std::string m_data;
	std::string m_result;
	std::string m_error;

	void calcSub(const std::string& part, const std::string& position,
		     const std::string& type, const std::string& factor);

	void decode(const std::string& data, const std::string& position,
		    const std::string& type, const std::string& factor);

	void encode(const std::string& data, const std::string& type,
		    const std::string& factor);

};

#endif // LIBEBUS_COMMAND_H_

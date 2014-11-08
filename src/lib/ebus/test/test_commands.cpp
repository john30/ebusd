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

#include "configfile.h"
#include "commands.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

using namespace libebus;

// will be part of cfg csv class
void readCSV(std::istream& is, Commands& commands){
	std::string line;

	// read lines
	while (std::getline(is, line) != 0) {
		std::vector<std::string> row;
		std::string column;
		int count;

		count = 0;

		std::istringstream stream(line);

		// walk through columns
		while (std::getline(stream, column, ';') != 0) {
			row.push_back(column);
			count++;
		}

		commands.addCommand(row);
	}
}

int main()
{
	Commands* commands = ConfigCommands("test", CSV).getCommands();
	std::cout << "Commands: " << commands->sizeCmdDB() << std::endl;

	//~ std::string data("g ci password pin1");
	std::string data("s vwxmk DesiredTemp");

	int index = commands->findCommand(data);
	std::cout << "found at index: " << index << std::endl;

	// prepare data
	std::string token;
	std::istringstream stream(data);
	std::vector<std::string> cmd;

	// split stream
	while (std::getline(stream, token, ' ') != 0)
		cmd.push_back(token);

	//~ Command* command = new Command(index, (*commands)[index], "ff15b509030d2c0035000401000000cf00");
	Command* command = new Command(index, (*commands)[index], "19.0");

	//~ std::string result = command->calcResult(cmd);
	std::string result = command->calcData();
	std::cout << "result: " << result << std::endl;

	delete command;

	return 0;
}



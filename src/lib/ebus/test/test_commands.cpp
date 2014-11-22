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

using namespace std;

// will be part of cfg csv class
void readCSV(istream& is, Commands& commands){
	string line;

	// read lines
	while (getline(is, line) != 0) {
		vector<string> row;
		string column;
		int count;

		count = 0;

		istringstream stream(line);

		// walk through columns
		while (getline(stream, column, ';') != 0) {
			row.push_back(column);
			count++;
		}

		commands.addCommand(row);
	}
}

int main()
{
	Commands* commands = ConfigCommands("test", ft_csv).getCommands();
	cout << "Commands: " << commands->sizeCmdDB() << endl;

	//~ string data("g ci password pin1");
	string data("s vwxmk DesiredTemp");

	int index = commands->findCommand(data);
	cout << "found at index: " << index << endl;

	// prepare data
	string token;
	istringstream stream(data);
	vector<string> cmd;

	// split stream
	while (getline(stream, token, ' ') != 0)
		cmd.push_back(token);

	//~ Command* command = new Command(index, (*commands)[index], "ff15b509030d2c0035000401000000cf00");
	Command* command = new Command(index, (*commands)[index], "19.0");

	//~ string result = command->calcResult(cmd);
	string result = command->calcData();
	cout << "result: " << result << endl;

	delete command;

	return 0;
}



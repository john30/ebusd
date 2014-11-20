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
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

int main() {

	std::string dir("test");
	ConfigCommands config(dir, ft_csv);

	Commands* commands = config.getCommands();

	std::cout << "size: " << commands->sizeCmdDB() << std::endl;

	commands->findCommand("g ci Password");

	std::cout << (*commands)[0][0] << std::endl;

	delete commands;

	return 0;
}



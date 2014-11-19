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
#include <sstream>
#include <fstream>
#include <dirent.h>

void ConfigFileCSV::parse(std::istream& is, Commands& commands)
{
	std::string line;

	// read lines
	while (std::getline(is, line) != 0) {
		cmd_t row;
		std::string column;

		std::istringstream isstr(line);

		// walk through columns
		while (std::getline(isstr, column, ';') != 0)
			row.push_back(column);

		// skip empty and commented rows
		if (row.empty() == true || row[0][0] == '#')
			continue;

		commands.addCommand(row);
	}
};


void ConfigFileXML::parse(std::istream& is, Commands& commands)
{
	;	// ToDo: Implementation for xml files
}



ConfigCommands::ConfigCommands(const std::string path, const FileType type)
{
	m_path = path;
	m_configfile = NULL;
	setType(type);
	addFiles(m_path, m_extension);
}

void ConfigCommands::setType(const FileType type)
{
	if (m_configfile != NULL)
		delete m_configfile;

	switch (type) {
	case CSV:
		m_configfile = new ConfigFileCSV();
		m_extension = "csv";
		break;
	case XML:
		m_configfile = new ConfigFileXML();
		m_extension = "xml";
		break;
	};
};

Commands* ConfigCommands::getCommands()
{
	Commands* commands = new Commands();
	std::vector<std::string>::const_iterator i = m_files.begin();

	for(; i != m_files.end(); i++) {
		std::fstream file((*i).c_str(), std::ios::in);

		if(file.is_open() == true) {
			m_configfile->parse(file, *commands);
			file.close();
		}
	}
	return commands;
};

void ConfigCommands::addFiles(const std::string path, const std::string extension)
{
        DIR* dir = opendir(path.c_str());

	if (dir == NULL)
		return;

        dirent* d = readdir(dir);

        while (d != NULL) {

                if (d->d_type == DT_DIR) {
                        std::string fn = d->d_name;

                        if (fn != "." && fn != "..") {
				const std::string p = path + "/" + d->d_name;
                                addFiles(p, extension);
			}

                } else if (d->d_type == DT_REG) {
                        std::string fn = d->d_name;

                        if (fn.find(extension, (fn.length() - extension.length())) != std::string::npos) {
				const std::string p = path + "/" + d->d_name;
                                m_files.push_back(p);
			}
                }

                d = readdir(dir);
        }

        closedir(dir);
};


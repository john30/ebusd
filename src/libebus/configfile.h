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

#ifndef LIBEBUS_CONFIGFILE_H_
#define LIBEBUS_CONFIGFILE_H_

#include "commands.h"
#include <string>
#include <vector>

namespace libebus
{

enum FileType { CSV, XML };

class ConfigFile
{

public:
	virtual ~ConfigFile() {}

	virtual void readFile(std::istream& is, Commands& commands) = 0;

};

class ConfigFileCSV : public ConfigFile
{

public:
	~ConfigFileCSV() {}

	void readFile(std::istream& is, Commands& commands);

};

class ConfigFileXML : public ConfigFile
{

public:
	~ConfigFileXML() {}

	void readFile(std::istream& is, Commands& commands);

};


class ConfigCommands
{

public:
	ConfigCommands(const std::string path, const FileType type);
	~ConfigCommands() { delete m_configfile; }

	void setType(const FileType type);
	Commands* getCommands();

private:
	ConfigFile* m_configfile;
	std::string m_path;
	std::string m_extension;
	std::vector<std::string> m_files;

	void addFiles(const std::string path, const std::string extension);

};


} //namespace

#endif // LIBEBUS_CONFIGFILE_H_

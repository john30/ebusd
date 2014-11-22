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

/** \file configfile.h */

/** available file endings / types. */
enum FileType {
	ft_csv  /*!< CSV */
};

/**
 * @brief base class for config files.
 */
class ConfigFile
{

public:
	/**
	 * @brief destructor.
	 */
	virtual ~ConfigFile() {}

	/**
	 * @brief read input stream and stored data into commands
	 * @param is open input stream for reading.
	 * @param commands object as datastore.
	 */
	virtual void parse(std::istream& is, Commands& commands) = 0;

};

/**
 * @brief derived class for CSV config files.
 */
class ConfigFileCSV : public ConfigFile
{

public:
	/**
	 * @brief destructor.
	 */
	~ConfigFileCSV() {}

	/**
	 * @brief read input stream and stored data into commands
	 * @param is open input stream for reading.
	 * @param commands object as datastore.
	 */
	void parse(std::istream& is, Commands& commands);

};

/**
 * @brief class to parse configuration files and store into commands instance.
 */
class ConfigCommands
{

public:
	/**
	 * @brief set file type and add recursive files from given path.
	 * @param path to configuration files.
	 * @param type to parse.
	 */
	ConfigCommands(const std::string path, const FileType type);

	/**
	 * @brief destructor.
	 */
	~ConfigCommands() { delete m_configfile; }

	/**
	 * @brief setter for file type.
	 * @param type of files.
	 */
	void setType(const FileType type);

	/**
	 * @brief parse files for commands and store them into commands instance.
	 * @return a commands instance
	 */
	Commands* getCommands();

private:
	/** the configfile instance */
	ConfigFile* m_configfile;

	/** main path for configuration files */
	std::string m_path;

	/** valid file extension */
	std::string m_extension;

	/** vector of configuration files */
	std::vector<std::string> m_files;

	/**
	 * @brief parse path for given file extension.
	 * @param path to configuration files.
	 * @param extension with file type.
	 */
	void addFiles(const std::string path, const std::string extension);

};

#endif // LIBEBUS_CONFIGFILE_H_


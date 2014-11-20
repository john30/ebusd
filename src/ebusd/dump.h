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

#ifndef DUMP_H_
#define DUMP_H_

#include <string>

/**
 * @brief class for writing raw bytes to binary file.
 */
class Dump
{

public:
	/**
	 * @brief create a new instance to write dump files.
	 * @param name the file name of dump file.
	 * @param size the max. size of the dump file, before switching.
	 */
	Dump(std::string name, long size)
		: m_name(name), m_size(size) {}

	/**
	 * @brief write byte to dump file.
	 * @param byte to write
	 * @return -1 if dump file cannot opened or renaming of dump file failed.
	 */
	int write(const char* byte);

	/**
	 * @brief set the name of dump file.
	 * @param name the file name of dump file.
	 */
	void setName(const std::string& name) { m_name = name; }

	/**
	 * @brief set the max size of dump file.
	 * @param size the max. size of the dump file, before switching.
	 */
	void setSize(const long size) { m_size = size; }

private:
	/** the name of dump file*/
	std::string m_name;

	/** max. size of dump file */
	long m_size;

};

#endif // DUMP_H_

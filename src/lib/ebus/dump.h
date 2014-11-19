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

#ifndef LIBEBUS_DUMP_H_
#define LIBEBUS_DUMP_H_

#include <string>

/**
 * @brief Class for writing raw bytes to binary file.
 */
class Dump
{

public:
	/**
	 * @brief Create a new instance to write dump files.
	 * @param filename which will be used for dumping raw bytes.
	 * @param filesize max. Size of the dump file, before switching.
	 */
	Dump(std::string filename, long filesize)
		: m_filename(filename), m_filesize(filesize) {}

	/**
	 * @brief write byte to dump file.
	 * @param byte to write
	 * @return -1 if dump file cannot opened or renaming of dump file failed.
	 */
	int write(const char* byte);

	/**
	 * @brief setter for dump file name.
	 * @param filename which will be used for dumping raw bytes.
	 */
	void setFilename(const std::string& filename) { m_filename = filename; }

	/**
	 * @brief setter for max size of dump file.
	 * @param filesize max. Size of the dump file, before switching.
	 */
	void setFilesize(const long filesize) { m_filesize = filesize; }

private:
	/** the name of dump file*/
	std::string m_filename;

	/** max. size of dump file */
	long m_filesize;

};

#endif // LIBEBUS_DUMP_H_

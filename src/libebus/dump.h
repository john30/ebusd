/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of libebus.
 *
 * libebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libebus. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBEBUS_DUMP_H_
#define LIBEBUS_DUMP_H_

#include <string>

namespace libebus
{


class Dump
{

public:
	Dump(std::string filename, long filesize)
		: m_filename(filename), m_filesize(filesize) {}

	int write(const char* byte);
	
	void setFilename(const std::string& filename) { m_filename = filename; }
	void setFilesize(const long filesize) { m_filesize = filesize; }
	
private:
	std::string m_filename;
	long m_filesize;

};


} //namespace

#endif // LIBEBUS_DUMP_H_

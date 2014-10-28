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

#include "dump.h"
#include <fstream>
#include <cstdio>

namespace libebus
{

int Dump::write(const char* byte)
{
	int ret = 0;

	std::ofstream fs(m_filename.c_str(), std::ios::out | std::ios::binary | std::ios::app);

	if (fs == 0)
		return -1;

	fs.write(byte, 1);

	if (fs.tellp() >= m_filesize * 1024) {
		std::string oldfile;
		oldfile += m_filename;
		oldfile += ".old";
		ret = rename(m_filename.c_str(), oldfile.c_str());
	}

	fs.close();

	return ret;
}



} //namespace


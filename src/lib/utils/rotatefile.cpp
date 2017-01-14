/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rotatefile.h"
#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include "clock.h"

using std::streamsize;

RotateFile::~RotateFile() {
	if (m_stream) {
		fclose(m_stream);
		m_stream = NULL;
	}
}

bool RotateFile::setEnabled(bool enabled) {
	if (enabled == m_enabled) {
		return false;
	}
	m_enabled = enabled;
	if (m_stream) {
		fclose(m_stream);
		m_stream = NULL;
	}
	if (enabled) {
		m_stream = fopen(m_fileName.c_str(), m_textMode ? "w" : "wb");
		m_fileSize = 0;
	}
	return true;
}

void RotateFile::write(unsigned char* value, unsigned int size, bool received) {
	if (!m_enabled || !m_stream) {
		return;
	}
	if (m_textMode) {
		struct timespec ts;
		struct tm td;
		clockGettime(&ts);
		localtime_r(&ts.tv_sec, &td);
		fprintf(m_stream, "%04d-%02d-%02d %02d:%02d:%02d.%03ld %c",
			td.tm_year+1900, td.tm_mon+1, td.tm_mday,
			td.tm_hour, td.tm_min, td.tm_sec, ts.tv_nsec/1000000,
			received ? '<' : '>');
		for (unsigned int pos = 0; pos < size; pos++) {
			fprintf(m_stream, "%2.2x ", value[pos]);
		}
		fprintf(m_stream, "\n");
		m_fileSize += 25+3*size+1;
	} else {
		fwrite(value, (streamsize)size, 1, m_stream);
		m_fileSize += size;
	}
	if ((m_fileSize%1024) == 0) {
		fflush(m_stream);
	}
	if (m_fileSize >= m_maxSize * 1024LL) {
		string oldfile = string(m_fileName)+".old";
		if (rename(m_fileName.c_str(), oldfile.c_str()) == 0) {
			fclose(m_stream);
			m_stream = fopen(m_fileName.c_str(), m_textMode ? "w" : "wb");
			m_fileSize = 0;
		}
	}
}

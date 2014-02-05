/*
 * Copyright (C) Roland Jax 2012-2013 <roland.jax@liwest.at>
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

#ifndef LOG_H_
#define LOG_H_

#define L_NUL 0x00
#define L_ALL 0xFF

#define L_INF 0x01
#define L_NOT 0x02
#define L_WAR 0x04
#define L_ERR 0x08
#define L_DBG 0x10
#define L_EBH 0x20
#define L_EBS 0x40
#define L_NET 0x80

#define LOGTXT "INF, NOT, WAR, ERR, DBG, EBH, EBS, NET, ALL"

#define err_if(exp) \
	if (exp) { log_print(L_ERR, "%s: %d: %s: Error %s", \
		__FILE__, __LINE__, __PRETTY_FUNCTION__, strerror(errno));\
	}

#define err_ret_if(exp, ret) \
	if (exp) { log_print(L_ERR, "%s: %d: %s: Error %s", \
		__FILE__, __LINE__, __PRETTY_FUNCTION__, strerror(errno));\
		return (ret); \
	}

void log_file(FILE *fp);
void log_level(char *lvl);

int log_open(const char *logfile, int foreground);
void log_close();

void log_print(unsigned char lvl, const char *txt, ...);

#endif /* LOG_H_ */

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>

#include "log.h"

static unsigned char loglvl = L_NUL;
static int logtxtlen = sizeof(logtxt) / sizeof(char*);

static FILE *logfp = NULL;

char *log_time(char *time);
char *log_txt(unsigned char lvl);

void
log_file(FILE *fp)
{
	if(logfp)
		fclose(logfp);

	logfp = fp;
}

void
log_level(char *lvl)
{
	unsigned char tmp;
	int i;
	char *par;
	
	loglvl = L_NUL;
	
	par = strtok(lvl, ", ");
	while (par) {
		if (strncasecmp(par, "ALL", 3) == 0) {
			loglvl = L_ALL;
			break;
		}	
		
		tmp = 0x01;
		for (i = 0; i < logtxtlen; i++) {
			if (strncasecmp(par, logtxt[i], 3) == 0) {
				loglvl |= (tmp << i);
				break;
			}
		}
		par = strtok(NULL, ", ");
	}
}

int
log_open(const char *file, int foreground)
{
	FILE *fp = NULL;

	if (foreground) {
		log_file(stdout);
	} else {
		if (file) {
			if ((fp = fopen(file, "a+")) == 0) {
				fprintf(stderr, "can't open logfile: %s\n", file);

				return -1;
			}

			log_file(fp);
		}
	}

	openlog(NULL, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

	return 0;
}

void
log_close()
{
	if (logfp) {
		fflush(logfp);
		fclose(logfp);
	}

	closelog();
}

char *
log_time(char *time)
{
	struct timeval tv;
	struct tm *tm;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	sprintf(time, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec/1000);

	return time;
}

char *
log_txt(unsigned char lvl)
{
	char *type = NULL;
	if (logtxtlen > 0) {
		unsigned char tmp;
		int i;
		
		tmp = 0x01;
		for (i = 0; i < logtxtlen; i++) {
			if (lvl == L_ALL) {
				type = "ALL";
				break;
			}	
					
			if ((lvl & (tmp << i)) != 0x00 &&
				(loglvl & (tmp << i)) != 0x00) {
				type = (char *)logtxt[i];
				break;
			}
		}
	}
	return type;	
}

void
log_print(unsigned char lvl, const char *txt, ...)
{
	char time[24];
	char buf[512];
	va_list ap;

	va_start(ap, txt);
	vsprintf(buf, txt, ap);

	if ((loglvl & lvl) != 0x00) {
		if (logfp) {
			//~ fprintf(logfp, "%s [0x%02x %s] %s\n",
					//~ log_time(time), lvl , log_txt(lvl), buf);
			fprintf(logfp, "%s [%s] %s\n",
					log_time(time), log_txt(lvl), buf);												
			fflush(logfp);

		} else {
			//~ syslog(LOG_INFO, "[0x%02x %s] %s\n",
							//~ lvl, log_txt(lvl), txt);
			syslog(LOG_INFO, "[%s] %s\n", log_txt(lvl), txt);							
		}
	}

	va_end(ap);
}


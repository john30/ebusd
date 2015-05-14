/*
 * Copyright (C) John Baier 2014-2015 <ebusd@ebusd.eu>
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

#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

using namespace std;

/** the name of each @a LogFacility. */
static const char *facilityNames[] = {
	"main",
	"network",
	"bus",
	"update",
	"all",
	NULL
};

/** the name of each @a LogLevel. */
static const char* levelNames[] = {
	"none",
	"error",
	"notice",
	"info",
	"debug",
	NULL
};

/** the bit combination of currently active log facilities (1 << @a LogFacility). */
static int logFacilites = LF_ALL;

/** the current log level. */
static LogLevel logLevel = ll_notice;

/** the current log FILE. */
static FILE* logFile = stdout;

bool setLogFacilities(const char* facilities)
{
	char *opt = (char*)facilities, *value = NULL;
	int newFacilites = 0;
	while (*opt) {
		int val = getsubopt(&opt, (char *const *)facilityNames, &value);
		if (val < 0 || val > lf_COUNT || value)
			return false;
		if (val == lf_COUNT) {
			newFacilites = LF_ALL;
			break;
		}
		newFacilites |= 1<<val;
	}

	logFacilites = newFacilites;
	return true;
}

bool setLogLevel(const char* level)
{
	char *opt = (char*)level, *value = NULL;
	int newLevel = 0;
	if (*opt) {
		int val = getsubopt(&opt, (char *const *)levelNames, &value);
		if (val < 0 || val >= ll_COUNT || value || *opt)
			return false;
		newLevel = val;
	}

	logLevel = (LogLevel)newLevel;
	return true;
}

bool setLogFile(const char* filename)
{
	FILE* newFile = fopen(filename, "a");
	if (newFile == NULL)
		return false;

	closeLogFile();
	logFile = newFile;
	return true;
}

void closeLogFile()
{
	if (logFile != NULL) {
		if (logFile != stdout)
			fclose(logFile);
		logFile = NULL;
	}
}

bool needsLog(const LogFacility facility, const LogLevel level)
{
	return ((logFacilites & (1<<facility)) != 0)
		&& (logLevel >= level);
}

void logWrite(const LogFacility facility, const LogLevel level, const char* message, ...)
{
	struct timespec ts;
	struct tm* tm;

	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&ts.tv_sec);

	char* buf;
	va_list ap;
	va_start(ap, message);

	if (vasprintf(&buf, message, ap) >= 0 && buf) {
		fprintf(logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s %s] %s\n",
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, ts.tv_nsec/1000000,
			facilityNames[facility], levelNames[level], buf);
		fflush(logFile);
	}

	va_end(ap);
	if (buf)
		free(buf);
}

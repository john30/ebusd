/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
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

#include "lib/utils/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include "lib/utils/clock.h"

/** the name of each @a LogFacility. */
static const char *facilityNames[] = {
  "main",
  "network",
  "bus",
  "update",
  "other",
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
static int s_logFacilites = LF_ALL;

/** the current log level. */
static LogLevel s_logLevel = ll_notice;

/** the current log FILE. */
static FILE* s_logFile = stdout;

bool setLogFacilities(const char* facilities) {
  char *input = strdup(facilities);
  char *opt = reinterpret_cast<char*>(input), *value = NULL;
  int newFacilites = 0;
  while (*opt) {
    int val = getsubopt(&opt, (char *const *)facilityNames, &value);
    if (val < 0 || val > lf_COUNT || value) {
      free(input);
      return false;
    }
    if (val == lf_COUNT) {
      newFacilites = LF_ALL;
    } else {
      newFacilites |= 1 << val;
    }
  }
  s_logFacilites = newFacilites;
  free(input);
  return true;
}

bool getLogFacilities(char* buffer) {
  if (s_logFacilites == LF_ALL) {
    return snprintf(buffer, 48, "%s", facilityNames[lf_COUNT]) != 0;
  }
  *buffer = 0;  // for strcat to work
  bool found = false;
  size_t len = 0;
  for (int val = 0; val < lf_COUNT; val++) {
    if (s_logFacilites&(1 << val)) {
      if (found) {
        len += snprintf(buffer+len, 48-len, ",");
      }
      found = true;
      len += snprintf(buffer+len, 48-len, "%s", facilityNames[val]);
    }
  }
  return true;
}

bool setLogLevel(const char* level) {
  char *input = strdup(level);
  char *opt = reinterpret_cast<char*>(input), *value = NULL;
  int newLevel = 0;
  if (*opt) {
    int val = getsubopt(&opt, (char *const *)levelNames, &value);
    if (val < 0 || val >= ll_COUNT || value || *opt) {
      free(input);
      return false;
    }
    newLevel = val;
  }
  s_logLevel = (LogLevel)newLevel;
  free(input);
  return true;
}

const char* getLogLevel() {
  return levelNames[s_logLevel];
}

bool setLogFile(const char* filename) {
  FILE* newFile = fopen(filename, "a");
  if (newFile == NULL) {
    return false;
  }
  closeLogFile();
  s_logFile = newFile;
  return true;
}

void closeLogFile() {
  if (s_logFile != NULL) {
    if (s_logFile != stdout) {
      fclose(s_logFile);
    }
    s_logFile = NULL;
  }
}

bool needsLog(const LogFacility facility, const LogLevel level) {
  return ((s_logFacilites & (1 << facility)) != 0)
    && (s_logLevel >= level);
}

void logWrite(const char* facility, const char* level, const char* message, va_list ap) {
  if (s_logFile == NULL) {
    return;
  }
  struct timespec ts;
  struct tm td;
  clockGettime(&ts);
  localtime_r(&ts.tv_sec, &td);
  char* buf;
  if (vasprintf(&buf, message, ap) >= 0 && buf) {
    fprintf(s_logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s %s] %s\n",
      td.tm_year+1900, td.tm_mon+1, td.tm_mday,
      td.tm_hour, td.tm_min, td.tm_sec, ts.tv_nsec/1000000,
      facility, level, buf);
    fflush(s_logFile);
  }
  if (buf) {
    free(buf);
  }
}

void logWrite(const LogFacility facility, const LogLevel level, const char* message, ...) {
  va_list ap;
  va_start(ap, message);
  logWrite(facilityNames[facility], levelNames[level], message, ap);
  va_end(ap);
}

void logWrite(const char* facility, const LogLevel level, const char* message, ...) {
  va_list ap;
  va_start(ap, message);
  logWrite(facility, levelNames[level], message, ap);
  va_end(ap);
}

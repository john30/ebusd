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

namespace ebusd {

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

/** the current log level by log facility. */
static LogLevel s_facilityLogLevel[] = { ll_notice, ll_notice, ll_notice, ll_notice, ll_notice, };

/** the current log FILE. */
static FILE* s_logFile = stdout;

LogFacility parseLogFacility(const char* facility) {
  if (!facility) {
    return lf_COUNT;
  }
  char *input = strdup(facility);
  char *opt = reinterpret_cast<char*>(input), *value = NULL;
  int val = getsubopt(&opt, (char *const *)facilityNames, &value);
  if (val < 0 || val >= lf_COUNT || value || *opt) {
    free(input);
    return lf_COUNT;
  }
  free(input);
  return (LogFacility)val;
}

int parseLogFacilities(const char* facilities) {
  char *input = strdup(facilities);
  char *opt = reinterpret_cast<char*>(input), *value = NULL;
  int newFacilites = 0;
  while (*opt) {
    int val = getsubopt(&opt, (char *const *)facilityNames, &value);
    if (val < 0 || val > lf_COUNT || value) {
      free(input);
      return -1;
    }
    if (val == lf_COUNT) {
      newFacilites = LF_ALL;
    } else {
      newFacilites |= 1 << val;
    }
  }
  free(input);
  return newFacilites;
}

LogLevel parseLogLevel(const char* level) {
  if (!level) {
    return ll_COUNT;
  }
  char *input = strdup(level);
  char *opt = reinterpret_cast<char*>(input), *value = NULL;
  int val = getsubopt(&opt, (char *const *)levelNames, &value);
  if (val < 0 || val >= ll_COUNT || value || *opt) {
    free(input);
    return ll_COUNT;
  }
  free(input);
  return (LogLevel)val;
}

const char* getLogFacilityStr(LogFacility facility) {
  return facilityNames[facility];
}

const char* getLogLevelStr(LogLevel level) {
  return levelNames[level];
}

bool setFacilitiesLogLevel(int facilities, LogLevel level) {
  bool changed = false;
  for (int val = 0; val < lf_COUNT && facilities != 0; val++) {
    if ((facilities & (1 << val)) != 0 && s_facilityLogLevel[(LogFacility)val] != level) {
      s_facilityLogLevel[(LogFacility)val] = level;
      changed = true;
    }
  }
  return changed;
}

LogLevel getFacilityLogLevel(LogFacility facility) {
  return s_facilityLogLevel[facility];
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
  return s_facilityLogLevel[facility] >= level;
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

}  // namespace ebusd

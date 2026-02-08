/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2026 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/utils/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include "lib/utils/clock.h"

namespace ebusd {

/** the name of each @a LogFacility. */
static const char *s_facilityNames[] = {
  "main",
  "network",
  "bus",
  "device",
  "update",
  "other",
  "all",
  nullptr
};

/** the name of each @a LogLevel. */
static const char* s_levelNames[] = {
  "none",
  "error",
  "notice",
  "info",
  "debug",
  nullptr
};

#ifdef HAVE_SYSLOG_H
/** the syslog level of each @a LogLevel. */
static const int s_syslogLevels[] = {
  LOG_INFO,
  LOG_ERR,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  0
};
#endif

/** the current log level by log facility. */
static LogLevel s_facilityLogLevel[] = { ll_notice, ll_notice, ll_notice, ll_notice, ll_notice, ll_notice, };

/** the current log FILE, or nullptr if closed or syslog is used. */
static FILE* s_logFile = stdout;

#ifdef HAVE_SYSLOG_H
/** whether to log to syslog. */
static bool s_useSyslog = false;
#endif

LogFacility parseLogFacility(const char* facility) {
  if (!facility) {
    return lf_COUNT;
  }
  char *input = strdup(facility);
  char *opt = reinterpret_cast<char*>(input), *value = nullptr;
  int val = getsubopt(&opt, (char *const *)s_facilityNames, &value);
  if (val < 0 || val >= lf_COUNT || value || *opt) {
    free(input);
    return lf_COUNT;
  }
  free(input);
  return (LogFacility)val;
}

int parseLogFacilities(const char* facilities) {
  char *input = strdup(facilities);
  char *opt = reinterpret_cast<char*>(input), *value = nullptr;
  int newFacilites = 0;
  while (*opt) {
    int val = getsubopt(&opt, (char *const *)s_facilityNames, &value);
    if (val < 0 || val > lf_COUNT || value) {
      free(input);
      return -1;
    }
    newFacilites |= 1 << val;
  }
  free(input);
  return newFacilites;
}

LogLevel parseLogLevel(const char* level) {
  if (!level) {
    return ll_COUNT;
  }
  char *input = strdup(level);
  char *opt = reinterpret_cast<char*>(input), *value = nullptr;
  int val = getsubopt(&opt, (char *const *)s_levelNames, &value);
  if (val < 0 || val >= ll_COUNT || value || *opt) {
    free(input);
    return ll_COUNT;
  }
  free(input);
  return (LogLevel)val;
}

const char* getLogFacilityStr(LogFacility facility) {
  return s_facilityNames[facility];
}

const char* getLogLevelStr(LogLevel level) {
  return s_levelNames[level];
}

bool setFacilitiesLogLevel(int facilities, LogLevel level) {
  bool changed = false;
  for (int val = 0; val < lf_COUNT && facilities != 0; val++) {
    if ((facilities & ((1 << val)|(1 << lf_COUNT))) != 0 && s_facilityLogLevel[(LogFacility)val] != level) {
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
  if (filename[0] == 0) {
    closeLogFile();
#ifdef HAVE_SYSLOG_H
    openlog("ebusd", LOG_NDELAY|LOG_PID, LOG_USER);
    s_useSyslog = true;
#else
    s_logFile = stdout;
#endif
    return true;
  }
  FILE* newFile = fopen(filename, "a");
  if (newFile == nullptr) {
    return false;
  }
  closeLogFile();
  s_logFile = newFile;
  return true;
}

void closeLogFile() {
  if (s_logFile != nullptr) {
    if (s_logFile != stdout) {
      fclose(s_logFile);
    }
    s_logFile = nullptr;
  }
#ifdef HAVE_SYSLOG_H
  if (s_useSyslog) {
    closelog();
    s_useSyslog = false;
  }
#endif
}

bool needsLog(const LogFacility facility, const LogLevel level) {
  if (s_logFile == nullptr
#ifdef HAVE_SYSLOG_H
    && !s_useSyslog
#endif
  ) {
    return false;
  }
  return s_facilityLogLevel[facility] >= level;
}

void logWrite(const char* facility, const LogLevel level, const char* message, va_list ap) {
  if (s_logFile == nullptr
#ifdef HAVE_SYSLOG_H
    && !s_useSyslog
#endif
  ) {
    return;
  }
  char* buf;
  if (vasprintf(&buf, message, ap) >= 0 && buf) {
#ifdef HAVE_SYSLOG_H
    if (s_useSyslog) {
      syslog(s_syslogLevels[level], "[%s %s] %s", facility, s_levelNames[level], buf);
    } else {
#endif
#ifdef LOG_WITHOUT_TIMEPREFIX
      fprintf(s_logFile, "[%s %s] %s\n",
#else
      struct timespec ts;
      struct tm td;
      clockGettime(&ts);
      localtime_r(&ts.tv_sec, &td);
      fprintf(s_logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s %s] %s\n",
        td.tm_year+1900, td.tm_mon+1, td.tm_mday,
        td.tm_hour, td.tm_min, td.tm_sec, ts.tv_nsec/1000000,
#endif
        facility, s_levelNames[level], buf);
      fflush(s_logFile);
#ifdef HAVE_SYSLOG_H
    }
#endif
  }
  if (buf) {
    free(buf);
  }
}

void logWrite(const LogFacility facility, const LogLevel level, const char* message, ...) {
  va_list ap;
  va_start(ap, message);
  logWrite(s_facilityNames[facility], level, message, ap);
  va_end(ap);
}

void logWrite(const char* facility, const LogLevel level, const char* message, ...) {
  va_list ap;
  va_start(ap, message);
  logWrite(facility, level, message, ap);
  va_end(ap);
}

}  // namespace ebusd

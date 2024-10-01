/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_UTILS_LOG_H_
#define LIB_UTILS_LOG_H_

namespace ebusd {

/** \file lib/utils/log.h */

/** the available log facilities. */
enum LogFacility {
  lf_main = 0,  //!< main loop
  lf_network,   //!< network related
  lf_bus,       //!< eBUS related
  lf_device,    //!< device related
  lf_update,    //!< updates found while listening to the bus
  lf_other,     //!< all other log facilities
  lf_COUNT = 5  //!< number of available log facilities and flag for setting all
};

/** the available log levels. */
enum LogLevel {
  ll_none = 0,  //!< no level at all
  ll_error,     //!< error message
  ll_notice,    //!< important message
  ll_info,      //!< informational message
  ll_debug,     //!< debugging message (normally suppressed)
  ll_COUNT = 5  //!< number of available log levels
};

/**
 * Parse the log facility from the string.
 * @param facility the string to parse the singular facility from.
 * @return the @a LogFacility, or @a lf_COUNT on error.
 */
LogFacility parseLogFacility(const char* facility);

/**
 * Parse the log facilities from the string.
 * @param facilities the string to parse the list of facilities from (separated by comma).
 * @return the @a LogFacility list as bit mask (1 << facility), or -1 on error.
 */
int parseLogFacilities(const char* facilities);

/**
 * Get the log facility as string.
 * @param facility the @a LogFacility.
 * @return the log facility as string.
 */
const char* getLogFacilityStr(LogFacility facility);

/**
 * Parse the log level from the string.
 * @param level the level as string.
 * @return the @a LogLevel, or @a ll_COUNT on error.
 */
LogLevel parseLogLevel(const char* level);

/**
 * Get the log level as string.
 * @param level the @a LogLevel.
 * @return the log level as string.
 */
const char* getLogLevelStr(LogLevel level);

/**
 * Set the log level for the specified facilities.
 * @param facilities the log facilities as bit mask (1 << facility).
 * @param level the @a LogLevel to set.
 * @return true when a level was changed for a facility, false when no level was changed at all.
 */
bool setFacilitiesLogLevel(int facilities, LogLevel level);

/**
 * Get the log level for the specified facility.
 * @param facility the @a LogFacility.
 * @return the @a LogLevel.
 */
LogLevel getFacilityLogLevel(LogFacility facility);

/**
 * Set the log file to use.
 * @param filename the name of the log file to use, or the empty string for syslog.
 * @return true on success, false on error.
 */
bool setLogFile(const char* filename);

/**
 * Close the log file if necessary.
 */
void closeLogFile();

/**
 * Return whether logging is needed for the specified facility and level.
 * @param facility the @a LogFacility of the message to check.
 * @param level the @a LogLevel of the message to check.
 * @return true when logging is needed for the specified facility and level.
 */
bool needsLog(const LogFacility facility, const LogLevel level);

/**
 * Log the message for the specified @a LogFacility and @a LogLevel (even if logging is not needed for the facility/level).
 * @param facility the @a LogFacility of the message to log.
 * @param level the @a LogLevel of the message to log.
 * @param message the message to log.
 * @param ... variable arguments depending on the @a message.
 */
void logWrite(const LogFacility facility, const LogLevel level, const char* message, ...);

/**
 * Log the message for the specified facility name @a LogLevel (even if logging is not needed for the facility/level).
 * @param facility the facility name of the message to log.
 * @param level the @a LogLevel of the message to log.
 * @param message the message to log.
 * @param ... variable arguments depending on the @a message.
 */
void logWrite(const char* facility, const LogLevel level, const char* message, ...);

/** A macro that calls the logging function only if needed. */
#define LOG(facility, level, ...) (needsLog(facility, level) ? logWrite(facility, level, __VA_ARGS__) : void(0))

/** A macro for an error message that calls the logging function only if needed. */
#define logError(facility, ...) (needsLog(facility, ll_error) ? logWrite(facility, ll_error, __VA_ARGS__) : void(0))

/** A macro for a notice message that calls the logging function only if needed. */
#define logNotice(facility, ...) (needsLog(facility, ll_notice) ? logWrite(facility, ll_notice, __VA_ARGS__) : void(0))

/** A macro for an info message that calls the logging function only if needed. */
#define logInfo(facility, ...) (needsLog(facility, ll_info) ? logWrite(facility, ll_info, __VA_ARGS__) : void(0))

/** A macro for a debug message that calls the logging function only if needed. */
#define logDebug(facility, ...) (needsLog(facility, ll_debug) ? logWrite(facility, ll_debug, __VA_ARGS__) : void(0))

/** A macro for an error message that calls the logging function only if needed. */
#define logOtherError(facility, ...) \
  (needsLog(lf_other, ll_error) ? logWrite(facility, ll_error, __VA_ARGS__) : void(0))

/** A macro for a notice message that calls the logging function only if needed. */
#define logOtherNotice(facility, ...) \
  (needsLog(lf_other, ll_notice) ? logWrite(facility, ll_notice, __VA_ARGS__) : void(0))

/** A macro for an info message that calls the logging function only if needed. */
#define logOtherInfo(facility, ...) \
  (needsLog(lf_other, ll_info) ? logWrite(facility, ll_info, __VA_ARGS__) : void(0))

/** A macro for a debug message that calls the logging function only if needed. */
#define logOtherDebug(facility, ...) \
  (needsLog(lf_other, ll_debug) ? logWrite(facility, ll_debug, __VA_ARGS__) : void(0))

}  // namespace ebusd

#endif  // LIB_UTILS_LOG_H_

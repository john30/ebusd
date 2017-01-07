/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
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

#ifndef LIBUTILS_LOG_H_
#define LIBUTILS_LOG_H_

/** \file log.h */

/** the available log facilities. */
enum LogFacility {
	lf_main=0,  //!< main loop
	lf_network, //!< network related
	lf_bus,     //!< eBUS related
	lf_update,  //!< updates found while listening to the bus
	lf_other,   //!< all other log facilities
	lf_COUNT=5  //!< number of available log facilities
};

/** macro for enabling all log facilities. */
#define LF_ALL ((1<<lf_main) | (1<<lf_network) | (1<<lf_bus) | (1<<lf_update) | (1<<lf_other))

/** the available log levels. */
enum LogLevel {
	ll_none=0, //!< no level at all
	ll_error,  //!< error message
	ll_notice, //!< important message
	ll_info,   //!< informational message
	ll_debug,  //!< debugging message (normally suppressed)
	ll_COUNT=5 //!< number of available log levels
};

/**
 * Set the log facilities from the string.
 * @param facilities the string to parse the facilities from (separated by comma).
 * @return true on success, false on error.
 */
bool setLogFacilities(const char* facilities);

/**
 * Get the log facilities.
 * @param buffer the buffer into which the facilities are written to (separated by comma, buffer needs to be at last 32 characters long).
 * @return true on success, false on error.
 */
bool getLogFacilities(char* buffer);

/**
 * Parse the log level from the string.
 * @param level the level as string.
 * @return true on success, false on error.
 */
bool setLogLevel(const char* level);

/**
 * Get the log level.
 * @return the level as string.
 */
const char* getLogLevel();

/**
 * Set the log file to use.
 * @param filename the name of the log file to use.
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
#define logOtherError(facility, ...) (needsLog(lf_other, ll_error) ? logWrite(facility, ll_error, __VA_ARGS__) : void(0))

/** A macro for a notice message that calls the logging function only if needed. */
#define logOtherNotice(facility, ...) (needsLog(lf_other, ll_notice) ? logWrite(facility, ll_notice, __VA_ARGS__) : void(0))

/** A macro for an info message that calls the logging function only if needed. */
#define logOtherInfo(facility, ...) (needsLog(lf_other, ll_info) ? logWrite(facility, ll_info, __VA_ARGS__) : void(0))

/** A macro for a debug message that calls the logging function only if needed. */
#define logOtherDebug(facility, ...) (needsLog(lf_other, ll_debug) ? logWrite(facility, ll_debug, __VA_ARGS__) : void(0))

#endif // LIBUTILS_LOG_H_

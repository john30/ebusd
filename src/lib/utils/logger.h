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

#ifndef LIBUTILS_LOGGER_H_
#define LIBUTILS_LOGGER_H_

#include "wqueue.h"
#include "thread.h"
#include <string>
#include <functional>
#include <algorithm>
#include <vector>
#include <cstdarg>

/** \file logger.h */

using namespace std;

/** available types for all subsystems */
enum AreasType {
	bas=0,           /*!< basis */
	net,             /*!< network */
	bus,             /*!< ebus */
	upd,             /*!< updates found while listening to the bus */
	Size_of_Areas=4  /*!< number of possible areas */
};

/** available logging levels */
enum LevelType {
	error=0,       /*!< silent run, only errors will be printed */
	event,         /*!< only interesting message for normal use */
	trace,         /*!< most of the information for normal use */
	debug,         /*!< print internal states too */
	Size_of_Level  /*!< number of possible levels */
};

/**
 * Calculate the mask of logging areas from string.
 * @param areas the string to parse the areas from.
 * @return the bit combination of @a AreasType.
 */
int calcAreaMask(const string areas);

/**
 * Calculate the log level from string.
 * @param level the level as string.
 * @return @a the LevelType.
 */
LevelType calcLevel(const string level);

/**
 * class which describes a logging message itself.
 */
class LogMessage
{

public:
	/**
	 * creates a new logging message.
	 * @param area the logging area of the message.
	 * @param level the logging level of the message.
	 * @param text the logging message.
	 */
	LogMessage(const int area, const int level, const string text);

	/**
	 * get the logging area.
	 * @return the logging area.
	 */
	int getArea() const { return m_area; }

	/**
	 * get the logging level.
	 * @return the logging level.
	 */
	int getLevel() const { return m_level; }

	/**
	 * get the logging text.
	 * @return the logging text.
	 */
	string getText() const { return m_text.c_str(); }

	/**
	 * get the logging timestamp.
	 * @return the logging timestamp.
	 */
	string getTime() const { return m_time.c_str(); }

private:
	/** the logging area. */
	int m_area;

	/** the logging level. */
	int m_level;

	/** the logging message. */
	string m_text;

	/** the logging timestamp. */
	string m_time;

};

/**
 * base class for all type of logging sinks.
 */
class LogSink : public Thread
{

public:
	/**
	 * creates a virtual logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 */
	LogSink(const int areaMask, const int level) : m_areaMask(areaMask), m_level(level) {}

	/**
	 * destructor.
	 */
	virtual ~LogSink();

	/**
	 * adds the logging message to internal message queue.
	 * @param message a reference to logging message.
	 */
	void addMessage(const LogMessage& message);

	/**
	 * endless loop for logging sink instance.
	 */
	void run();

	/**
	 * get the logging area mask.
	 * @return the logging area mask.
	 */
	int getAreaMask() const { return m_areaMask; }

	/**
	 * set the logging area mask.
	 * @param areaMask the logging area mask.
	 */
	void setAreaMask(const int& areaMask) { m_areaMask = areaMask; }

	/**
	 * get the logging level.
	 * @return the logging level.
	 */
	int getLevel() const { return (m_level); }

	/**
	 * set the logging level.
	 * @param level the logging level.
	 */
	void setLevel(const int& level) { m_level = level; }

protected:
	/** queue for logging messages */
	WQueue<LogMessage*> m_logQueue;

private:
	/** the logging area mask. */
	int m_areaMask;

	/** the logging level. */
	int m_level;

protected:

	/**
	 * virtual function for writing the logging message.
	 * @param message the logging message.
	 */
	virtual void write(const LogMessage& message) const = 0;

};

/**
 * class for console logging sink type.
 */
class LogConsole : public LogSink
{

public:
	/**
	 * creates a console logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 */
	LogConsole(const int areaMask, const int level, const char* name)
		: LogSink(areaMask, level) { this->start(name); }

	/**
	 * destructor.
	 */
	virtual ~LogConsole() {}

protected:

	/**
	 * write the logging message to stdout.
	 * @param message the logging message.
	 */
	virtual void write(const LogMessage& message) const;

};

/**
 * class for logfile logging sink type.
 */
class LogFile : public LogSink
{

public:
	/**
	 * creates a log file logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 * @param file the log file.
	 */
	LogFile(const int areaMask, const int level, const char* name, const char* file)
		: LogSink(areaMask, level), m_file(file) { this->start(name); }

	/**
	 * destructor.
	 */
	virtual ~LogFile() {}

private:

	/** the logging file */
	string m_file;

protected:

	/**
	 * write the logging message to specific log file.
	 * @param message the logging message.
	 */
	virtual void write(const LogMessage& message) const;

};

/**
 * logger base class which provide the logging interface.
 */
class Logger : public Thread
{

public:
	/**
	 * create an instance and return the reference.
	 * @return the reference to instance.
	 */
	static Logger& Instance();

	/**
	 * destructor.
	 */
	virtual ~Logger();

	/**
	 * Add a @a LogSink.
	 * @param sink the used @a LogSink.
	 * @return the reference to this @a Logger.
	 */
	Logger& operator+=(LogSink* sink);

	/**
	 * Remove a @a LogSink and delete it.
	 * @param sink the used @a LogSink.
	 * @return the reference to this @a Logger.
	 */
	Logger& operator-=(const LogSink* sink);

	/**
	 * Set the logging area mask on all @a LogSink instances.
	 * @param areaMask the logging area mask.
	 */
	void setAreaMask(const int& areaMask);

	/**
	 * Set the logging level on all @a LogSink instances.
	 * @param level the logging level.
	 */
	void setLevel(const int& level);

	/**
	 * Return whether a @a LogSink is available that will produce output for the specified area and level.
	 * @param area the logging area of the message.
	 * @param level the logging level of the message.
	 * @return whether a @a LogSink is available that will produce output for the specified area and level.
	 */
	bool hasSink(const int area, const int level);

	/**
	 * creates a logging message and add them to internal message queue.
	 * @param area the logging area of the message.
	 * @param level the logging level of the message.
	 * @param text the logging message.
	 * @param ... possible 'variable argument lists'.
	 */
	void log(const int area, const int level, const string& text, ...);

	//@copydoc
	virtual bool start(const char* name);

	//@copydoc
	virtual void stop();

protected:

	//@copydoc
	virtual void run();

private:
	/**
	 * private constructor.
	 */
	Logger() : m_direct(true), m_sink(NULL) {}

	/**
	 * private copy constructor.
	 * @param reference to an instance.
	 */
	Logger(const Logger&);

	/**
	 * private = operator.
	 * @param reference to an instance.
	 * @return reference to instance.
	 */
	Logger& operator=(const Logger&);

	/**
	 * Distribute the @a LogMessage to all known sinks and delete it afterwards.
	 * @param mesage the @a LogMessage to distribute.
	 */
	void handleMessage(LogMessage* message);

	/** true to directly log to all sinks, false to buffer via @a m_logQueue. */
	bool m_direct;

	/** the used @a LogSink, or NULL. */
	LogSink* m_sink;

	/** queue for logging messages. */
	WQueue<LogMessage*> m_logQueue;

};

#endif // LIBUTILS_LOGGER_H_

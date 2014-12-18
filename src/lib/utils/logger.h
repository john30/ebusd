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

/** @brief available types for all subsystems */
enum AreasType {
	bas=0,           /*!< basis */
	net,             /*!< network */
	bus,             /*!< ebus */
	upd,             /*!< updates found while listening to the bus */
	Size_of_Areas=4  /*!< number of possible areas */
};

/** @brief available logging levels */
enum LevelType {
	error=0,       /*!< silent run, only errors will be printed */
	event,         /*!< only interesting message for normal use */
	trace,         /*!< most of the information for normal use */
	debug,         /*!< print internal states too */
	Size_of_Level  /*!< number of possible levels */
};

/**
 * @brief Calculate the mask of logging areas from string.
 * @param areas the string to parse the areas from.
 * @return the bit combination of @a AreasType.
 */
int calcAreaMask(const string areas);

/**
 * @brief Calculate the log level from string.
 * @param level the level as string.
 * @return @a the LevelType.
 */
LevelType calcLevel(const string level);

/**
 * @brief class which describes a logging message itself.
 */
class LogMessage
{

public:
	/**
	 * @brief creates a new logging message.
	 * @param area the logging area of the message.
	 * @param level the logging level of the message.
	 * @param text the logging message.
	 * @param running the status of logging subsystem.
	 */
	LogMessage(const int area, const int level, const string text, const bool running=true);

	/**
	 * @brief get the logging area.
	 * @return the logging area.
	 */
	int getArea() const { return m_area; }

	/**
	 * @brief get the logging level.
	 * @return the logging level.
	 */
	int getLevel() const { return m_level; }

	/**
	 * @brief get the logging text.
	 * @return the logging text.
	 */
	string getText() const { return m_text.c_str(); }

	/**
	 * @brief status of logging subsystem.
	 * @return false if logging subsystem is going down.
	 */
	bool isRunning() const { return m_running; }

	/**
	 * @brief get the logging timestamp.
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

	/** true if this instance is running. */
	bool m_running;

	/** the logging timestamp. */
	string m_time;

};

/**
 * @brief base class for all type of logging sinks.
 */
class LogSink : public Thread
{

public:
	/**
	 * @brief creates a virtual logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 */
	LogSink(const int areaMask, const int level) : m_areaMask(areaMask), m_level(level) {}

	/**
	 * @brief adds the logging message to internal message queue.
	 * @param message a reference to logging message.
	 */
	void addMessage(const LogMessage& message);

	/**
	 * @brief endless loop for logging sink instance.
	 */
	void run();

	/**
	 * @brief get the logging area mask.
	 * @return the logging area mask.
	 */
	int getAreaMask() const { return m_areaMask; }

	/**
	 * @brief set the logging area mask.
	 * @param areaMask the logging area mask.
	 */
	void setAreaMask(const int& areaMask) { m_areaMask = areaMask; }

	/**
	 * @brief get the logging level.
	 * @return the logging level.
	 */
	int getLevel() const { return (m_level); }

	/**
	 * @brief set the logging level.
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

	/**
	 * @brief virtual function for writing the logging message.
	 * @param message the logging message.
	 */
	virtual void write(const LogMessage& message) const = 0;

};

/**
 * @brief class for console logging sink type.
 */
class LogConsole : public LogSink
{

public:
	/**
	 * @brief creates a console logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 */
	LogConsole(const int areaMask, const int level, const char* name)
		: LogSink(areaMask, level) { this->start(name); }

private:
	/**
	 * @brief write the logging message to stdout.
	 * @param message the logging message.
	 */
	void write(const LogMessage& message) const;

};

/**
 * @brief class for logfile logging sink type.
 */
class LogFile : public LogSink
{

public:
	/**
	 * @brief creates a log file logging sink.
	 * @param areaMask the logging area mask.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 * @param file the log file.
	 */
	LogFile(const int areaMask, const int level, const char* name, const char* file)
		: LogSink(areaMask, level), m_file(file) { this->start(name); }

private:
	/** the logging file */
	string m_file;

	/**
	 * @brief write the logging message to specific log file.
	 * @param message the logging message.
	 */
	void write(const LogMessage& message) const;

};

/**
 * @brief logger base class which provide the logging interface.
 */
class Logger : public Thread
{

public:
	/**
	 * @brief create an instance and return the reference.
	 * @return the reference to instance.
	 */
	static Logger& Instance();

	/**
	 * @brief destructor.
	 */
	~Logger();

	/**
	 * @brief adds a logging sink and returns the reference to logger.
	 * @param sink the used logging sink.
	 * @return the reference to logger.
	 */
	Logger& operator+=(LogSink* sink);

	/**
	 * @brief removes a logging sink and returns the reference to logger.
	 * @param sink the used logging sink.
	 * @return the reference to logger.
	 */
	Logger& operator-=(const LogSink* sink);

	/**
	 * @brief creates a logging message and add them to internal message queue.
	 * @param area the logging area of the message.
	 * @param level the logging level of the message.
	 * @param text the logging message.
	 * @param ... possible 'variable argument lists'.
	 */
	void log(const int area, const int level, const string& text, ...);

	/**
	 * @brief returns the sink at the specified index.
	 * @param index the index of the sink to return.
	 * @return the sink at the specified index.
	 */
	LogSink* getSink(const int index) const { return(m_sinks[index]); }

	//@copydoc
	virtual bool start(const char* name);

	/**
	 * @brief endless loop for logger instance.
	 */
	virtual void run();

	/**
	 * @brief shutdown logger subsystem.
	 */
	virtual void stop();

private:
	/**
	 * @brief private construtor.
	 */
	Logger() : m_direct(true) {}

	/**
	 * @brief private copy construtor.
	 * @param reference to an instance.
	 */
	Logger(const Logger&);

	/**
	 * @brief private = operator.
	 * @param reference to an instance.
	 * @return reference to instance.
	 */
	Logger& operator=(const Logger&);

	/**
	 * @brief Distribute the @a LogMessage to all known sinks and delete it afterwards.
	 * @param mesage the @a LogMessage to distribute.
	 * @return true to continue running, false to stop.
	 */
	bool handleMessage(LogMessage* message);

	/** typedefs for a vector of type LogSink* */
	typedef vector<LogSink*> sink_t;

	/** typedefs for a vector of type LogSink* iterator */
	typedef vector<LogSink*>::iterator sinkCI_t;

	/** true to directly log to all sinks, false to buffer via @a m_logQueue. */
	bool m_direct;

	/** vector of available logging sinks */
	sink_t m_sinks;

	/** queue for logging messages */
	WQueue<LogMessage*> m_logQueue;

};

#endif // LIBUTILS_LOGGER_H_

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

/** available types for all subsystems */
enum AreasType {
	bas=1,           // basis
	net=2,           // network
	bus=4,           // ebus
	all=7,           // type for all subsystems
	Size_of_Areas=3, // number of possible areas
};

/** available logging levels */
enum LevelType {
	error=0,       //
	event,         //
	trace,         //
	debug,         //
	Size_of_Level, // number of possible levels
};

/** global function to get calculate logging areas */
int calcAreas(const std::string areas);

/** global function to get calculate logging level */
int calcLevel(const std::string level);

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
	LogMessage(const int area, const int level, const std::string text, const bool running=true);

	/**
	 * @brief get the logging area.
	 * @return the logging area.
	 */
	int getArea() const { return (m_area); }

	/**
	 * @brief get the logging level.
	 * @return the logging level.
	 */
	int getLevel() const { return(m_level); }

	/**
	 * @brief get the logging text.
	 * @return the logging text.
	 */
	std::string getText() const { return (m_text.c_str()); }

	/**
	 * @brief status of logging subsystem.
	 * @return false if logging subsystem is going down.
	 */
	bool isRunning() const { return m_running; }

	/**
	 * @brief get the logging timestamp.
	 * @return the logging timestamp.
	 */
	std::string getTime() const { return (m_time.c_str()); }

private:
	/** the logging area */
	int m_area;

	/** the logging level */
	int m_level;

	/** the logging message */
	std::string m_text;

	/** true if this instance is running */
	bool m_running;

	/** the logging timestamp */
	std::string m_time;

};

/**
 * @brief base class for all type of logging sinks.
 */
class LogSink : public Thread
{

public:
	/**
	 * @brief creates a virtual logging sink.
	 * @param areas the logging areas.
	 * @param level the logging level.
	 */
	LogSink(const int areas, const int level) : m_areas(areas), m_level(level) {}

	/**
	 * @brief adds the logging message to internal message queue.
	 * @param message a reference to logging message.
	 */
	void addMessage(const LogMessage& message);

	/**
	 * @brief endless loop for logging sink instance.
	 * @return void pointer.
	 */
	void* run();

	/**
	 * @brief get the logging areas.
	 * @return the logging areas.
	 */
	int getAreas() const { return (m_areas); }

	/**
	 * @brief set the logging areas.
	 * @param areas the logging areas.
	 */
	void setAreas(const int& areas) { m_areas = areas; }

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
	/** the logging areas */
	int m_areas;

	/** the logging level */
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
	 * @param areas the logging areas.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 */
	LogConsole(const int areas, const int level, const char* name)
		: LogSink(areas, level) { this->start(name); }

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
	 * @param areas the logging areas.
	 * @param level the logging level.
	 * @param name the thread name for logging sink.
	 * @param file the log file.
	 */
	LogFile(const int areas, const int level, const char* name, const char* file)
		: LogSink(areas, level), m_file(file) { this->start(name); }

private:
	/** the logging file */
	std::string m_file;

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
	 * @param pointer of logging sink.
	 * @return the reference to logger.
	 */
	Logger& operator+=(LogSink* sink);

	/**
	 * @brief removes a logging sink and returns the reference to logger.
	 * @param pointer of logging sink.
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
	void log(const int area, const int level, const std::string& text, ...);

	/**
	 * @brief returns the sink at the specified index.
	 * @param index the index of the sink to return.
	 * @return the sink at the specified index.
	 */
	LogSink* getSink(const int index) const { return(m_sinks[index]); }

	/**
	 * @brief endless loop for logger instance.
	 * @return void pointer.
	 */
	void* run();

	/**
	 * @brief shutdown logger subsystem.
	 */
	void stop();

private:
	/** private constructor - singleton pattern */
	Logger() {}
	Logger(const Logger&);
	Logger& operator=(const Logger&);

	/** typedefs for a vector of type LogSink* */
	typedef std::vector<LogSink*> sink_t;
	typedef std::vector<LogSink*>::iterator sinkCI_t;

	/** vector of available logging sinks */
	sink_t m_sinks;

	/** queue for logging messages */
	WQueue<LogMessage*> m_logQueue;

	/** true if this instance is running */
	bool m_running;

};

#endif // LIBUTILS_LOGGER_H_

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

enum Areas  { bas=1, net=2, bus=4, all=7, Size_of_Areas=3 };
enum Level { error=0, event, trace, debug, Size_of_Level };

int calcAreas(const std::string areas);
int calcLevel(const std::string level);


class LogMessage
{

public:
	enum Status { Run, End };

	LogMessage(const int area, const int level, const std::string text, const Status status);

	~LogMessage() {}

	LogMessage(const LogMessage& src)
		: m_area(src.m_area), m_level(src.m_level), m_text(src.m_text),
		  m_status(src.m_status), m_time(src.m_time) {}

	void operator= (const LogMessage& src)
		{ m_area = src.m_area; m_level = src.m_level; m_text = src.m_text;
		  m_status = src.m_status; m_time = src.m_time; }

	int getArea() const { return (m_area); }
	int getLevel() const { return(m_level); }
	std::string getText() const { return (m_text.c_str()); }
	Status getStatus() const { return (m_status); }
	std::string getTime() const { return (m_time.c_str()); }

private:
	int m_area;
	int m_level;
	std::string m_text;
	Status m_status;
	std::string m_time;

};

enum Type { Console, Logfile };

class LogSink : public Thread
{

public:
	LogSink(const int areas, const int level, const Type type, const char* name)
		: m_areas(areas), m_level(level), m_type(type), m_name(name) {}

	virtual ~LogSink() {}

	void addMessage(const LogMessage& message);

	void* run();

	int getAreas() const { return (m_areas); }
	void setAreas(const int& areas) { m_areas = areas; }

	int getLevel() const { return (m_level); }
	void setLevel(const int& level) { m_level = level; }

	Type getType() const { return (m_type); }
	const char* getName() const { return (m_name.c_str()); }

protected:
	WQueue<LogMessage*> m_queue;

private:
	int m_areas;
	int m_level;
	Type m_type;
	std::string m_name;

	virtual void write(const LogMessage& message) const = 0;

};

class LogConsole : public LogSink
{

public:
	LogConsole(const int areas, const int level, const char* name)
		: LogSink(areas, level, Console, name), m_instance(++m_numInstance)
		{ this->start(name); }

	~LogConsole() {}

private:
	const int m_instance;
	static int m_numInstance;

	void write(const LogMessage& message) const;

};

class LogFile : public LogSink
{

public:
	LogFile(const int areas, const int level, const char* name, const char* filename)
		: LogSink(areas, level, Logfile, name), m_filename(filename), m_instance(++m_numInstance)
		{ this->start(name); }

	~LogFile() {}

private:
	std::string m_filename;
	const int m_instance;
	static int m_numInstance;

	void write(const LogMessage& message) const;

};

class LogInstance : public Thread
{

public:
	static LogInstance& Instance();

	~LogInstance();

	LogInstance& operator+= (LogSink* sink);
	LogInstance& operator-= (const LogSink* sink);

	void log(const int area, const int level, const std::string& text, ...);

	int getNumberOfSinks() const { return(m_sinks.size()); }
	LogSink* getSink(const int Index) const { return(m_sinks[Index]); }

	void* run();

	void stop();

private:
	LogInstance() {}
	LogInstance(const LogInstance&);
	LogInstance& operator= (const LogInstance&);

	typedef std::vector<LogSink*> sink_t;
	typedef std::vector<LogSink*>::iterator sinkCI_t;

	sink_t m_sinks;

	WQueue<LogMessage*> m_messages;

	bool m_running;

};

#endif // LIBUTILS_LOGGER_H_

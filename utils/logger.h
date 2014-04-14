/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include "wqueue.h"
#include "thread.h"
#include <string>
#include <functional>
#include <algorithm>
#include <vector>
#include <cstdarg>

enum Area  { bas=1, net=2, bus=4, cyc=8, all=15, Size_of_Area=4 };
enum Level { error=0, event, trace, debug, Size_of_Level };

class LogMessage
{
	
public:
	enum Status { Run, End };
	
	LogMessage(const Area area, const Level level, const std::string text, const Status status);
	
	~LogMessage() {}

	LogMessage(const LogMessage& src)
		: m_area(src.m_area), m_level(src.m_level), m_text(src.m_text),
		  m_status(src.m_status), m_time(src.m_time) {}

	void operator= (const LogMessage& src)
		{ m_area = src.m_area; m_level = src.m_level; m_text = src.m_text;
		  m_status = src.m_status; m_time = src.m_time; }
	
	Area getArea() const { return (m_area); }
	Level getLevel() const { return(m_level); }
	std::string getText() const { return (m_text.c_str()); }
	Status getStatus() const { return (m_status); }
	std::string getTime() const { return (m_time.c_str()); }

private:
	Area m_area;
	Level m_level;
	std::string m_text;
	Status m_status;
	std::string m_time;

};

enum Type { Console, Logfile };

class LogSink : public Thread
{
	
public:
	LogSink(const unsigned char areas, const Level level, const Type type, const char* name)
		: m_areas(areas), m_level(level), m_type(type), m_name(name) {}

	virtual ~LogSink() {}
	
	void addMessage(const LogMessage& message);

	void* run();

	unsigned char getAreas() const { return (m_areas); }
	void setAreas(const unsigned char areas) { m_areas = areas; }
	
	Level getLevel() const { return (m_level); }
	void setLevel(const Level level) { m_level = level; }
	
	Type getType() const { return (m_type); }
	const char* getName() const { return (m_name.c_str()); }

protected:
	WQueue<LogMessage*> m_queue;

private:
	unsigned char m_areas;
	Level m_level;
	Type m_type;
	std::string m_name;
	
	virtual void write(const LogMessage& message) const = 0;

};

class LogConsole : public LogSink
{
	
public:
	LogConsole(const unsigned char areas, const Level level, const char* name)
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
	LogFile(const unsigned char areas, const Level level, const char* name, const char* filename)
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

	void log(const Area area, const Level level, const std::string& text, ...);

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

#endif // LOGGER_H_

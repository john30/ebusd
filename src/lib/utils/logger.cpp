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

#include "logger.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <sys/time.h>
#include <unistd.h>

/** static char array with logging area names */
static const char* AreaNames[Size_of_Areas] = { "bas", "net", "bus", "cyc" };

/** static char array with logging level names */
static const char* LevelNames[Size_of_Level] = { "error", "event", "trace", "debug" };

/** inline function of log2 */
inline double Log2(double n) { return log(n) / log(2); }

int calcAreas(const std::string areas)
{
	int m_areas = 0;

	// prepare data
	std::string token;
	std::istringstream stream(areas);
	std::vector<std::string> cmd;

	while (std::getline(stream, token, ',') != 0)
		cmd.push_back(token);

	for (std::vector<std::string>::iterator it = cmd.begin() ; it != cmd.end(); ++it)
		for (int i = 0; i < Size_of_Areas; i++) {
			if (strcasecmp("ALL", it->c_str()) == 0)
				return (pow(2, (int)Size_of_Areas) - 1);

			if (strcasecmp(AreaNames[i], it->c_str()) == 0)
				m_areas += pow(2, i);
		}

	return m_areas;
}

int calcLevel(const std::string level)
{
	int m_level = event;
	for (int i = 0; i < Size_of_Level; i++)
		if (strcasecmp(LevelNames[i], level.c_str()) == 0)
			return i;

	return m_level;
}


LogMessage::LogMessage(const int area, const int level, const std::string text, const bool running)
	: m_area(area), m_level(level), m_text(text), m_running(running)
{
	char time[24];
	struct timeval tv;
	struct timezone tz;
	struct tm* tm;

	gettimeofday(&tv, &tz);
	tm = localtime(&tv.tv_sec);

	sprintf(&time[0], "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec/1000);

	m_time = std::string(time);
}



void LogSink::addMessage(const LogMessage& message)
{
	LogMessage* tmp = new LogMessage(LogMessage(message));
	m_logQueue.add((tmp));
}

void* LogSink::run()
{
	while (1) {
		LogMessage* message = m_logQueue.remove();
			if (message->isRunning() == false) {
				delete message;
				while (m_logQueue.size() == true) {
					LogMessage* message = m_logQueue.remove();
					write(*message);
					delete message;
				}
				return NULL;
			}

		write(*message);
		delete message;
	}
	return NULL;
}



void LogConsole::write(const LogMessage& message) const
{
	std::cout << message.getTime() << " ["
		  << AreaNames[(int)Log2(message.getArea())] << " "
		  << LevelNames[message.getLevel()] << "] "
		  << message.getText() << std::endl;
}



void LogFile::write(const LogMessage& message) const
{
	std::fstream file(m_file.c_str(), std::ios::out | std::ios::app);

	if (file.is_open() == true) {
		file << message.getTime() << " ["
		     << AreaNames[(int)Log2(message.getArea())] << " "
		     << LevelNames[message.getLevel()] << "] "
		     << message.getText() << std::endl;
		file.close();
	}
}



Logger& Logger::Instance()
{
	static Logger instance;
	return (instance);
}

Logger::~Logger()
{
	while (m_sinks.empty() == false)
		*this -= *(m_sinks.begin());
}

Logger& Logger::operator+=(LogSink* sink)
{
	sinkCI_t itEnd = m_sinks.end();
	sinkCI_t it = std::find(m_sinks.begin(), itEnd, sink);

	if (it == itEnd)
		m_sinks.push_back(sink);

	return (*this);
}

Logger& Logger::operator-=(const LogSink* sink)
{
	sinkCI_t itEnd = m_sinks.end();
	sinkCI_t it = std::find(m_sinks.begin(), itEnd, sink);

	if (it == itEnd)
		return (*this);

	m_sinks.erase(it);

	delete (sink);

	return (*this);
}

void Logger::log(const int area, const int level, const std::string& data, ...)
{
	if (m_running == true) {
		char* tmp;
		va_list ap;
		va_start(ap, data);

		if (vasprintf(&tmp, data.c_str(), ap) != -1) {
			std::string buffer(tmp);
			m_logQueue.add(new LogMessage(area, level, buffer));
		}

		va_end(ap);
		free(tmp);
	}

}

void* Logger::run()
{
	m_running = true;

	while (m_running == true) {
		LogMessage* message = m_logQueue.remove();

		sinkCI_t iter = m_sinks.begin();

		for (; iter != m_sinks.end(); ++iter) {
			if (*iter != 0) {

				if (((*iter)->getAreas() & message->getArea()
				&& (*iter)->getLevel() >= message->getLevel())
				&& message->isRunning() == true) {
					(*iter)->addMessage(*message);
				}
				else if (message->isRunning() == false) {
					(*iter)->addMessage(*message);
					m_running = false;
				}


			}
		}

		delete message;

	}
	return NULL;
}

void Logger::stop()
{
	m_logQueue.add(new LogMessage(LogMessage(bas, error, "", false)));
	usleep(100000);
}

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
#include <ctime>
#include <sys/time.h>
#include <unistd.h>

using namespace std;

/** static char array with logging area names */
static const char* AreaNames[Size_of_Areas] = { "bas", "net", "bus", "upd" };

/** static char array with logging level names */
static const char* LevelNames[Size_of_Level] = { "error", "event", "trace", "debug" };

int calcAreaMask(const string areas)
{
	int mask = 0;

	// prepare data
	string token;
	istringstream stream(areas);
	vector<string> cmd;

	while (getline(stream, token, ',') != 0)
		cmd.push_back(token);

	for (vector<string>::iterator it = cmd.begin() ; it != cmd.end(); ++it)
		for (int i = 0; i < Size_of_Areas; i++) {
			if (strcasecmp("ALL", it->c_str()) == 0)
				return (1 << (int)Size_of_Areas) - 1;

			if (strcasecmp(AreaNames[i], it->c_str()) == 0)
				mask |= 1 << i;
		}

	return mask;
}

LevelType calcLevel(const string level)
{
	for (int i = 0; i < Size_of_Level; i++)
		if (strcasecmp(LevelNames[i], level.c_str()) == 0)
			return (LevelType)i;

	return event;
}


LogMessage::LogMessage(const int area, const int level, const string text)
	: m_area(area), m_level(level), m_text(text)
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

	m_time = string(time);
}


LogSink::~LogSink()
{
	m_logQueue.add(NULL);

	Thread::join();

	while (m_logQueue.size() > 0) {
		LogMessage* message = m_logQueue.remove(false);
		delete message;
	}
}

void LogSink::addMessage(const LogMessage& message)
{
	m_logQueue.add(new LogMessage(LogMessage(message)));
}

void LogSink::run()
{
	while (isRunning() == true) {
		LogMessage* message = m_logQueue.remove();
		if (message == NULL)
			break;

		write(*message);
		delete message;
	}
}



void LogConsole::write(const LogMessage& message) const
{
	cout << message.getTime() << " ["
		  << AreaNames[(int)message.getArea()] << " "
		  << LevelNames[message.getLevel()] << "] "
		  << message.getText() << endl;
}



void LogFile::write(const LogMessage& message) const
{
	fstream file(m_file.c_str(), ios::out | ios::app);

	if (file.is_open() == true) {
		file << message.getTime() << " ["
		     << AreaNames[(int)message.getArea()] << " "
		     << LevelNames[message.getLevel()] << "] "
		     << message.getText() << endl;
		file.close();
	}
}



Logger& Logger::Instance()
{
	static Logger instance;
	return instance;
}

Logger::~Logger()
{
	if (m_sink != NULL) {
		delete m_sink;
		m_sink = NULL;
	}
}

Logger& Logger::operator+=(LogSink* sink)
{
	if (m_sink != NULL)
		delete m_sink;
	m_sink = sink;
	return *this;
}

Logger& Logger::operator-=(const LogSink* sink)
{
	if (sink != NULL && sink == m_sink) {
		delete m_sink;
		m_sink = NULL;
	}
	return *this;
}

void Logger::setAreaMask(const int& areaMask)
{
	if (m_sink != NULL)
		m_sink->setAreaMask(areaMask);
}

void Logger::setLevel(const int& level)
{
	if (m_sink != NULL)
		m_sink->setLevel(level);
}

bool Logger::hasSink(const int area, const int level)
{
	if (m_sink != NULL) {
		if (((m_sink->getAreaMask() & (1 << area)) != 0
		&& m_sink->getLevel() >= level)) {
			return true;
		}
	}
	return false;
}

void Logger::log(const int area, const int level, const string& data, ...)
{
	if (m_direct == true || isRunning() == true) {
		char* tmp;
		va_list ap;
		va_start(ap, data);

		if (vasprintf(&tmp, data.c_str(), ap) != -1) {
			string buffer(tmp);
			LogMessage* message = new LogMessage(area, level, buffer);
			if (m_direct == true)
				handleMessage(message);
			else
				m_logQueue.add(message);
		}

		va_end(ap);
		free(tmp);
	}

}

void Logger::handleMessage(LogMessage* message) {
	if (message == NULL)
		return;
	if (m_sink != NULL) {
		if (((m_sink->getAreaMask() & (1 << message->getArea())) != 0
		&& m_sink->getLevel() >= message->getLevel())) {
			m_sink->addMessage(*message);
		}
	}
	delete message;
}

bool Logger::start(const char* name)
{
	m_direct = false;
	return Thread::start(name);
}

void Logger::run()
{

	while (isRunning() == true) {
		LogMessage* message = m_logQueue.remove();
		if (message == NULL)
			break;
		handleMessage(message);
	}

}

void Logger::stop()
{
	m_logQueue.add(NULL);
	usleep(100000);
	Thread::stop();
}

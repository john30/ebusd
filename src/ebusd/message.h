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

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <string>


class Message
{

public:
	Message(const std::string data, void* source = NULL) : m_data(data), m_source(source) {}
	Message(const Message& src) : m_data(src.m_data), m_source(src.m_source) {}

	std::string getData() const { return m_data; }
	void* getSource() const { return m_source; }

private:
	std::string m_data;
	void* m_source;

};


#endif // MESSAGE_H_

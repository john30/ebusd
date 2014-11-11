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

/** forward declaration for class Connection */
class Connection;

/**
 * @brief class for data/message transfer between connection and baseloop
 */
class Message
{

public:
	/**
	 * @brief constructs a new instance with message and source client address
	 * @param data from client
	 * @param connection to return result to correct client
	 */
	Message(const std::string data, Connection* connection=NULL)
		: m_data(data), m_connection(connection) {}

	/**
	 * @brief copy constructor.
	 * @param src message object for copy
	 */
	Message(const Message& src) : m_data(src.m_data), m_connection(src.m_connection) {}

	/**
	 * @brief data from client
	 * @return data string
	 */
	std::string getData() const { return m_data; }

	/**
	 * @brief original connection
	 * @return pointer to connection
	 */
	Connection* getConnection() const { return m_connection; }

private:
	/** the data/message string */
	std::string m_data;
	/** the source connection */
	Connection* m_connection;

};


#endif // MESSAGE_H_

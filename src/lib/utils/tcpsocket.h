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

#ifndef LIBUTILS_TCPSOCKET_H_
#define LIBUTILS_TCPSOCKET_H_

#include <unistd.h>
#include <string>

class TCPSocket
{

public:
	friend class TCPClient;
	friend class TCPServer;

	~TCPSocket() { close(m_sfd); }

	ssize_t recv(char* buffer, size_t len) { return read(m_sfd, buffer, len); }
	ssize_t send(const char* buffer, size_t len) { return write(m_sfd, buffer, len); }

	int getPort() const { return m_port; }
	std::string getIP() const { return m_ip; }

	int getFD() const { return m_sfd; }
	bool isValid();

private:
	int m_sfd;
	int m_port;
	std::string  m_ip;

	TCPSocket(int sfd, struct sockaddr_in* address);

};

class TCPClient
{

public:
	TCPSocket* connect(const std::string& server, const int& port);

private:

};

class TCPServer
{

public:
	TCPServer(const int port, const std::string address)
		: m_lfd(0), m_port(port), m_address(address), m_listening(false) {}

	~TCPServer() { if (m_lfd > 0) {close(m_lfd);} }

	int start();
	TCPSocket* newSocket();

	int getFD() const { return m_lfd; }

private:
	int m_lfd;
	int m_port;
	std::string m_address;
	bool m_listening;

};

#endif // LIBUTILS_TCPSOCKET_H_


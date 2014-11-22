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

using namespace std;

/**
 * @brief class for low level tcp socket operations. (open, close, send, receive).
 */
class TCPSocket
{

public:
	/** grant access for friend classes */
	friend class TCPClient;
	friend class TCPServer;

	/**
	 * @brief destructor.
	 */
	~TCPSocket() { close(m_sfd); }

	/**
	 * @brief write bytes to opened file descriptor.
	 * @param buffer data to send.
	 * @param len number of bytes to send.
	 * @return number of written bytes or -1 if an error has occured.
	 */
	ssize_t send(const char* buffer, size_t len) { return write(m_sfd, buffer, len); }

	/**
	 * @brief read bytes from opened file descriptor.
	 * @param buffer for received bytes.
	 * @param len size of the receive buffer.
	 * @return number of read bytes or -1 if an error has occured.
	 */
	ssize_t recv(char* buffer, size_t len) { return read(m_sfd, buffer, len); }

	/**
	 * @brief returns the tcp port.
	 * @return the tcp port.
	 */
	int getPort() const { return m_port; }

	/**
	 * @brief returns the ip address.
	 * @return the ip address.
	 */
	string getIP() const { return m_ip; }

	/**
	 * @brief returns the file descriptor.
	 * @return the file descriptor.
	 */
	int getFD() const { return m_sfd; }

	/**
	 * @brief returns status of file descriptor.
	 * @return true if file descriptor is valid.
	 */
	bool isValid();

private:
	/** file descriptor from tcp socket */
	int m_sfd;

	/** port of tcp socket */
	int m_port;

	/** ip address of tcp socket */
	string  m_ip;

	/**
	 * @brief private constructor, limited access only for friend classes.
	 * @param sfd the file desctriptor of tcp socket.
	 * @param address struct which holds the ip address.
	 */
	TCPSocket(int sfd, struct sockaddr_in* address);

};

/**
 * @brief class to initiate a tcp socket connection to a listening server.
 */
class TCPClient
{

public:
	/**
	 * @brief initiate a tcp socket connection to a listening server.
	 * @param server the server name or ip address to connect.
	 * @param port the tcp port.
	 * @return pointer to an opened tcp socket.
	 */
	TCPSocket* connect(const string& server, const int& port);

};

/**
 * @brief class for a tcp based network server.
 */
class TCPServer
{

public:
	/**
	 * @brief creates a new instance of a listening tcp server.
	 * @param port the tcp port.
	 * @param address the ip address.
	 */
	TCPServer(const int port, const string address)
		: m_lfd(0), m_port(port), m_address(address), m_listening(false) {}

	/**
	 * @brief destructor.
	 */
	~TCPServer() { if (m_lfd > 0) {close(m_lfd);} }

	/**
	 * @brief start listening of tcp socket.
	 * @return result of low level functions.
	 */
	int start();

	/**
	 * @brief accept an incomming tcp connection and create a local tcp socket for communication.
	 * @return pointer to an opened tcp socket.
	 */
	TCPSocket* newSocket();

	/**
	 * @brief returns the file descriptor.
	 * @return the file descriptor.
	 */
	int getFD() const { return m_lfd; }

private:
	/** file descriptor from listening tcp socket */
	int m_lfd;

	/** listening tcp port */
	int m_port;

	/** listening tcp socket ip address */
	string m_address;

	/** true if object is already listening */
	bool m_listening;

};

#endif // LIBUTILS_TCPSOCKET_H_


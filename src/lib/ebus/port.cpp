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

#include "port.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

bool Device::isOpen()
{
	if (isValid() == false)
		m_open = false;

	return m_open;
}

bool Device::isValid()
{
	if (m_noDeviceCheck == false) {
		int port;

		if (ioctl(m_fd, TIOCMGET, &port) == -1) {
			closeDevice();
			m_open = false;
			return false;
		}
	}

	return true;
}

ssize_t Device::sendBytes(const unsigned char* buffer, size_t nbytes)
{
	if (isValid() == false)
		return -1; // TODO RESULT_ERR_DEVICE

	// write bytes to device
	return write(m_fd, buffer, nbytes);
}

ssize_t Device::recvBytes(const long timeout, size_t maxCount)
{
	if (isValid() == false)
		return -1; // TODO RESULT_ERR_DEVICE

	if (timeout > 0) {
		int ret, nfds = 1;
		struct pollfd fds[nfds];
		struct timespec tdiff;

		// set select timeout
		tdiff.tv_sec = 0;
		tdiff.tv_nsec = timeout*1000;

		memset(fds, 0, sizeof(fds));

		fds[0].fd = m_fd;
		fds[0].events = POLLIN;

		ret = ppoll(fds, nfds, &tdiff, NULL);

		if (ret == -1) return -1; // TODO RESULT_ERR_DEVICE
		if (ret == 0) return -2; // TODO RESULT_ERR_TIMEOUT
	}

	if (maxCount > sizeof(m_buffer))
		maxCount = sizeof(m_buffer);

	// read bytes from device
	ssize_t nbytes = read(m_fd, m_buffer, maxCount);

	for (int i = 0; i < nbytes; i++)
		m_recvBuffer.push(m_buffer[i]);

	return nbytes;
}

unsigned char Device::getByte()
{
	unsigned char byte;

	if (m_recvBuffer.empty() == false) {
		byte = m_recvBuffer.front();
		m_recvBuffer.pop();

		return byte;
	}

	return 0;
}


void DeviceSerial::openDevice(const std::string deviceName, const bool noDeviceCheck)
{
	m_noDeviceCheck = noDeviceCheck;

	termios newSettings;

	m_open = false;

	// open file descriptor from serial device
	m_fd = open(deviceName.c_str(), O_RDWR | O_NOCTTY);

	if (m_fd < 0)
		return;

	// save current settings of serial device
	tcgetattr(m_fd, &m_oldSettings);

	memset(&newSettings, '\0', sizeof(newSettings));

	newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
	newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	newSettings.c_iflag |= IGNPAR;
	newSettings.c_oflag &= ~OPOST;

	newSettings.c_cc[VMIN]  = 1;
	newSettings.c_cc[VTIME] = 0;

	// empty device buffer
	tcflush(m_fd, TCIFLUSH);

	// activate new settings of serial device
	tcsetattr(m_fd, TCSANOW, &newSettings);

	// set serial device into blocking mode
	fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

	m_open = true;

}

void DeviceSerial::closeDevice()
{
	if (m_open == true) {
		// empty device buffer
		tcflush(m_fd, TCIOFLUSH);

		// activate old settings of serial device
		tcsetattr(m_fd, TCSANOW, &m_oldSettings);

		// close file descriptor from serial device
		close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}


void DeviceNetwork::openDevice(const std::string deviceName, const bool noDeviceCheck)
{
	m_noDeviceCheck = noDeviceCheck;

	struct sockaddr_in sock;
	char* hostport;
	int ret;

	m_open = false;

	memset((char*) &sock, 0, sizeof(sock));

	hostport = strdup(deviceName.c_str());
	char* host = strtok(hostport, ":");
	char* port = strtok(NULL, ":");

	if (inet_addr(host) == INADDR_NONE) {
		struct hostent* he;

		he = gethostbyname(host);
		if (he == NULL)
			return;

		memcpy(&sock.sin_addr, he->h_addr_list[0], he->h_length);
	} else {
		ret = inet_aton(host, &sock.sin_addr);
		if (ret == 0)
			return;
	}

	sock.sin_family = AF_INET;
	sock.sin_port = htons(strtol(port, NULL, 10));

	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd < 0)
		return;

	ret = connect(m_fd, (struct sockaddr*) &sock, sizeof(sock));
	if (ret < 0)
		return;

	free(hostport);
	m_open = true;
}

void DeviceNetwork::closeDevice()
{
	if (m_open == true) {
		// close file descriptor from network device
		close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}


Port::Port(const std::string deviceName, const bool noDeviceCheck)
	: m_deviceName(deviceName), m_noDeviceCheck(noDeviceCheck)
{
	m_device = NULL;

	if (strchr(deviceName.c_str(), '/') == NULL &&
	    strchr(deviceName.c_str(), ':') != NULL)
		setType(NETWORK);
	else
		setType(SERIAL);
}

void Port::setType(const DeviceType type)
{
	if (m_device != NULL)
		delete m_device;

	switch (type) {
	case SERIAL:
		m_device = new DeviceSerial();
		break;
	case NETWORK:
		m_device = new DeviceNetwork();
		break;
	};
};


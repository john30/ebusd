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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "port.h"
#include "result.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef HAVE_PPOLL
#include <poll.h>
#endif

using namespace std;

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

result_t Device::send(const unsigned char value)
{
	if (isValid() == false)
		return RESULT_ERR_DEVICE;

	// write bytes to device
	return write(m_fd, &value, 1) == 1 ? RESULT_OK : RESULT_ERR_SEND;
}

result_t Device::recv(const long timeout, unsigned char& value)
{
	if (isValid() == false)
		return RESULT_ERR_DEVICE;

	if (timeout > 0) {
		int ret;
		struct timespec tdiff;

		// set select timeout
		tdiff.tv_sec = 0;
		tdiff.tv_nsec = timeout*1000;

#ifdef HAVE_PPOLL
		int nfds = 1;
		struct pollfd fds[nfds];

		memset(fds, 0, sizeof(fds));

		fds[0].fd = m_fd;
		fds[0].events = POLLIN;

		ret = ppoll(fds, nfds, &tdiff, NULL);
#else
#ifdef HAVE_PSELECT
		fd_set readfds;

		FD_ZERO(&readfds);
		FD_SET(m_fd, &readfds);

		ret = pselect(m_fd + 1, &readfds, NULL, NULL, &tdiff, NULL);
#else
		ret = 1; // ignore timeout if neither ppoll nor pselect are available
#endif
#endif
		if (ret == -1) return RESULT_ERR_DEVICE;
		if (ret == 0) return RESULT_ERR_TIMEOUT;
	}

	// directly read byte from device
	ssize_t nbytes = read(m_fd, &value, 1);
	if (nbytes == 0)
		return RESULT_ERR_EOF;

	return nbytes < 0 ? RESULT_ERR_DEVICE : RESULT_OK;
}


result_t DeviceSerial::openDevice(const string deviceName, const bool noDeviceCheck)
{
	m_noDeviceCheck = noDeviceCheck;
	struct termios newSettings;
	m_open = false;

	// open file descriptor
	m_fd = open(deviceName.c_str(), O_RDWR | O_NOCTTY);

	if (m_fd < 0 || isatty(m_fd) == 0)
		return RESULT_ERR_NOTFOUND;

	// save current settings
	tcgetattr(m_fd, &m_oldSettings);

	// create new settings
	memset(&newSettings, '\0', sizeof(newSettings));

	newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
	newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // non-canonical mode
	newSettings.c_iflag |= IGNPAR; // ignore parity errors
	newSettings.c_oflag &= ~OPOST;

	// non-canonical mode: read() blocks until at least one byte is available
	newSettings.c_cc[VMIN]  = 1;
	newSettings.c_cc[VTIME] = 0;

	// empty device buffer
	tcflush(m_fd, TCIFLUSH);

	// activate new settings of serial device
	tcsetattr(m_fd, TCSAFLUSH, &newSettings);

	// set serial device into blocking mode
	fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

	m_open = true;
	return RESULT_OK;
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


result_t DeviceNetwork::openDevice(const string deviceName, const bool noDeviceCheck)
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
			return RESULT_ERR_NOTFOUND;

		memcpy(&sock.sin_addr, he->h_addr_list[0], he->h_length);
	}
	else {
		ret = inet_aton(host, &sock.sin_addr);
		if (ret == 0)
			return RESULT_ERR_NOTFOUND;
	}

	sock.sin_family = AF_INET;
	sock.sin_port = htons(strtol(port, NULL, 10));

	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd < 0)
		return RESULT_ERR_GENERIC_IO;

	ret = connect(m_fd, (struct sockaddr*) &sock, sizeof(sock));
	if (ret < 0)
		return RESULT_ERR_GENERIC_IO;

	free(hostport);
	m_open = true;

	return RESULT_OK;
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


Port::Port(const string deviceName, const bool noDeviceCheck,
		const bool logRaw, void (*logRawFunc)(const unsigned char byte, bool received),
		const bool dumpRaw, const char* dumpRawFile, const long dumpRawMaxSize)
	: m_deviceName(deviceName), m_noDeviceCheck(noDeviceCheck),
	  m_logRaw(logRaw), m_logRawFunc(logRawFunc),
	  m_dumpRawFile(dumpRawFile), m_dumpRawMaxSize(dumpRawMaxSize),
	  m_dumpRawFileSize(0)
{
	m_device = NULL;

	if (strchr(deviceName.c_str(), '/') == NULL &&
	    strchr(deviceName.c_str(), ':') != NULL)
		setType(dt_network);
	else
		setType(dt_serial);

	m_dumpRaw = false;

	setDumpRaw(dumpRaw); // open fstream if necessary
}

result_t Port::send(const unsigned char value)
{
	result_t ret = m_device->send(value);
	if (ret == RESULT_OK && m_logRaw == true && m_logRawFunc != NULL)
		(*m_logRawFunc)(value, false);
	return ret;
}

result_t Port::recv(const long timeout, unsigned char& value)
{
	result_t ret = m_device->recv(timeout, value);
	if (ret == RESULT_OK) {
		if (m_logRaw == true && m_logRawFunc != NULL) {
			(*m_logRawFunc)(value, true);
		}

		if (m_dumpRaw == true && m_dumpRawStream.is_open() == true) {
			m_dumpRawStream.write((char*)&value, 1);
			m_dumpRawFileSize++;
			if ((m_dumpRawFileSize%1024) == 0)
				m_dumpRawStream.flush();

			if (m_dumpRawFileSize >= m_dumpRawMaxSize * 1024) {
				string oldfile = m_dumpRawFile + ".old";
				if (rename(m_dumpRawFile.c_str(), oldfile.c_str()) == 0) {
					m_dumpRawStream.close();
					m_dumpRawStream.open(m_dumpRawFile.c_str(), ios::out | ios::binary | ios::app);
					m_dumpRawFileSize = 0;
				}
			}
		}
	}

	return ret;
}

void Port::setDumpRaw(bool dumpRaw)
{
	if (dumpRaw == m_dumpRaw)
		return;

	m_dumpRaw = dumpRaw;

	if (dumpRaw == false)
		m_dumpRawStream.close();
	else {
		m_dumpRawStream.open(m_dumpRawFile.c_str(), ios::out | ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

void Port::setDumpRawFile(const string& dumpFile) {
	if (dumpFile == m_dumpRawFile)
		return;

	m_dumpRawStream.close();
	m_dumpRawFile = dumpFile;

	if (m_dumpRaw == true) {
		m_dumpRawStream.open(m_dumpRawFile.c_str(), ios::out | ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

void Port::setType(const DeviceType type)
{
	if (m_device != NULL)
		delete m_device;

	switch (type) {
	case dt_serial:
		m_device = new DeviceSerial();
		break;
	case dt_network:
		m_device = new DeviceNetwork();
		break;
	};
};


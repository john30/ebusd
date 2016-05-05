/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2016 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBEBUS_DEVICE_H_
#define LIBEBUS_DEVICE_H_

#include <unistd.h>
#include <termios.h>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <netdb.h>
#include "result.h"

/** @file device.h
 * Classes providing access to the eBUS.
 *
 * A @a Device is either a @a SerialDevice directly connected to a local tty
 * port or a remote @a NetworkDevice handled via a TCP socket. It allows to
 * send and receive bytes to/from the eBUS while optionally dumping the data
 * to a file and/or forwarding it to a logging function.
 */

using namespace std;

/**
 * The base class for accessing an eBUS.
 */
class Device
{

public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param checkDevice whether to regularly check the device availability (only for serial devices).
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 * @param logRawFunc the function to call for logging raw data, or NULL.
	 */
	Device(const char* name, const bool checkDevice, const bool readOnly, const bool initialSend,
		void (*logRawFunc)(const unsigned char byte, bool received))
		: m_name(name), m_checkDevice(checkDevice), m_readOnly(readOnly), m_initialSend(initialSend), m_fd(-1),
		  m_logRaw(false), m_logRawFunc(logRawFunc),
		  m_dumpRaw(false), m_dumpRawFile(NULL), m_dumpRawMaxSize(0), m_dumpRawStream(), m_dumpRawFileSize(0) {}

	/**
	 * Destructor.
	 */
	virtual ~Device();

	/**
	 * Factory method for creating a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param checkDevice whether to regularly check the device availability (only for serial devices).
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 * @param logRawFunc the function to call for logging raw data, or NULL.
	 * @return the new @a Device, or NULL on error.
	 * Note: the caller needs to free the created instance.
	 */
	static Device* create(const char* name, const bool checkDevice=true, const bool readOnly=false, const bool initialSend=false,
		void (*logRawFunc)(const unsigned char byte, bool received)=NULL);

	/**
	 * Get the transfer latency of this device.
	 * @return the transfer latency in microseconds.
	 */
	virtual unsigned int getLatency() const { return 0; }

	/**
	 * Open the file descriptor.
	 * @return the @a result_t code.
	 */
	virtual result_t open() = 0; // abstract

	/**
	 * Close the file descriptor if opened.
	 */
	virtual void close();

	/**
	 * Write a single byte to the device.
	 * @param value the byte value to write.
	 * @return the @a result_t code.
	 */
	result_t send(const unsigned char value);

	/**
	 * Read a single byte from the device.
	 * @param timeout maximum time to wait for the byte in microseconds, or 0 for infinite.
	 * @param value the reference in which the received byte value is stored.
	 * @return the result_t code.
	 */
	result_t recv(const long timeout, unsigned char& value);

	/**
	 * Get whether logging of raw data is enabled.
	 * @return whether logging of raw data is enabled.
	 */
	bool getLogRaw() { return m_logRaw; }

	/**
	 * Enable or disable logging of raw data.
	 * @param logRaw true to enable logging of raw data, false to disable it.
	 */
	void setLogRaw(bool logRaw=true) { m_logRaw = logRaw; }

	/**
	 * Get whether dumping of raw data to a file is enabled.
	 * @return whether dumping of raw data to a file is enabled.
	 */
	bool getDumpRaw() { return m_dumpRaw; }

	/**
	 * Enable or disable dumping of raw data to a file.
	 * @param dumpRaw true to enable dumping of raw data to a file, false to disable it.
	 */
	void setDumpRaw(bool dumpRaw=true);

	/**
	 * Set the name of the file to dump raw data to.
	 * @param dumpFile the name of the file to dump raw data to.
	 */
	void setDumpRawFile(const char* dumpFile);

	/**
	 * Set the maximum size of a file to dump raw data to.
	 * @param maxSize the maximum size of a file to dump raw data to.
	 */
	void setDumpRawMaxSize(const long maxSize) { m_dumpRawMaxSize = maxSize; }

	/**
	 * Return the device name.
	 * @return the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 */
	const char* getName() { return m_name; }

	/**
	 * Return whether the device is opened and available.
	 * @return whether the device is opened and available.
	 */
	bool isValid();

protected:
	/**
	 * Check if the device is still available and close it if not.
	 */
	virtual void checkDevice() = 0; // abstract

	/**
	 * Check whether a byte is available immediately (without waiting).
	 * @return true when a a byte is available immediately.
	 */
	virtual bool available() { return false; }

	/**
	 * Write a single byte.
	 * @param value the byte value to write.
	 * @return the number of bytes written, or -1 on error.
	 */
	virtual ssize_t write(const unsigned char value) { return ::write(m_fd, &value, 1); }

	/**
	 * Read a single byte.
	 * @param value the reference in which the read byte value is stored.
	 * @return the number of bytes read, or -1 on error.
	 */
	virtual ssize_t read(unsigned char& value) { return ::read(m_fd, &value, 1); }

protected:
	/** the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network). */
	const char* m_name;

	/** whether to regularly check the device availability (only for serial devices). */
	const bool m_checkDevice;

	/** whether to allow read access to the device only. */
	const bool m_readOnly;

	/** whether to send an initial @a ESC symbol in @a open(). */
	const bool m_initialSend;

	/** the opened file descriptor, or -1. */
	int m_fd;

private:
	/** whether logging of raw data is enabled. */
	bool m_logRaw;

	/** the function to call for logging raw data, or NULL. */
	void (*m_logRawFunc)(const unsigned char byte, bool received);

	/** whether dumping of raw data to a file is enabled. */
	bool m_dumpRaw;

	/** the name of the file to dump raw data to. */
	const char* m_dumpRawFile;

	/** the maximum size of @a m_dumpFile, or 0 for infinite. */
	long m_dumpRawMaxSize;

	/** the @a ofstream for dumping raw data to. */
	ofstream m_dumpRawStream;

	/** the number of bytes already written to the @a m_dumpFile. */
	long m_dumpRawFileSize;

};

/**
 * The @a Device for directly connected serial interfaces (tty).
 */
class SerialDevice : public Device
{
public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param checkDevice whether to regularly check the device availability (only for serial devices).
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 * @param logRawFunc the function to call for logging raw data, or NULL.
	 */
	SerialDevice(const char* name, const bool checkDevice, const bool readOnly, const bool initialSend,
		void (*logRawFunc)(const unsigned char byte, bool received))
		: Device(name, checkDevice, readOnly, initialSend, logRawFunc) {}

	// @copydoc
	virtual result_t open();

	// @copydoc
	void close();

protected:
	// @copydoc
	virtual void checkDevice();

private:
	/** the previous settings of the device for restoring. */
	termios m_oldSettings;

};

/**
 * The @a Device for remote network interfaces.
 */
class NetworkDevice : public Device
{
public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param address the socket address of the device.
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 * @param logRawFunc the function to call for logging raw data, or NULL.
	 * @param udp true for UDP, false to TCP.
	 */
	NetworkDevice(const char* name, const struct sockaddr_in address, const bool readOnly, const bool initialSend,
		void (*logRawFunc)(const unsigned char byte, bool received), const bool udp)
		: Device(name, true, readOnly, initialSend, logRawFunc), m_address(address), m_udp(udp),
		  m_buffer(NULL), m_bufSize(0), m_bufLen(0), m_bufPos(0) {}

	// @copydoc
	virtual unsigned int getLatency() const { return 10000; }

	// @copydoc
	virtual result_t open();

protected:
	// @copydoc
	virtual void checkDevice();

	// @copydoc
	virtual bool available();

	// @copydoc
	virtual ssize_t write(const unsigned char value);

	// @copydoc
	virtual ssize_t read(unsigned char& value);

private:
	/** the socket address of the device. */
	const struct sockaddr_in m_address;

	/** true for UDP, false to TCP. */
	const bool m_udp;

	/** the buffer memory, or NULL. */
	unsigned char* m_buffer;

	/** the buffer size. */
	unsigned char m_bufSize;

	/** the buffer fill length. */
	unsigned char m_bufLen;

	/** the buffer read position. */
	unsigned char m_bufPos;

};

#endif // LIBEBUS_DEVICE_H_

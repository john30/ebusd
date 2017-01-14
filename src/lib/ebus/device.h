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

#ifndef LIB_EBUS_DEVICE_H_
#define LIB_EBUS_DEVICE_H_

#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include "result.h"

/** @file device.h
 * Classes providing access to the eBUS.
 *
 * A @a Device is either a @a SerialDevice directly connected to a local tty
 * port or a remote @a NetworkDevice handled via a TCP socket. It allows to
 * send and receive bytes to/from the eBUS while optionally dumping the data
 * to a file and/or forwarding it to a logging function.
 */

namespace ebusd {

/**
 * Interface for listening to data received on/sent to a device.
 */
class DeviceListener {
	public:
	/**
	 * Destructor.
	 */
	virtual ~DeviceListener() {}

	/**
	 * Listener method that is called when a data byte was received/sent.
	 * @param byte the data byte received/sent.
	 * @param received @a true on reception, @a false on sending.
	 */
	virtual void notifyDeviceData(const unsigned char byte, bool received) = 0; // abstract
};


/**
 * The base class for accessing an eBUS.
 */
class Device {
	public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param checkDevice whether to regularly check the device availability (only for serial devices).
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 */
	Device(const char* name, const bool checkDevice, const bool readOnly, const bool initialSend)
		: m_name(name), m_checkDevice(checkDevice), m_readOnly(readOnly), m_initialSend(initialSend), m_fd(-1),
		  m_listener(NULL) {}

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
	 * @return the new @a Device, or NULL on error.
	 * Note: the caller needs to free the created instance.
	 */
	static Device* create(const char* name, const bool checkDevice = true, const bool readOnly = false, const bool initialSend = false);

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
	result_t recv(const unsigned int timeout, unsigned char& value);

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

	/**
	 * Return whether to allow read access to the device only.
	 * @return whether to allow read access to the device only.
	 */
	bool isReadOnly() const { return m_readOnly; }

	/**
	 * Set the @a DeviceListener.
	 * @param listener the @a DeviceListener.
	 */
	void setListener(DeviceListener* listener) { m_listener = listener; }


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
	/** the @a DeviceListener, or NULL. */
	DeviceListener* m_listener;
};

/**
 * The @a Device for directly connected serial interfaces (tty).
 */
class SerialDevice : public Device {
	public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param checkDevice whether to regularly check the device availability (only for serial devices).
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 */
	SerialDevice(const char* name, const bool checkDevice, const bool readOnly, const bool initialSend)
		: Device(name, checkDevice, readOnly, initialSend) {}

	// @copydoc
	virtual result_t open();

	// @copydoc
	virtual void close();


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
class NetworkDevice : public Device {
	public:
	/**
	 * Construct a new instance.
	 * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
	 * @param address the socket address of the device.
	 * @param readOnly whether to allow read access to the device only.
	 * @param initialSend whether to send an initial @a ESC symbol in @a open().
	 * @param udp true for UDP, false to TCP.
	 */
	NetworkDevice(const char* name, const struct sockaddr_in address, const bool readOnly, const bool initialSend,
		const bool udp)
		: Device(name, true, readOnly, initialSend), m_address(address), m_udp(udp),
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

} // namespace ebusd

#endif // LIB_EBUS_DEVICE_H_

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

#ifndef LIBEBUS_PORT_H_
#define LIBEBUS_PORT_H_

#include <string>
#include <queue>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include "result.h"

/** \file port.h */

using namespace std;

/** available device types. */
enum DeviceType {
	dt_serial,  /*!< serial device */
	dt_network  /*!< network device */
};


/**
 * base class for input devices.
 */
class Device
{

public:
	/**
	 * constructs a new instance.
	 */
	Device() : m_fd(-1), m_open(false), m_noDeviceCheck(false) {}

	/**
	 * destructor.
	 */
	virtual ~Device() {}

	/**
	 * virtual open function for opening file descriptor
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 * @return the @a result_t code.
	 */
	virtual result_t openDevice(const string deviceName, const bool noDeviceCheck) = 0;

	/**
	 * virtual close function for closing opened file descriptor
	 */
	virtual void closeDevice() = 0;

	/**
	 * connection state of device.
	 * @return true if device is open
	 */
	bool isOpen();

	/**
	 * Write a single byte to opened file descriptor.
	 * @param value the value to send.
	 * @return the result_t code.
	 */
	result_t send(const unsigned char value);

	/**
	 * Read a single byte from opened file descriptor.
	 * @param timeout max time out for new input data [usec], or 0 for infinite.
	 * @param value the reference in which the value is stored.
	 * @return the result_t code.
	 */
	result_t recv(const long timeout, unsigned char& value);

protected:
	/** file descriptor from input device */
	int m_fd;

	/** true if device is opened */
	bool m_open;

	/** true if device check is disabled */
	bool m_noDeviceCheck;

private:
	/**
	 * system check if opened file descriptor is valid
	 * @return true if file descriptor is valid
	 */
	bool isValid();

};

/**
 * class for serial input device.
 */
class DeviceSerial : public Device
{

public:
	/**
	 * destructor.
	 */
	~DeviceSerial() { closeDevice(); }

	// @copydoc
	virtual result_t openDevice(const string deviceName, const bool noDeviceCheck);

	// @copydoc
	void closeDevice();

private:
	/** save settings from serial device */
	termios m_oldSettings;

};

/**
 * class for network input device.
 */
class DeviceNetwork : public Device
{

public:
	/**
	 * destructor.
	 */
	~DeviceNetwork() { closeDevice(); }

	// @copydoc
	virtual result_t openDevice(const string deviceName, const bool noDeviceCheck);

	// @copydoc
	void closeDevice();

private:

};

/**
 * wrapper class for class device.
 */
class Port
{

public:
	/**
	 * constructs a new instance and determine device type.
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 * @param logRaw whether logging of raw data is enabled.
	 * @param logRawFunc a function to call for logging raw data, or NULL.
	 * @param dumpRaw whether dumping of raw data to a file is enabled.
	 * @param dumpRawFile the name of the file to dump raw data to.
	 * @param dumpRawMaxSize the maximum size of @a m_dumpFile.
	 */
	Port(const string deviceName, const bool noDeviceCheck,
		const bool logRaw, void (*logRawFunc)(const unsigned char byte, bool received),
		const bool dumpRaw, const char* dumpRawFile, const long dumpRawMaxSize);

	/**
	 * destructor.
	 */
	~Port() { delete m_device; m_dumpRawStream.close(); }

	/**
	 * open device
	 */
	result_t open() { return m_device->openDevice(m_deviceName, m_noDeviceCheck); }

	/**
	 * close device
	 */
	void close() { m_device->closeDevice(); }

	/**
	 * connection state of device.
	 * @return true if device is open
	 */
	bool isOpen() { return m_device->isOpen(); }

	/**
	 * Write a single byte to opened file descriptor.
	 * @param value the value to send.
	 * @return the result_t code.
	 */
	result_t send(const unsigned char value);

	/**
	 * Read a single byte from opened file descriptor.
	 * @param timeout max time out for new input data [usec], or 0 for infinite.
	 * @param value the reference in which the value is stored.
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
	void setDumpRawFile(const string& dumpFile);

	/**
	 * Set the maximum size of a file to dump raw data to.
	 * @param maxSize the maximum size of a file to dump raw data to.
	 */
	void setDumpRawMaxSize(const long maxSize) { m_dumpRawMaxSize = maxSize; }

	/**
	 * Return the device name.
	 * @return the device name.
	 */
	const char* getDeviceName() { return m_deviceName.c_str(); }

private:
	/** the device name */
	const string m_deviceName;

	/** the device instance */
	Device* m_device;

	/** true if device check is disabled */
	bool m_noDeviceCheck;

	/** whether logging of raw data is enabled. */
	bool m_logRaw;

	/** a function to call for logging raw data, or NULL. */
	void (*m_logRawFunc)(const unsigned char byte, bool received);

	/** whether dumping of raw data to a file is enabled. */
	bool m_dumpRaw;

	/** the name of the file to dump raw data to. */
	string m_dumpRawFile;

	/** the maximum size of @a m_dumpFile. */
	long m_dumpRawMaxSize;

	/** the @a ofstream for dumping raw data to. */
	ofstream m_dumpRawStream;

	/** the number of bytes already written to the @a m_dumpFile. */
	long m_dumpRawFileSize;

	/**
	 * internal setter for device type.
	 * @param type of device
	 */
	void setType(const DeviceType type);

};

#endif // LIBEBUS_PORT_H_

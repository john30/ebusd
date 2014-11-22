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

/** available device types. */
enum DeviceType {
	dt_serial,  // serial device
	dt_network  // network device
};

/** max bytes write to bus. */
#define MAX_WRITE_SIZE 1

/** max size of receive buffer. */
#define MAX_READ_SIZE 100


/**
 * @brief base class for input devices.
 */
class Device
{

public:
	/**
	 * @brief constructs a new instance.
	 */
	Device() : m_fd(-1), m_open(false), m_noDeviceCheck(false) {}

	/**
	 * @brief destructor.
	 */
	virtual ~Device() {}

	/**
	 * @brief virtual open function for opening file descriptor
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 */
	virtual void openDevice(const std::string deviceName, const bool noDeviceCheck) = 0;

	/**
	 * @brief virtual close function for closing opened file descriptor
	 */
	virtual void closeDevice() = 0;

	/**
	 * @brief connection state of device.
	 * @return true if device is open
	 */
	bool isOpen();

	/**
	 * @brief sendBytes write bytes to opened file descriptor.
	 * @param buffer data to send.
	 * @param nbytes number of bytes to send.
	 * @return number of written bytes or -1 if an error has occured.
	 */
	ssize_t sendBytes(const unsigned char* buffer, size_t nbytes);

	/**
	 * @brief recvBytes read bytes from opened file descriptor.
	 * @param timeout time for new input data [usec].
	 * @param maxCount max size of receive buffer.
	 * @return number of read bytes or -1 if an error has occured.
	 */
	ssize_t recvBytes(const long timeout, size_t maxCount);

	/**
	 * @brief fetch first byte from receive buffer.
	 * @return first byte (raw)
	 */
	unsigned char getByte();

	/**
	 * @brief get current size (bytes) of the receive buffer.
	 * @return number of bytes in queued.
	 */
	ssize_t sizeRecvBuffer() const { return m_recvBuffer.size(); }

protected:
	/** if of file descriptor */
	int m_fd;

	/** state of device*/
	bool m_open;

	/** state of device check */
	bool m_noDeviceCheck;

	/** queue for received bytes */
	std::queue<unsigned char> m_recvBuffer;

	/** receive buffer */
	unsigned char m_buffer[MAX_READ_SIZE];

private:
	/**
	 * @brief system check if opened file descriptor is valid
	 * @return true if file descriptor is valid
	 */
	bool isValid();

};

/**
 * @brief class for serial input device.
 */
class DeviceSerial : public Device
{

public:
	/**
	 * @brief destructor.
	 */
	~DeviceSerial() { closeDevice(); }

	/**
	 * @brief open function for opening file descriptor
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 */
	void openDevice(const std::string deviceName, const bool noDeviceCheck);

	/**
	 * @brief close function for closing opened file descriptor
	 */
	void closeDevice();

private:
	/** save settings from serial device */
	termios m_oldSettings;

};

/**
 * @brief class for network input device.
 */
class DeviceNetwork : public Device
{

public:
	/**
	 * @brief destructor.
	 */
	~DeviceNetwork() { closeDevice(); }

	/**
	 * @brief open function for opening file descriptor
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 */
	void openDevice(const std::string deviceName, const bool noDeviceCheck);

	/**
	 * @brief close opened file descriptor
	 */
	void closeDevice();

private:

};

/**
 * @brief wrapper class for class device.
 */
class Port
{

public:
	/**
	 * @brief constructs a new instance and determine device type.
	 * @param deviceName to determine device type.
	 * @param noDeviceCheck en-/disable device check.
	 */
	Port(const std::string deviceName, const bool noDeviceCheck);

	/**
	 * @brief destructor.
	 */
	~Port() { delete m_device; }

	/**
	 * @brief open device
	 */
	void open() { m_device->openDevice(m_deviceName, m_noDeviceCheck); }

	/**
	 * @brief close device
	 */
	void close() { m_device->closeDevice(); }

	/**
	 * @brief connection state of device.
	 * @return true if device is open
	 */
	bool isOpen() { return m_device->isOpen(); }

	/**
	 * @brief send write bytes into opened file descriptor.
	 * @param buffer data to send.
	 * @param nbytes number of bytes to send.
	 * @return number of written bytes or -1 if an error has occured.
	 */
	ssize_t send(const unsigned char* buffer, size_t nbytes = MAX_WRITE_SIZE)
		{ return m_device->sendBytes(buffer, nbytes); }

	/**
	 * @brief recv read bytes from opened file descriptor.
	 * @param timeout max time out for new input data.
	 * @param maxCount max size of receive buffer.
	 * @return number of read bytes or -1 if an error has occured.
	 */
	ssize_t recv(const long timeout, size_t maxCount = MAX_READ_SIZE)
		{ return m_device->recvBytes(timeout, maxCount); }

	/**
	 * @brief fetch first byte from receive buffer.
	 * @return first byte (raw)
	 */
	unsigned char byte() { return m_device->getByte(); }

	/**
	 * @brief get current size (bytes) of the receive buffer.
	 * @return number of bytes in queued.
	 */
	ssize_t size() const { return m_device->sizeRecvBuffer(); }

private:
	/** the device name */
	std::string m_deviceName;

	/** the device instance */
	Device* m_device;

	/** true if device check is disabled */
	bool m_noDeviceCheck;

	/**
	 * @brief internal setter for device type.
	 * @param type of device
	 */
	void setType(const DeviceType type);

};

#endif // LIBEBUS_PORT_H_

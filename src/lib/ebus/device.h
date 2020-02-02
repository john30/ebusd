/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2018 John Baier <ebusd@ebusd.eu>
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
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/device.h
 * Classes providing access to the eBUS.
 *
 * A @a Device is either a @a SerialDevice directly connected to a local tty
 * port or a remote @a NetworkDevice handled via a TCP socket. It allows to
 * send and receive bytes to/from the eBUS while optionally dumping the data
 * to a file and/or forwarding it to a logging function.
 */

/** the arbitration state handled by @a Device. */
enum ArbitrationState {
  as_none,    //!< no arbitration in process
  as_start,   //!< arbitration start requested
  as_error,   //!< error while sending master address
  as_running, //!< arbitration currently running (master address sent, waiting for reception)
  as_lost,    //!< arbitration lost
  as_won,     //!< arbitration won
};

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
   * Listener method that is called when a symbol was received/sent.
   * @param symbol the received/sent symbol.
   * @param received @a true on reception, @a false on sending.
   */
  virtual void notifyDeviceData(symbol_t symbol, bool received) = 0;  // abstract

  /**
   * Called to notify a status message from the device.
   * @param error true for an error message, false for an info message.
   * @param message the message string.
   */
  virtual void notifyStatus(bool error, const char* message) = 0;  // abstract
};


/**
 * The base class for accessing an eBUS.
 */
class Device {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param checkDevice whether to regularly check the device availability.
   * @param readOnly whether to allow read access to the device only.
   * @param initialSend whether to send an initial @a ESC symbol in @a open().
   * @param enhancedProto whether to use the ebusd enhanced protocol.
   */
  Device(const char* name, bool checkDevice, bool readOnly, bool initialSend, bool enhancedProto=false);

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
   * @return the new @a Device, or nullptr on error.
   * Note: the caller needs to free the created instance.
   */
  static Device* create(const char* name, bool checkDevice = true, bool readOnly = false,
      bool initialSend = false);

  /**
   * Get the transfer latency of this device.
   * @return the transfer latency in microseconds.
   */
  virtual unsigned int getLatency() const { return 0; }

  /**
   * Open the file descriptor.
   * @return the @a result_t code.
   */
  virtual result_t open();

  /**
   * Has to be called by subclasses upon successful opening the device as last action in open().
   * @return the @a result_t code.
   */
  result_t afterOpen();

  /**
   * Close the file descriptor if opened.
   */
  virtual void close();

  /**
   * Write a single byte to the device.
   * @param value the byte value to write.
   * @return the @a result_t code.
   */
  result_t send(symbol_t value);

  /**
   * Read a single byte from the device.
   * @param timeout maximum time to wait for the byte in microseconds, or 0 for infinite.
   * @param value the reference in which the received byte value is stored.
   * @param arbitrationState the reference in which the current @a ArbitrationState is stored on success. When set to
   * @a as_won, the received byte is the master address that was successfully arbitrated with.
   * @return the result_t code.
   */
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState);

  /**
   * Start the arbitration with the specified master address. A subsequent request while an arbitration is currently in
   * checking state will always result in @a RESULT_ERR_DUPLICATE.
   * @param masterAddress the master address, or @a SYN to cancel a previous arbitration request.
   * @return the result_t code.
   */
  result_t startArbitration(symbol_t masterAddress);

  /**
   * Return whether the device is currently in arbitration.
   * @return true when the device is currently in arbitration.
   */
  bool isArbitrating() const { return m_arbitrationMaster != SYN; };

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
  virtual void checkDevice() = 0;  // abstract

  /**
   * Write a single byte.
   * @param value the byte value to write.
   * @param startArbitration true to start arbitration.
   * @return true on success, false on error.
   */
  virtual bool write(symbol_t value, bool startArbitration=false);

  /**
   * Check whether a symbol is available for reading immediately (without waiting).
   * @return true when a symbol is available for reading immediately.
   */
  virtual bool available();

  /**
   * Read a single byte.
   * @param value the reference in which the read byte value is stored.
   * @param isAvailable the result of the immediately preceding call to @a available().
   * @param arbitrationState the variable in which to store the received arbitration state (mandatory for enhanced proto).
   * @param incomplete the variable in which to store when a partial transfer needs another poll.
   * @return true on success, false on error.
   */
  virtual bool read(symbol_t* value, bool isAvailable, ArbitrationState* arbitrationState=nullptr, bool* incomplete=nullptr);

  /** the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network). */
  const char* m_name;

  /** whether to regularly check the device availability. */
  const bool m_checkDevice;

  /** whether to allow read access to the device only. */
  const bool m_readOnly;

  /** whether to send an initial @a ESC symbol in @a open(). */
  const bool m_initialSend;

  /** whether the device supports the ebusd enhanced protocol. */
  const bool m_enhancedProto;

  /** the opened file descriptor, or -1. */
  int m_fd;


 private:
  /** the @a DeviceListener, or nullptr. */
  DeviceListener* m_listener;

  /** the arbitration master address to send when in arbitration, or @a SYN. */
  symbol_t m_arbitrationMaster;

  /** true when in arbitration and the next received symbol needs to be checked against the sent master address. */
  bool m_arbitrationCheck;

  /** the read buffer. */
  symbol_t* m_buffer;

  /** the read buffer size (multiple of 4). */
  size_t m_bufSize;

  /** the read buffer fill length. */
  size_t m_bufLen;

  /** the read buffer read position. */
  size_t m_bufPos;
};


/**
 * The @a Device for directly connected serial interfaces (tty).
 */
class SerialDevice : public Device {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param checkDevice whether to regularly check the device availability.
   * @param readOnly whether to allow read access to the device only.
   * @param initialSend whether to send an initial @a ESC symbol in @a open().
   * @param enhancedProto whether to use the ebusd enhanced protocol.
   */
  SerialDevice(const char* name, bool checkDevice, bool readOnly, bool initialSend, bool enhancedProto=false)
    : Device(name, checkDevice, readOnly, initialSend, enhancedProto) {}

  // @copydoc
  result_t open() override;

  // @copydoc
  void close() override;


 protected:
  // @copydoc
  void checkDevice() override;


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
   * @param enhancedProto whether to use the ebusd enhanced protocol.
   */
  NetworkDevice(const char* name, const struct sockaddr_in& address, bool readOnly, bool initialSend,
    bool udp, bool enhancedProto=false)
    : Device(name, true, readOnly, initialSend, enhancedProto), m_address(address), m_udp(udp) {}

  /**
   * Destructor.
   */
  ~NetworkDevice() override {}

  // @copydoc
  unsigned int getLatency() const override { return 10000; }

  // @copydoc
  result_t open() override;


 protected:
  // @copydoc
  void checkDevice() override;


 private:
  /** the socket address of the device. */
  const struct sockaddr_in m_address;

  /** true for UDP, false to TCP. */
  const bool m_udp;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_H_

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2025 John Baier <ebusd@ebusd.eu>
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

#include <cstdint>
#include <string>
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/device.h
 * Classes providing access to the eBUS.
 *
 * A @a Device allows to send and receive data to/from a local or remote eBUS
 * device while optionally dumping the data to a file and/or forwarding it to
 * a logging function.
 */

/** the arbitration state handled by @a Device. */
enum ArbitrationState {
  as_none,     //!< no arbitration in process
  as_start,    //!< arbitration start requested
  as_error,    //!< error while sending master address
  as_running,  //!< arbitration currently running (master address sent, waiting for reception)
  as_lost,     //!< arbitration lost
  as_timeout,  //!< arbitration timed out
  as_won,      //!< arbitration won
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
   * Listener method that is called when symbols were received from/sent to eBUS.
   * @param data the received/sent data.
   * @param len the length of received/sent data.
   * @param received @a true on reception, @a false on sending.
   */
  virtual void notifyDeviceData(const symbol_t* data, size_t len, bool received) = 0;  // abstract

  /**
   * Called to notify a status message from the device.
   * @param error true for an error message, false for an info message.
   * @param message the message string.
   */
  virtual void notifyDeviceStatus(bool error, const char* message) = 0;  // abstract
};


/**
 * The base class for accessing an eBUS.
 */
class Device {
 protected:
  /**
   * Construct a new instance.
   */
  Device()
  : m_listener(nullptr) {
  }

 public:
  /**
   * Destructor.
   */
  virtual ~Device() {
  }

  /**
   * Get the device name.
   * @return the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  virtual const char* getName() const = 0;  // abstract

  /**
   * Set the @a DeviceListener.
   * @param listener the @a DeviceListener.
   */
  void setListener(DeviceListener* listener) { m_listener = listener; }

  /**
   * Format device infos in plain text.
   * @param output the @a ostringstream to append the infos to.
   * @param verbose whether to add verbose infos.
   * @param prefix true for the synchronously retrievable prefix, false for the potentially asynchronous suffix.
   */
  virtual void formatInfo(ostringstream* output, bool verbose, bool prefix) = 0;  // abstract

  /**
   * Format device infos in JSON format.
   * @param output the @a ostringstream to append the infos to.
   */
  virtual void formatInfoJson(ostringstream* output) {}

  /**
   * @return whether the device supports checking for version updates.
   */
  virtual bool supportsUpdateCheck() const { return false; }

  /**
   * Open the file descriptor.
   * @return the @a result_t code.
   */
  virtual result_t open() = 0;  // abstract

  /**
   * Return whether the device is opened and available.
   * @return whether the device is opened and available.
   */
  virtual bool isValid() = 0;  // abstract

  /**
   * Write a single byte to the device.
   * @param value the byte value to write.
   * @return the @a result_t code.
   */
  virtual result_t send(symbol_t value) = 0;  // abstract

  /**
   * Read a single byte from the device.
   * @param timeout maximum time to wait for the byte in milliseconds, or 0 for infinite.
   * @param value the reference in which the received byte value is stored.
   * @param arbitrationState the reference in which the current @a ArbitrationState is stored on success. When set to
   * @a as_won, the received byte is the master address that was successfully arbitrated with.
   * @return the result_t code.
   */
  virtual result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) = 0;  // abstract

  /**
   * Start the arbitration with the specified master address. A subsequent request while an arbitration is currently in
   * checking state will always result in @a RESULT_ERR_DUPLICATE.
   * @param masterAddress the master address, or @a SYN to cancel a previous arbitration request.
   * @return the result_t code.
   */
  virtual result_t startArbitration(symbol_t masterAddress) = 0;  // abstract

  /**
   * Return whether the device is currently in arbitration.
   * @return true when the device is currently in arbitration.
   */
  virtual bool isArbitrating() const = 0;  // abstract

  /**
   * Cancel a running arbitration.
   * @param arbitrationState the reference in which @a as_error is stored when cancelled.
   * @return true if it was cancelled, false if not.
   */
  virtual bool cancelRunningArbitration(ArbitrationState* arbitrationState) = 0;  // abstract

 protected:
  /** the @a DeviceListener, or nullptr. */
  DeviceListener* m_listener;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_H_

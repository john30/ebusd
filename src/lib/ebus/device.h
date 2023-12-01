/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2023 John Baier <ebusd@ebusd.eu>
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

#include <string>
#include "lib/ebus/result.h"
#include "lib/ebus/transport.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/device.h
 * Classes providing access to the eBUS.
 *
 * A @a Device allows to send and receive data to/from a local or remote eBUS
 * device while optionally dumping the data to a file and/or forwarding it to
 * a logging function.
 * The data transport itself is handled by a @a Transport instance.
 */

/** the arbitration state handled by @a CharDevice. */
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
class Device : public TransportListener {
 protected:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit Device(Transport* transport)
  : m_transport(transport), m_listener(nullptr) {
  }

 public:
  /**
   * Destructor.
   */
  virtual ~Device() {
    if (m_transport) {
      delete m_transport;
      m_transport = nullptr;
    }
  }

  /**
   * Get the device name.
   * @return the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  const char* getName() const { return m_transport->getName(); }

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
  virtual void formatInfo(ostringstream* output, bool verbose, bool prefix) {
    if (prefix) {
      *output << m_transport->getName() << ", " << m_transport->getTransportInfo();
    } else if (!m_transport->isValid()) {
      *output << ", invalid";
    }
  }

  /**
   * Format device infos in JSON format.
   * @param output the @a ostringstream to append the infos to.
   */
  virtual void formatInfoJson(ostringstream* output) const {}

  /**
   * @return whether the device supports checking for version updates.
   */
  virtual bool supportsUpdateCheck() const { return false; }

  // @copydoc
  virtual result_t notifyTransportStatus(bool opened) {
    m_listener->notifyDeviceStatus(!opened, opened ? "transport opened" : "transport closed");
    return RESULT_OK;
  }

  // @copydoc
  virtual void notifyTransportMessage(bool error, const char* message) {
    m_listener->notifyDeviceStatus(error, message);
  }

  /**
   * Open the file descriptor.
   * @return the @a result_t code.
   */
  virtual result_t open() { return m_transport->open(); }

  /**
   * Return whether the device is opened and available.
   * @return whether the device is opened and available.
   */
  virtual bool isValid() { return m_transport->isValid(); }

 protected:
  /** the @a Transport to use. */
  Transport* m_transport;

  /** the @a DeviceListener, or nullptr. */
  DeviceListener* m_listener;
};


class CharDevice : public Device {
 protected:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit CharDevice(Transport* transport)
  : Device(transport), m_arbitrationMaster(SYN), m_arbitrationCheck(0) {
    transport->setListener(this);
  }

 public:
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
  virtual result_t startArbitration(symbol_t masterAddress);

  /**
   * Return whether the device is currently in arbitration.
   * @return true when the device is currently in arbitration.
   */
  virtual bool isArbitrating() const { return m_arbitrationMaster != SYN; }

  /**
   * Cancel a running arbitration.
   * @param arbitrationState the reference in which @a as_error is stored when cancelled.
   * @return true if it was cancelled, false if not.
   */
  virtual bool cancelRunningArbitration(ArbitrationState* arbitrationState);

 protected:
  /** the arbitration master address to send when in arbitration, or @a SYN. */
  symbol_t m_arbitrationMaster;

  /** >0 when in arbitration and the next received symbol needs to be checked against the sent master address,
   * incremented with each received SYN when arbitration was not performed as expected and needs to be stopped. */
  size_t m_arbitrationCheck;
};


class PlainCharDevice : public CharDevice {
 public:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit PlainCharDevice(Transport* transport)
  : CharDevice(transport) {
  }

  // @copydoc
  result_t send(symbol_t value) override;

  // @copydoc
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) override;
};


class EnhancedCharDevice : public CharDevice {
 public:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit EnhancedCharDevice(Transport* transport)
  : CharDevice(transport), m_resetRequested(false),
    m_extraFatures(0), m_infoReqTime(0), m_infoLen(0), m_infoPos(0) {
  }

  // @copydoc
  void formatInfo(ostringstream* output, bool verbose, bool prefix) override;

  // @copydoc
  void formatInfoJson(ostringstream* output) const override;

  // @copydoc
  result_t send(symbol_t value) override;

  // @copydoc
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) override;

  // @copydoc
  result_t startArbitration(symbol_t masterAddress) override;

  // @copydoc
  virtual result_t notifyTransportStatus(bool opened);

  // @copydoc
  bool supportsUpdateCheck() const override { return m_extraFatures & 0x01; }

  /**
   * Check for a running extra infos request, wait for it to complete,
   * and then send a new request for extra infos to enhanced device.
   * @param infoId the ID of the info to request.
   * @param wait true to wait for a running request to complete, false to send right away.
   * @return @a RESULT_OK on success, or an error code otherwise.
   */
  result_t requestEnhancedInfo(symbol_t infoId, bool wait = true);

  /**
   * Get the enhanced device version.
   * @return @a a string with the version infos, or empty.
   */
  string getEnhancedVersion() const { return m_enhInfoVersion; }

  /**
   * Retrieve/update all extra infos from an enhanced device.
   * @return @a a string with the extra infos, or empty.
   */
  string getEnhancedInfos();

 private:
  /**
   * Cancel a running arbitration.
   * @param arbitrationState the reference in which @a as_error is stored when cancelled.
   * @return true if it was cancelled, false if not.
   */
  bool cancelRunningArbitration(ArbitrationState* arbitrationState);

  /**
   * Handle the already buffered enhanced data.
   * @param value the reference in which the read byte value is stored.
   * @param arbitrationState the variable in which to store the current/received arbitration state.
   * @return the @a result_t code, especially RESULT_CONTINE if the value was set and more data is available immediately.
   */
  result_t handleEnhancedBufferedData(const uint8_t* data, size_t len, symbol_t* value,
  ArbitrationState* arbitrationState);

  /**
   * Called when reception of an info ID was completed.
   */
  void notifyInfoRetrieved();

  /** whether the reset of the device was already requested. */
  bool m_resetRequested;

  /** the extra features supported by the device. */
  symbol_t m_extraFatures;

  /** the time of the last info request. */
  time_t m_infoReqTime;

  /** the info buffer expected length. */
  size_t m_infoLen;

  /** the info buffer write position. */
  size_t m_infoPos;

  /** the info buffer. */
  symbol_t m_infoBuf[16+1];

  /** a string describing the enhanced device version. */
  string m_enhInfoVersion;

  /** a string describing the enhanced device temperature. */
  string m_enhInfoTemperature;

  /** a string describing the enhanced device supply voltage. */
  string m_enhInfoSupplyVoltage;

  /** a string describing the enhanced device bus voltage. */
  string m_enhInfoBusVoltage;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_H_

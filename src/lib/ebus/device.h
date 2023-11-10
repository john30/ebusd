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

#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <string>
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

/** the transfer latency of the network device [ms]. */
#define NETWORK_LATENCY_MS 30

/** the extra transfer latency to take into account for enhanced protocol. */
#define ENHANCED_LATENCY_MS 10

/** the latency of the host [ms]. */
#if defined(__CYGWIN__) || defined(_WIN32)
#define HOST_LATENCY_MS 20
#else
#define HOST_LATENCY_MS 10
#endif

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
   * Listener method that is called when a symbol was received from/sent to eBUS.
   * @param symbol the received/sent symbol.
   * @param received @a true on reception, @a false on sending.
   */
  virtual void notifyDeviceData(symbol_t symbol, bool received) = 0;  // abstract

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
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  explicit Device(const char* name);

 public:
  /**
   * Destructor.
   */
  virtual ~Device() { }

  /**
   * Factory method for creating a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param extraLatency the extra bus transfer latency in milliseconds.
   * @param checkDevice whether to regularly check the device availability (only for serial devices).
   * @return the new @a Device, or nullptr on error.
   * Note: the caller needs to free the created instance.
   */
  static Device* create(const char* name, unsigned int extraLatency = 0, bool checkDevice = true);

  /**
   * Get the device name.
   * @return the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  const char* getName() const { return m_name; }

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
  virtual void formatInfo(ostringstream* output, bool verbose, bool prefix) = 0;

  /**
   * Format device infos in JSON format.
   * @param output the @a ostringstream to append the infos to.
   */
  virtual void formatInfoJson(ostringstream* output) = 0;

  /**
   * Open the file descriptor.
   * @return the @a result_t code.
   */
  virtual result_t open() = 0;

  /**
   * Has to be called by subclasses upon successful opening the device as last action in open().
   * @return the @a result_t code.
   */
  virtual result_t afterOpen() { return RESULT_OK; }

  /**
   * Close the file descriptor if opened.
   */
  virtual void close() = 0;

  /**
   * Return whether the device is opened and available.
   * @return whether the device is opened and available.
   */
  virtual bool isValid() = 0;

  /**
   * Write a single byte to the device.
   * @param value the byte value to write.
   * @return the @a result_t code.
   */
  virtual result_t send(symbol_t value) = 0;

  /**
   * Read a single byte from the device.
   * @param timeout maximum time to wait for the byte in milliseconds, or 0 for infinite.
   * @param value the reference in which the received byte value is stored.
   * @param arbitrationState the reference in which the current @a ArbitrationState is stored on success. When set to
   * @a as_won, the received byte is the master address that was successfully arbitrated with.
   * @return the result_t code.
   */
  virtual result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) = 0;

  /**
   * Start the arbitration with the specified master address. A subsequent request while an arbitration is currently in
   * checking state will always result in @a RESULT_ERR_DUPLICATE.
   * @param masterAddress the master address, or @a SYN to cancel a previous arbitration request.
   * @return the result_t code.
   */
  virtual result_t startArbitration(symbol_t masterAddress) = 0;

  /**
   * Return whether the device is currently in arbitration.
   * @return true when the device is currently in arbitration.
   */
  virtual bool isArbitrating() const = 0;

  /**
   * @return whether the device supports checking for version updates.
   */
  virtual bool supportsUpdateCheck() const = 0;

 protected:
  /** the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network). */
  const char* m_name;

  /** the @a DeviceListener, or nullptr. */
  DeviceListener* m_listener;
};


/** the possible enhanced protocol levels. */
enum EnhancedLevel {
  el_none = 0,   //!< non-enhanced
  el_basic = 1,  //!< enhanced basic
  el_speed = 2,  //!< enhanced high-speed
};


/**
 * The common base class for devices using a file descriptor.
 */
class FileDevice : public Device {
 protected:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param checkDevice whether to regularly check the device availability.
   * @param latency the bus transfer latency in milliseconds.
   * @param enhancedLevel whether to use the ebusd enhanced protocol.
   */
  FileDevice(const char* name, bool checkDevice, unsigned int latency,
      EnhancedLevel enhancedLevel);

 public:
  /**
   * Destructor.
   */
  virtual ~FileDevice();

  // @copydoc
  void formatInfo(ostringstream* output, bool verbose, bool prefix) override;

  // @copydoc
  void formatInfoJson(ostringstream* output) override;

  // @copydoc
  result_t open() override;

  // @copydoc
  result_t afterOpen() override;

  // @copydoc
  void close() override;

  // @copydoc
  bool isValid() override;

  // @copydoc
  result_t send(symbol_t value) override;

  // @copydoc
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) override;

  // @copydoc
  result_t startArbitration(symbol_t masterAddress) override;

  // @copydoc
  bool isArbitrating() const override { return m_arbitrationMaster != SYN; }

  /**
   * Get the transfer latency of this device.
   * @return the transfer latency in milliseconds.
   */
  virtual unsigned int getLatency() const { return m_latency; }

  /**
   * Return whether the device supports the ebusd enhanced protocol.
   * @return whether the device supports the ebusd enhanced protocol.
   */
  bool isEnhancedProto() const { return m_enhancedLevel != el_none; }

  /**
   * Get info about enhanced protocol support as string.
   * @return a @a string describing level of enhanced protocol support, or the empty string.
   */
  virtual string getEnhancedProtoInfo() const { return m_enhancedLevel ? "enhanced" : ""; }

  // @copydoc
  bool supportsUpdateCheck() const override { return m_enhancedLevel && m_extraFatures & 0x01; }

  /**
   * @return whether the device supports the ebusd enhanced protocol and supports querying extra infos.
   */
  bool supportsEnhancedInfos() const { return m_enhancedLevel && m_extraFatures & 0x01; }

  /**
   * Check for a running extra infos request, wait for it to complete,
   * and then send a new request for extra infos to enhanced device.
   * @param infoId the ID of the info to request.
   * @return @a RESULT_OK on success, or an error code otherwise.
   */
  result_t requestEnhancedInfo(symbol_t infoId);

  /**
   * Send a request for extra infos to enhanced device.
   * @param infoId the ID of the info to request.
   * @return @a RESULT_OK on success, or an error code otherwise.
   */
  result_t sendEnhancedInfoRequest(symbol_t infoId);

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

 protected:
  /**
   * Check if the device is still available and close it if not.
   */
  virtual void checkDevice() = 0;  // abstract

  /**
   * Cancel a running arbitration.
   * @param arbitrationState the reference in which @a as_error is stored when cancelled.
   * @return true if it was cancelled, false if not.
   */
  bool cancelRunningArbitration(ArbitrationState* arbitrationState);

  /**
   * Write a single byte.
   * @param value the byte value to write.
   * @param startArbitration true to start arbitration.
   * @return true on success, false on error.
   */
  virtual bool write(symbol_t value, bool startArbitration = false);

  /**
   * Check whether a symbol is available for reading immediately (without waiting).
   * @return true when a symbol is available for reading immediately.
   */
  virtual bool available();

  /**
   * Read a single byte.
   * @param value the reference in which the read byte value is stored.
   * @param isAvailable the result of the immediately preceding call to @a available().
   * @param arbitrationState the variable in which to store the current/received arbitration state (mandatory for enhanced proto).
   * @param incomplete the variable in which to store when a partial transfer needs another poll.
   * @return true on success, false on error.
   */
  virtual bool read(symbol_t* value, bool isAvailable, ArbitrationState* arbitrationState = nullptr,
                    bool* incomplete = nullptr);

  /** whether to regularly check the device availability. */
  const bool m_checkDevice;

  /** the bus transfer latency in milliseconds. */
  const unsigned int m_latency;

  /** whether the device supports the ebusd enhanced protocol. */
  const EnhancedLevel m_enhancedLevel;

  /** the opened file descriptor, or -1. */
  int m_fd;

  /** whether the reset of an enhanced device was already requested. */
  bool m_resetRequested;

 private:
  /**
   * Handle the already buffered enhanced data.
   * @param value the reference in which the read byte value is stored.
   * @param arbitrationState the variable in which to store the current/received arbitration state (mandatory for enhanced proto).
   * @return true if the value was set, false otherwise.
   */
  bool handleEnhancedBufferedData(symbol_t* value, ArbitrationState* arbitrationState);

  /**
   * Called when reception of an info ID was completed.
   */
  void notifyInfoRetrieved();

  /** the arbitration master address to send when in arbitration, or @a SYN. */
  symbol_t m_arbitrationMaster;

  /** >0 when in arbitration and the next received symbol needs to be checked against the sent master address,
   * incremented with each received SYN when arbitration was not performed as expected and needs to be stopped. */
  size_t m_arbitrationCheck;

  /** the read buffer. */
  symbol_t* m_buffer;

  /** the read buffer size (multiple of 4). */
  size_t m_bufSize;

  /** the read buffer fill length. */
  size_t m_bufLen;

  /** the read buffer read position. */
  size_t m_bufPos;

  /** the extra features supported by the device. */
  symbol_t m_extraFatures;

  /** the ID of the last requested info. */
  symbol_t m_infoId;

  /** the time of the last info request. */
  time_t m_infoReqTime;

  /** the info buffer expected length. */
  size_t m_infoLen;

  /** the info buffer write position. */
  size_t m_infoPos;

  /** the info buffer. */
  symbol_t m_infoBuf[16];

  /** a string describing the enhanced device version. */
  string m_enhInfoVersion;

  /** a string describing the enhanced device temperature. */
  string m_enhInfoTemperature;

  /** a string describing the enhanced device supply voltage. */
  string m_enhInfoSupplyVoltage;

  /** a string describing the enhanced device bus voltage. */
  string m_enhInfoBusVoltage;
};


/**
 * The @a Device for directly connected serial interfaces (tty).
 */
class SerialDevice : public FileDevice {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param checkDevice whether to regularly check the device availability.
   * @param extraLatency the extra bus transfer latency in milliseconds.
   * @param enhancedLevel whether to use the ebusd enhanced protocol.
   */
  SerialDevice(const char* name, bool checkDevice, unsigned int extraLatency,
               EnhancedLevel enhancedLevel)
    : FileDevice(name, checkDevice, extraLatency, enhancedLevel) {
  }

  // @copydoc
  string getEnhancedProtoInfo() const override {
    return m_enhancedLevel == el_speed ? "enhanced high speed" : FileDevice::getEnhancedProtoInfo();
  }

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
class NetworkDevice : public FileDevice {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param address the socket address of the device.
   * @param hostOrIp the host name or IP address of the device.
   * @param port the TCP or UDP port of the device.
   * @param extraLatency the extra bus transfer latency in milliseconds.
   * @param udp true for UDP, false to TCP.
   * @param enhancedLevel whether to use the ebusd enhanced protocol.
   */
  NetworkDevice(const char* name, const char* hostOrIp, uint16_t port, unsigned int extraLatency,
      bool udp, EnhancedLevel enhancedLevel)
    : FileDevice(name, true, NETWORK_LATENCY_MS+extraLatency, enhancedLevel),
    m_hostOrIp(hostOrIp), m_port(port), m_udp(udp) {}

  /**
   * Destructor.
   */
  ~NetworkDevice() override {
    if (m_hostOrIp) {
      free((void*)m_hostOrIp);
      m_hostOrIp = nullptr;
    }
  }

  // @copydoc
  result_t open() override;


 protected:
  // @copydoc
  void checkDevice() override;


 private:
  /** the host name or IP address of the device. */
  const char* m_hostOrIp;

  /** the TCP or UDP port of the device. */
  const uint16_t m_port;

  /** true for UDP, false to TCP. */
  const bool m_udp;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_H_

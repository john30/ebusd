/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_TRANSPORT_H_
#define LIB_EBUS_TRANSPORT_H_

#include <unistd.h>
#include <termios.h>
#include <string>
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/transport.h
 * Classes for low level transport to/from the eBUS device.
 *
 * A @a Transport is either a @a SerialTransport directly connected
 * to a local tty port or a remote @a NetworkTransport handled via a
 * socket.
 */

/** the transfer latency of the network device [ms]. */
#define NETWORK_LATENCY_MS 30

/** the latency of the host [ms]. */
#if defined(__CYGWIN__) || defined(_WIN32)
#define HOST_LATENCY_MS 20
#else
#define HOST_LATENCY_MS 10
#endif

/**
 * Interface for listening to data received on/sent to a @a Transport.
 */
class TransportListener {
 public:
  /**
   * Destructor.
   */
  virtual ~TransportListener() {}

  /**
   * Called to notify a status change from the @a Transport.
   * @param opened true when the transport was successfully opened, false when it was closed or open failed.
   * @return the result_t code (other than RESULT_OK if an extra open action was performed unsuccessfully).
   */
  virtual result_t notifyTransportStatus(bool opened) = 0;  // abstract

  /**
   * Called to notify a message from the @a Transport.
   * @param error true for an error message, false for an info message.
   * @param message the message string.
   */
  virtual void notifyTransportMessage(bool error, const char* message) = 0;  // abstract
};


/**
 * The base class for low level transport to/from the eBUS device.
 */
class Transport {
 protected:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  Transport(const char* name, unsigned int latency)
  : m_name(name), m_latency(latency), m_listener(nullptr) {}

 public:
  /**
   * Destructor.
   */
  virtual ~Transport() { }

  /**
   * Get the device name.
   * @return the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   */
  const char* getName() const { return m_name; }

  /**
   * Get the transfer latency of this device.
   * @return the transfer latency in milliseconds.
   */
  unsigned int getLatency() const { return m_latency; }

  /**
   * Get info about the transport as string.
   * @return a @a string describing the transport.
   */
  virtual string getTransportInfo() const = 0;  // abstract

  /**
   * Set the @a TransportListener.
   * @param listener the @a TransportListener.
   */
  void setListener(TransportListener* listener) { m_listener = listener; }

  /**
   * Open the transport.
   * @return the @a result_t code.
   */
  virtual result_t open() = 0;  // abstract

  /**
   * Close the device if opened.
   */
  virtual void close() = 0;  // abstract

  /**
   * Return whether the device is opened and available.
   * @return whether the device is opened and available.
   */
  virtual bool isValid() = 0;  // abstract

  /**
   * Write arbitrary data to the device.
   * @param data the data to send.
   * @param len the length of data.
   * @return the @a result_t code.
   */
  virtual result_t write(const uint8_t* data, size_t len) = 0;  // abstract

  /**
   * Read data from the device.
   * @param timeout maximum time to wait for the byte in milliseconds, or 0 for returning only already buffered data.
   * @param data pointer to a variable in which to put the received data.
   * @param len pointer to a variable in which to put the number of available bytes.
   * @return the @a result_t code.
   */
  virtual result_t read(unsigned int timeout, const uint8_t** data, size_t* len) = 0;  // abstract

  /**
   * Needs to be called after @a read() in order to mark all or parts of the available
   * bytes as consumed.
   * @param len the number of bytes consumed.
   */
  virtual void readConsumed(size_t len) = 0;  // abstract

 protected:
  /**
   * Internal method for opening the device. Called from @a open().
   * @return the @a result_t code.
   */
  virtual result_t openInternal() = 0;  // abstract

  /** the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network). */
  const char* m_name;

  /** the bus transfer latency in milliseconds. */
  const unsigned int m_latency;

  /** the @a TransportListener, or nullptr. */
  TransportListener* m_listener;
};


/**
 * The common base class for transport using a file descriptor.
 */
class FileTransport : public Transport {
 protected:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param latency the bus transfer latency in milliseconds.
   * @param checkDevice whether to regularly check the device availability.
   */
  FileTransport(const char* name, unsigned int latency, bool checkDevice);

 public:
  /**
   * Destructor.
   */
  virtual ~FileTransport();

  // @copydoc
  result_t open() override;

  // @copydoc
  void close() override;

  // @copydoc
  bool isValid() override;

  // @copydoc
  result_t write(const uint8_t* data, size_t len) override;

  // @copydoc
  result_t read(unsigned int timeout, const uint8_t** data, size_t* len) override;

  // @copydoc
  void readConsumed(size_t len) override;

 protected:
  /**
   * Check if the device is still available and close it if not.
   */
  virtual void checkDevice() = 0;  // abstract

  /** whether to regularly check the device availability. */
  const bool m_checkDevice;

  /** the opened file descriptor, or -1. */
  int m_fd;

 private:
  /** the receive buffer. */
  symbol_t* m_buffer;

  /** the receive buffer size (multiple of 4). */
  size_t m_bufSize;

  /** the receive buffer fill length. */
  size_t m_bufLen;
};


/**
 * The @a Transport for a directly connected serial interface (tty).
 */
class SerialTransport : public FileTransport {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param extraLatency the extra bus transfer latency in milliseconds.
   * @param checkDevice whether to regularly check the device availability.
   * @param speed 0 for normal speed, 1 for 4x speed, or 2 for 48x speed.
   */
  SerialTransport(const char* name, unsigned int extraLatency, bool checkDevice, uint8_t speed)
    : FileTransport(name, extraLatency, checkDevice), m_speed(speed) {
  }

  // @copydoc
  string getTransportInfo() const override {
    return m_speed ? (m_speed == 1 ? "serial speed" : "serial high speed") : "serial";
  }

  // @copydoc
  result_t openInternal() override;

  // @copydoc
  void close() override;


 protected:
  // @copydoc
  void checkDevice() override;


 private:
  /** the previous settings of the device for restoring. */
  termios m_oldSettings;

  /** 0 for normal speed, 1 for 4x speed, or 2 for 48x speed. */
  const int m_speed;
};


/**
 * The @a Transport for a remote network interface.
 */
class NetworkTransport : public FileTransport {
 public:
  /**
   * Construct a new instance.
   * @param name the device name (e.g. "/dev/ttyUSB0" for serial, "127.0.0.1:1234" for network).
   * @param extraLatency the extra bus transfer latency in milliseconds.
   * @param address the socket address of the device.
   * @param hostOrIp the host name or IP address of the device.
   * @param port the TCP or UDP port of the device.
   * @param udp true for UDP, false to TCP.
   */
  NetworkTransport(const char* name, unsigned int extraLatency, const char* hostOrIp, uint16_t port,
      bool udp)
    : FileTransport(name, NETWORK_LATENCY_MS+extraLatency, true),
    m_hostOrIp(hostOrIp), m_port(port), m_udp(udp) {}

  /**
   * Destructor.
   */
  ~NetworkTransport() override {
    if (m_hostOrIp) {
      free((void*)m_hostOrIp);
      m_hostOrIp = nullptr;
    }
  }

  // @copydoc
  string getTransportInfo() const override {
    return m_udp ? "UDP" : "TCP";
  }

  // @copydoc
  result_t openInternal() override;


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

#endif  // LIB_EBUS_TRANSPORT_H_

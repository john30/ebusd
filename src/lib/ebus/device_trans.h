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

#ifndef LIB_EBUS_DEVICE_TRANS_H_
#define LIB_EBUS_DEVICE_TRANS_H_

#include <cstdint>
#include <string>
#include "lib/ebus/device.h"
#include "lib/ebus/device_enhanced.h"
#include "lib/ebus/transport.h"
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/device_trans.h
 * Classes providing access to the eBUS via a @a Transport instance.
 */

/**
 * The base class for accessing an eBUS via a @a Transport instance.
 */
class BaseDevice : public Device, public TransportListener {
 protected:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit BaseDevice(Transport* transport)
  : Device(), m_transport(transport),
    m_arbitrationMaster(SYN), m_arbitrationCheck(0) {
    transport->setListener(this);
  }

 public:
  /**
   * Destructor.
   */
  virtual ~BaseDevice() {
    if (m_transport) {
      delete m_transport;
      m_transport = nullptr;
    }
  }

  // @copydoc
  virtual const char* getName() const { return m_transport->getName(); }

  // @copydoc
  virtual void formatInfo(ostringstream* output, bool verbose, bool prefix) {
    if (prefix) {
      *output << m_transport->getName() << ", " << m_transport->getTransportInfo();
    } else if (!m_transport->isValid()) {
      *output << ", invalid";
    }
  }

  // @copydoc
  virtual void formatInfoJson(ostringstream* output) {}

  // @copydoc
  virtual result_t notifyTransportStatus(bool opened) {
    m_listener->notifyDeviceStatus(!opened, opened ? "transport opened" : "transport closed");
    return RESULT_OK;
  }

  // @copydoc
  virtual void notifyTransportMessage(bool error, const char* message) {
    m_listener->notifyDeviceStatus(error, message);
  }

  // @copydoc
  virtual result_t open() { return m_transport->open(); }

  // @copydoc
  virtual bool isValid() { return m_transport->isValid(); }

  // @copydoc
  virtual result_t startArbitration(symbol_t masterAddress);

  // @copydoc
  virtual bool isArbitrating() const { return m_arbitrationMaster != SYN; }

  // @copydoc
  virtual bool cancelRunningArbitration(ArbitrationState* arbitrationState);

 protected:
  /** the @a Transport to use. */
  Transport* m_transport;

  /** the arbitration master address to send when in arbitration, or @a SYN. */
  symbol_t m_arbitrationMaster;

  /** >0 when in arbitration and the next received symbol needs to be checked against the sent master address,
   * incremented with each received SYN when arbitration was not performed as expected and needs to be stopped. */
  size_t m_arbitrationCheck;
};


class PlainDevice : public BaseDevice {
 public:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit PlainDevice(Transport* transport)
  : BaseDevice(transport) {
  }

  // @copydoc
  result_t send(symbol_t value) override;

  // @copydoc
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) override;
};


class EnhancedDevice : public BaseDevice, public EnhancedDeviceInterface {
 public:
  /**
   * Construct a new instance.
   * @param transport the @a Transport to use.
   */
  explicit EnhancedDevice(Transport* transport)
  : BaseDevice(transport), EnhancedDeviceInterface(), m_resetTime(0), m_resetRequested(false),
    m_extraFeatures(0), m_infoReqTime(0), m_infoLen(0), m_infoPos(0), m_enhInfoIsWifi(false),
    m_enhInfoIdRequestNeeded(false), m_enhInfoIdRequested(false) {
  }

  // @copydoc
  void formatInfo(ostringstream* output, bool verbose, bool prefix) override;

  // @copydoc
  void formatInfoJson(ostringstream* output) override;

  // @copydoc
  result_t send(symbol_t value) override;

  // @copydoc
  result_t recv(unsigned int timeout, symbol_t* value, ArbitrationState* arbitrationState) override;

  // @copydoc
  result_t startArbitration(symbol_t masterAddress) override;

  // @copydoc
  virtual result_t notifyTransportStatus(bool opened);

  // @copydoc
  bool supportsUpdateCheck() const override { return m_extraFeatures & 0x01; }

  // @copydoc
  virtual result_t requestEnhancedInfo(symbol_t infoId, bool wait = true);

  // @copydoc
  virtual string getEnhancedVersion() const { return m_enhInfoVersion; }

  // @copydoc
  virtual string getEnhancedInfos();

  // @copydoc
  virtual bool cancelRunningArbitration(ArbitrationState* arbitrationState);

 private:
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

  /** the time when the transport was resetted. */
  time_t m_resetTime;

  /** whether the reset of the device was already requested. */
  bool m_resetRequested;

  /** the extra features supported by the device. */
  symbol_t m_extraFeatures;

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

  /** whether the device is known to be connected via WIFI. */
  bool m_enhInfoIsWifi;

  /** whether the device ID request is needed. */
  bool m_enhInfoIdRequestNeeded;

  /** whether the device ID was already requested. */
  bool m_enhInfoIdRequested;

  /** a string with the ID of the enhanced device. */
  string m_enhInfoId;

  /** a string describing the enhanced device temperature. */
  string m_enhInfoTemperature;

  /** a string describing the enhanced device supply voltage. */
  string m_enhInfoSupplyVoltage;

  /** a string describing the enhanced device bus voltage. */
  string m_enhInfoBusVoltage;
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_TRANS_H_

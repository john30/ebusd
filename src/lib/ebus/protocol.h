/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2023 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_PROTOCOL_H_
#define LIB_EBUS_PROTOCOL_H_

#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/device.h"
#include "lib/utils/queue.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** @file lib/ebus/protocol.h
 * Classes, functions, and constants related to handling the eBUS protocol.
 */

/** the default time [ms] for retrieving a symbol from an addressed slave. */
#define SLAVE_RECV_TIMEOUT 15

/** the maximum allowed time [ms] for retrieving the AUTO-SYN symbol (45ms + 2*1,2% + 1 Symbol). */
#define SYN_TIMEOUT 51

/** the time [ms] for determining bus signal availability (AUTO-SYN timeout * 5). */
#define SIGNAL_TIMEOUT 250

/** the maximum duration [us] of a single symbol (Start+8Bit+Stop+Extra @ 2400Bd-2*1,2%). */
#define SYMBOL_DURATION_MICROS 4700

/** the maximum duration [ms] of a single symbol (Start+8Bit+Stop+Extra @ 2400Bd-2*1,2%). */
#define SYMBOL_DURATION 5

/** the maximum allowed time [ms] for retrieving back a sent symbol (2x symbol duration). */
#define SEND_TIMEOUT ((int)((2*SYMBOL_DURATION_MICROS+999)/1000))


/** settings for the eBUS protocol handler. */
typedef struct ebus_protocol_config {
  /** the own master address. */
  symbol_t ownAddress;
  /** whether to answer queries for the own master/slave address. */
  bool answer;
  /** the number of times a send is repeated due to lost arbitration. */
  unsigned int busLostRetries;
  /** the number of times a failed send is repeated (other than lost arbitration). */
  unsigned int failedSendRetries;
  /** the maximum time in milliseconds for bus acquisition. */
  unsigned int busAcquireTimeout;
  /** the maximum time in milliseconds an addressed slave is expected to acknowledge. */
  unsigned int slaveRecvTimeout;
  /** the number of AUTO-SYN symbols before sending is allowed after lost arbitration, or 0 for auto detection. */
  unsigned int lockCount;
  /** whether to enable AUTO-SYN symbol generation. */
  bool generateSyn;
} ebus_protocol_config_t;


class ProtocolHandler;

/**
 * Generic request for sending to and receiving from the bus.
 */
class BusRequest {
  friend class ProtocolHandler;

 public:
  /**
   * Constructor.
   * @param master the master data @a MasterSymbolString to send.
   * @param deleteOnFinish whether to automatically delete this @a BusRequest when finished.
   */
  BusRequest(const MasterSymbolString& master, bool deleteOnFinish)
    : m_master(master), m_busLostRetries(0),
      m_deleteOnFinish(deleteOnFinish) {}

  /**
   * Destructor.
   */
  virtual ~BusRequest() {}

  /**
   * @return the master data @a MasterSymbolString to send.
   */
  const MasterSymbolString& getMaster() const { return m_master; }

  /**
   * @return the number of times a send was repeated due to lost arbitration.
   */
  unsigned int getBusLostRetries() const { return m_busLostRetries; }

  /**
   * Increment the number of times a send was repeated due to lost arbitration.
   */
  void incrementBusLostRetries() { m_busLostRetries++; }

  /**
   * Reset the number of times a send was repeated due to lost arbitration.
   */
  void resetBusLostRetries() { m_busLostRetries = 0; }

  /**
   * @return whether to automatically delete this @a BusRequest when finished.
   */
  bool deleteOnFinish() const { return m_deleteOnFinish; }

  /**
   * Notify the request of the specified result.
   * @param result the result of the request.
   * @param slave the @a SlaveSymbolString received.
   * @return true if the request needs to be restarted.
   */
  virtual bool notify(result_t result, const SlaveSymbolString& slave) = 0;  // abstract


 protected:
  /** the master data @a MasterSymbolString to send. */
  const MasterSymbolString& m_master;

  /** the number of times a send was repeated due to lost arbitration. */
  unsigned int m_busLostRetries;

  /** whether to automatically delete this @a BusRequest when finished. */
  const bool m_deleteOnFinish;
};


/**
 * An active @a BusRequest that can be waited for.
 */
class ActiveBusRequest : public BusRequest {
  friend class ProtocolHandler;

 public:
  /**
   * Constructor.
   * @param master the master data @a MasterSymbolString to send.
   * @param slave reference to @a SlaveSymbolString for filling in the received slave data.
   */
  ActiveBusRequest(const MasterSymbolString& master, SlaveSymbolString* slave)
    : BusRequest(master, false), m_result(RESULT_ERR_NO_SIGNAL), m_slave(slave) {}

  /**
   * Destructor.
   */
  virtual ~ActiveBusRequest() {}

  // @copydoc
  bool notify(result_t result, const SlaveSymbolString& slave) override;


 private:
  /** the result of handling the request. */
  result_t m_result;

  /** reference to @a SlaveSymbolString for filling in the received slave data. */
  SlaveSymbolString* m_slave;
};


/**
 * Interface for listening to eBUS protocol data.
 */
class ProtocolListener {
 public:
  /**
   * Destructor.
   */
  virtual ~ProtocolListener() {}

  /**
   * Called to notify a status update from the protocol.
   * @param signal true when signal is acquired, false otherwise.
   */
  virtual void notifyProtocolStatus(bool signal) = 0;  // abstract

  /**
   * Called to notify a new valid seen address on the bus.
   * @param address the seen address.
   */
  virtual void notifyProtocolSeenAddress(symbol_t address) = 0;  // abstract

  /**
   * Listener method that is called when a message was sent or received.
   * @param sent true when the master part was actively sent, false if the whole message
   * was received only.
   * @param master the @a MasterSymbolString received.
   * @param slave the @a SlaveSymbolString received.
   */
  virtual void notifyProtocolMessage(bool sent, const MasterSymbolString& master,
    const SlaveSymbolString& slave) = 0;  // abstract

  /**
   * Listener method that is called when in answer mode and a message targeting ourself was received.
   * @param master the @a MasterSymbolString received.
   * @param slave the @a SlaveSymbolString for writing the response to.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t notifyProtocolAnswer(const MasterSymbolString& master,
    SlaveSymbolString* slave) = 0;  // abstract
};



/**
 * Handles input from and output to eBUS with respect to the eBUS protocol.
 */
class ProtocolHandler : public WaitThread {
 public:
  /**
   * Construct a new instance.
   * @param config the configuration to use.
   * @param device the @a Device instance for accessing the bus.
   * @param listener the @a ProtocolListener.
   */
  ProtocolHandler(const ebus_protocol_config_t config,
      Device* device, ProtocolListener* listener)
    : WaitThread(), m_config(config), m_device(device), m_listener(listener),
      m_reconnect(false),
      m_ownMasterAddress(config.ownAddress), m_ownSlaveAddress(getSlaveAddress(config.ownAddress)),
      m_addressConflict(false),
      m_masterCount(device->isReadOnly()?0:1),
      m_symbolLatencyMin(-1), m_symbolLatencyMax(-1), m_arbitrationDelayMin(-1),
      m_arbitrationDelayMax(-1), m_lastReceive(0),
      m_symPerSec(0), m_maxSymPerSec(0) {
    memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
  }

  /**
   * Destructor.
   */
  virtual ~ProtocolHandler() {
    join();
    BusRequest* req;
    while ((req = m_finishedRequests.pop()) != nullptr) {
      delete req;
    }
    while ((req = m_nextRequests.pop()) != nullptr) {
      if (req->m_deleteOnFinish) {
        delete req;
      }
    }
  }

  /**
   * Create a new instance.
   * @param config the configuration to use.
   * @param device the @a Device instance for accessing the bus.
   * @param listener the @a ProtocolListener.
   * @return the new ProtocolHandler, or @a nullptr on error.
   */
  static ProtocolHandler* create(const ebus_protocol_config_t config, Device* device, ProtocolListener* listener);

  /**
   * @return the own master address.
   */
  symbol_t getOwnMasterAddress() const { return m_ownMasterAddress; }

  /**
   * @return the own slave address.
   */
  symbol_t getOwnSlaveAddress() const { return m_ownSlaveAddress; }

  /**
   * @return @p true if answering queries for the own master/slave address (if not readonly).
   */
  bool isAnswering() const { return m_config.answer; }

  /**
   * @param address the address to check.
   * @return @p true when the address is the own master or slave address (if not readonly).
   */
  bool isOwnAddress(symbol_t address) const {
    return !m_device->isReadOnly() && (address == m_ownMasterAddress || address == m_ownSlaveAddress);
  }

  /**
   * @param address the own address to check for conflict or @a SYN for any.
   * @return @p true when an address conflict with any of the own addresses or the specified own address was detected.
   */
  bool isAddressConflict(symbol_t address) const {
    return m_addressConflict && (address == SYN || m_seenAddresses[address]);
  }

  /**
   * @return the maximum number of received symbols per second ever seen.
   */
  unsigned int getMaxSymPerSec() const { return m_maxSymPerSec; }

  /**
   * @return the @a Device instance for accessing the bus.
   */
  const Device* getDevice() const { return m_device; }

  /**
   * Clear stored values (e.g. scan results).
   */
  virtual void clear();

  /**
   * Inject a message from outside and treat it as regularly retrieved from the bus.
   * This may only be called before bus handling was actually started.
   * @param master the @a MasterSymbolString with the master data.
   * @param slave the @a SlaveSymbolString with the slave data.
   */
  virtual void injectMessage(const MasterSymbolString& master, const SlaveSymbolString& slave) = 0;  // abstract

  /**
   * Add a @a BusRequest to the internal queue and optionally wait for it to complete.
   * @param request the @a BusRequest to add.
   * @param wait true to wait for it to complete, false to return immediately.
   * @return true when it was not waited for or when it was completed.
   */
  virtual bool addRequest(BusRequest* request, bool wait);

  /**
   * Send a message on the bus and wait for the answer.
   * @param master the @a MasterSymbolString with the master data to send.
   * @param slave the @a SlaveSymbolString that will be filled with retrieved slave data.
   * @return the result code.
   */
  virtual result_t sendAndWait(const MasterSymbolString& master, SlaveSymbolString* slave);

  /**
   * Main thread entry.
   */
  virtual void run() = 0;  // abstract

  /**
   * Return true when a signal on the bus is available.
   * @return true when a signal on the bus is available.
   */
  virtual bool hasSignal() const = 0;  // abstract

  /**
   * Reconnect the device.
   */
  virtual void reconnect() { m_reconnect = true; }

  /**
   * Return the current symbol rate.
   * @return the number of received symbols in the last second.
   */
  unsigned int getSymbolRate() const { return m_symPerSec; }

  /**
   * Return the maximum seen symbol rate.
   * @return the maximum number of received symbols per second ever seen.
   */
  unsigned int getMaxSymbolRate() const { return m_maxSymPerSec; }

  /**
   * Return the minimal measured latency between send and receive of a symbol.
   * @return the minimal measured latency between send and receive of a symbol in milliseconds, -1 if not yet known.
   */
  int getMinSymbolLatency() const { return m_symbolLatencyMin; }

  /**
   * Return the maximal measured latency between send and receive of a symbol.
   * @return the maximal measured latency between send and receive of a symbol in milliseconds, -1 if not yet known.
   */
  int getMaxSymbolLatency() const { return m_symbolLatencyMax; }

  /**
   * Return the minimal measured delay between received SYN and sent own master address in microseconds.
   * @return the minimal measured delay between received SYN and sent own master address in microseconds, -1 if not yet known.
   */
  int getMinArbitrationDelay() const { return m_arbitrationDelayMin; }

  /**
   * Return the maximal measured delay between received SYN and sent own master address in microseconds.
   * @return the maximal measured delay between received SYN and sent own master address in microseconds, -1 if not yet known.
   */
  int getMaxArbitrationDelay() const { return m_arbitrationDelayMax; }

  /**
   * Return the number of masters already seen.
   * @return the number of masters already seen (including ebusd itself).
   */
  unsigned int getMasterCount() const { return m_masterCount; }


 protected:
  /**
   * Called to measure the latency between send and receive of a symbol.
   * @param sentTime the time the symbol was sent.
   * @param recvTime the time the symbol was received.
   */
  virtual void measureLatency(struct timespec* sentTime, struct timespec* recvTime);

  /**
   * Add a seen bus address.
   * @param address the seen bus address.
   * @return true if a conflict with the own addresses was detected, false otherwise.
   */
  virtual bool addSeenAddress(symbol_t address);

  /** the client configuration to use. */
  const ebus_protocol_config_t m_config;

  /** the @a Device instance for accessing the bus. */
  Device* m_device;

  /** the @a ProtocolListener. */
  ProtocolListener *m_listener;

  /** set to @p true when the device shall be reconnected. */
  bool m_reconnect;

  /** the own master address. */
  const symbol_t m_ownMasterAddress;

  /** the own slave address. */
  const symbol_t m_ownSlaveAddress;

  /** set to @p true once an address conflict with the own addresses was detected. */
  bool m_addressConflict;

  /** the number of masters already seen. */
  unsigned int m_masterCount;

  /** the minimal measured latency between send and receive of a symbol in milliseconds, -1 if not yet known. */
  int m_symbolLatencyMin;

  /** the maximal measured latency between send and receive of a symbol in milliseconds, -1 if not yet known. */
  int m_symbolLatencyMax;

  /**
   * the minimal measured delay between received SYN and sent own master address in microseconds,
   * -1 if not yet known.
   */
  int m_arbitrationDelayMin;

  /**
   * the maximal measured delay between received SYN and sent own master address in microseconds,
   * -1 if not yet known.
   */
  int m_arbitrationDelayMax;

  /** the time of the last received symbol, or 0 for never. */
  time_t m_lastReceive;

  /** the queue of @a BusRequests that shall be handled. */
  Queue<BusRequest*> m_nextRequests;

  /** the queue of @a BusRequests that are already finished. */
  Queue<BusRequest*> m_finishedRequests;

  /** the number of received symbols in the last second. */
  unsigned int m_symPerSec;

  /** the maximum number of received symbols per second ever seen. */
  unsigned int m_maxSymPerSec;

  /** the participating bus addresses seen so far. */
  bool m_seenAddresses[256];
};

}  // namespace ebusd

#endif  // LIB_EBUS_PROTOCOL_H_

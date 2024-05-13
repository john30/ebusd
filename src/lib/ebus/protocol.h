/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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
#include "lib/utils/rotatefile.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** @file lib/ebus/protocol.h
 * Classes, functions, and constants related to handling the eBUS protocol.
 */

/** the default time [ms] for retrieving a symbol from an addressed slave. */
#define SLAVE_RECV_TIMEOUT 15

/** the desired delay time [ms] for sending the AUTO-SYN symbol after last seen symbol. */
#define SYN_INTERVAL 40

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
  /** eBUS device string (serial device or [udp:]ip[:port]) with optional protocol prefix (enh: or ens:). */
  const char* device;
  /** whether to skip serial eBUS device test. */
  bool noDeviceCheck;
  /** whether to allow read access to the device only. */
  bool readOnly;
  /** extra transfer latency in ms. */
  unsigned int extraLatency;
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
  /** whether to send an initial escape symbol after connecting device. */
  bool initialSend;
} ebus_protocol_config_t;


/** the possible protocol states. */
enum ProtocolState {
  ps_noSignal,   //!< no signal on the bus
  ps_idle,       //!< idle (after @a SYN symbol)
  ps_idleSYN,    //!< idle (after sent SYN symbol in acting as SYN generator)
  ps_recv,       //!< receiving
  ps_send,       //!< sending
  ps_empty,      //!< idle, no more lock remaining, and no other request queued
};

/**
 * Return the string corresponding to the @a ProtocolState.
 * @param state the @a ProtocolState.
 * @return the string corresponding to the @a ProtocolState.
 */
const char* getProtocolStateCode(ProtocolState state);

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


/** the possible message directions. */
enum MessageDirection {
  md_recv,    //!< message received from bus
  md_send,    //!< message sent to bus
  md_answer,  //!< answered to a message received from bus
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
   * @param state the current protocol state.
   * @param result the error code reason for the state change, or @a RESULT_OK.
   */
  virtual void notifyProtocolStatus(ProtocolState state, result_t result) = 0;  // abstract

  /**
   * Called to notify a new valid seen address on the bus.
   * @param address the seen address.
   */
  virtual void notifyProtocolSeenAddress(symbol_t address) = 0;  // abstract

  /**
   * Listener method that is called when a message was sent or received.
   * @param direction the message direction.
   * @param master the @a MasterSymbolString received/sent.
   * @param slave the @a SlaveSymbolString received/sent or the answer passed to @a ProtocolHandler::setAnswer() with
   * the the length of the data part following the ID as master.
   */
  virtual void notifyProtocolMessage(MessageDirection direction, const MasterSymbolString& master,
    const SlaveSymbolString& slave) = 0;  // abstract
};



/**
 * Handles input from and output to eBUS with respect to the eBUS protocol.
 */
class ProtocolHandler : public WaitThread, public DeviceListener {
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
      m_listenerState(ps_noSignal), m_reconnect(false),
      m_ownMasterAddress(config.ownAddress), m_ownSlaveAddress(getSlaveAddress(config.ownAddress)),
      m_addressConflict(false),
      m_masterCount(config.readOnly ? 0 : 1),
      m_symbolLatencyMin(-1), m_symbolLatencyMax(-1), m_arbitrationDelayMin(-1),
      m_arbitrationDelayMax(-1), m_lastReceive(0),
      m_symPerSec(0), m_maxSymPerSec(0),
      m_logRawFile(nullptr), m_logRawEnabled(false), m_logRawBytes(false),
      m_logRawLastSymbol(SYN), m_dumpFile(nullptr) {
    memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
    device->setListener(this);
    m_logRawLastReceived = true;
    m_logRawLastSymbol = SYN;
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
    if (m_dumpFile) {
      delete m_dumpFile;
      m_dumpFile = nullptr;
    }
    if (m_logRawFile) {
      delete m_logRawFile;
      m_logRawFile = nullptr;
    }
    if (m_device != nullptr) {
      delete m_device;
      m_device = nullptr;
    }
  }

  /**
   * Create a new instance.
   * @param config the configuration to use.
   * @param listener the @a ProtocolListener.
   * @return the new ProtocolHandler, or @a nullptr on error.
   */
  static ProtocolHandler* create(const ebus_protocol_config_t config, ProtocolListener* listener);

  /**
   * Open the device.
   * @return the @a result_t code.
   */
  virtual result_t open();

  /**
   * Format device/protocol infos in plain text.
   * @param output the @a ostringstream to append the infos to.
   * @param verbose whether to add verbose infos.
   * @param noWait true to not wait for any response asynchronously and return immediately.
   */
  virtual void formatInfo(ostringstream* output, bool verbose, bool noWait);

  /**
   * Format device/protocol infos in JSON format.
   * @param output the @a ostringstream to append the infos to.
   */
  virtual void formatInfoJson(ostringstream* output) const;

  /**
   * @return whether to allow read access to the device only.
   */
  bool isReadOnly() const { return m_config.readOnly; }

  /**
   * @return the own master address.
   */
  symbol_t getOwnMasterAddress() const { return m_ownMasterAddress; }

  /**
   * @return the own slave address.
   */
  symbol_t getOwnSlaveAddress() const { return m_ownSlaveAddress; }

  /**
   * @return @p true if answering queries (if not readonly).
   */
  virtual bool isAnswering() const { return false; }

  /**
   * Add a message to be answered.
   * @param srcAddress the source address to limit to, or @a SYN for any.
   * @param dstAddress the destination address (either master or slave address).
   * @param pb the primary ID byte.
   * @param sb the secondary ID byte.
   * @param id optional further ID bytes.
   * @param idLen the length of the further ID bytes (maximum 4).
   * @param answer the sequence to respond when addressed as slave or the length of the data part following the ID as master.
   * @return @p true on success, @p false on error (e.g. invalid address, read only, or too long id).
   */
  virtual bool setAnswer(symbol_t srcAddress, symbol_t dstAddress, symbol_t pb, symbol_t sb, const symbol_t* id,
      size_t idLen, const SlaveSymbolString& answer) { return false; }

  /**
   * @return @p true if an answer was set for the destination address.
   */
  virtual bool hasAnswer(symbol_t dstAddress) const { return false; }

  /**
   * @param address the address to check.
   * @return @p true when the address is the own master or slave address (if not readonly).
   */
  bool isOwnAddress(symbol_t address) const {
    return !m_config.readOnly && (address == m_ownMasterAddress || address == m_ownSlaveAddress);
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
   * @return whether the device supports checking for version updates.
   */
  virtual bool supportsUpdateCheck() const { return m_device->supportsUpdateCheck(); }

  // @copydoc
  virtual void notifyDeviceData(const symbol_t* data, size_t len, bool received);

  // @copydoc
  void notifyDeviceStatus(bool error, const char* message) override;

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
   * @return the result code of adding the request (i.e. RESULT_OK when it was not waited for or when it was completed).
   */
  virtual result_t addRequest(BusRequest* request, bool wait);

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

  /**
   * Set the dump file to use.
   * @param dumpFile the dump file to use, or nullptr.
   * @param dumpSize the maximum file size.
   * @param dumpFlush true to early flush the file.
   */
  void setDumpFile(const char* dumpFile, unsigned int dumpSize, bool dumpFlush);

  /**
   * @return whether a dump file is set.
   */
  bool hasDumpFile() const { return m_dumpFile; }

  /**
   * Toggle dumping to file.
   * @return true if dumping is now enabled.
   */
  bool toggleDump();

  /**
   * Set the log raw data file to use.
   * @param logRawFile the log raw file to use, or nullptr.
   * @param logRawSize the maximum file size.
   */
  void setLogRawFile(const char* logRawFile, unsigned int logRawSize);

  /**
   * Toggle logging raw data.
   * @return true if logging raw data is now enabled.
   */
  bool toggleLogRaw(bool bytes);

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

  /** the configuration to use. */
  const ebus_protocol_config_t m_config;

  /** the @a Device instance for accessing the bus. */
  Device* m_device;

  /** the @a ProtocolListener. */
  ProtocolListener *m_listener;

  /** the last state the listener was informed with. */
  ProtocolState m_listenerState;

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

  /** the @a RotateFile for writing sent/received bytes in log format, or nullptr. */
  RotateFile* m_logRawFile;

  /** whether raw logging to @p logNotice is enabled (only relevant if m_logRawFile is nullptr). */
  bool m_logRawEnabled;

  /** whether to log raw bytes instead of messages with @a m_logRawEnabled. */
  bool m_logRawBytes;

  /** the buffer for building log raw message. */
  ostringstream m_logRawBuffer;

  /** true when the last byte in @a m_logRawBuffer was receive, false if it was sent. */
  bool m_logRawLastReceived;

  /** the last sent/received symbol.*/
  symbol_t m_logRawLastSymbol;

  /** the @a RotateFile for dumping received data, or nullptr. */
  RotateFile* m_dumpFile;
};

}  // namespace ebusd

#endif  // LIB_EBUS_PROTOCOL_H_

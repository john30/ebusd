/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2022 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_BUSHANDLER_H_
#define EBUSD_BUSHANDLER_H_

#include <pthread.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include "lib/ebus/message.h"
#include "lib/ebus/data.h"
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/device.h"
#include "lib/utils/queue.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** @file ebusd/bushandler.h
 * Classes, functions, and constants related to handling of symbols on the eBUS.
 *
 * The following table shows the possible states, symbols, and state transition
 * depending on the kind of message to send/receive:
 * @image html states.png "ebusd BusHandler states"
 */

using std::string;

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

/** the possible bus states. */
enum BusState {
  bs_noSignal,  //!< no signal on the bus
  bs_skip,        //!< skip all symbols until next @a SYN
  bs_ready,       //!< ready for next master (after @a SYN symbol, send/receive QQ)
  bs_recvCmd,     //!< receive command (ZZ, PBSB, master data) [passive set]
  bs_recvCmdCrc,  //!< receive command CRC [passive set]
  bs_recvCmdAck,  //!< receive command ACK/NACK [passive set + active set+get]
  bs_recvRes,     //!< receive response (slave data) [passive set + active get]
  bs_recvResCrc,  //!< receive response CRC [passive set + active get]
  bs_recvResAck,  //!< receive response ACK/NACK [passive set]
  bs_sendCmd,     //!< send command (ZZ, PBSB, master data) [active set+get]
  bs_sendCmdCrc,  //!< send command CRC [active set+get]
  bs_sendResAck,  //!< send response ACK/NACK [active get]
  bs_sendCmdAck,  //!< send command ACK/NACK [passive get]
  bs_sendRes,     //!< send response (slave data) [passive get]
  bs_sendResCrc,  //!< send response CRC [passive get]
  bs_sendSyn,     //!< send SYN for completed transfer [active set+get]
};

/** bit for the seen state: seen. */
#define SEEN 0x01

/** bit for the seen state: scan initiated. */
#define SCAN_INIT 0x02

/** bit for the seen state: scan finished. */
#define SCAN_DONE 0x04

/** bit for the seen state: configuration loading initiated. */
#define LOAD_INIT 0x08

/** bit for the seen state: configuration loaded. */
#define LOAD_DONE 0x10

class BusHandler;

/**
 * Generic request for sending to and receiving from the bus.
 */
class BusRequest {
  friend class BusHandler;

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
   * Notify the request of the specified result.
   * @param result the result of the request.
   * @param slave the @a SlaveSymbolString received.
   * @return true if the request needs to be restarted.
   */
  virtual bool notify(result_t result, const SlaveSymbolString& slave) = 0;


 protected:
  /** the master data @a MasterSymbolString to send. */
  const MasterSymbolString& m_master;

  /** the number of times a send is repeated due to lost arbitration. */
  unsigned int m_busLostRetries;

  /** whether to automatically delete this @a BusRequest when finished. */
  const bool m_deleteOnFinish;
};


/**
 * A poll @a BusRequest handled by @a BusHandler itself.
 */
class PollRequest : public BusRequest {
  friend class BusHandler;

 public:
  /**
   * Constructor.
   * @param message the associated @a Message.
   */
  explicit PollRequest(Message* message)
    : BusRequest(m_master, true), m_message(message), m_index(0) {}

  /**
   * Destructor.
   */
  virtual ~PollRequest() {}

  /**
   * Prepare the master data.
   * @param masterAddress the master bus address to use.
   * @return the result code.
   */
  result_t prepare(symbol_t masterAddress);

  // @copydoc
  bool notify(result_t result, const SlaveSymbolString& slave) override;


 private:
  /** the master data @a MasterSymbolString. */
  MasterSymbolString m_master;

  /** the associated @a Message. */
  Message* m_message;

  /** the current part index in @a m_message. */
  size_t m_index;
};


/**
 * A scan @a BusRequest handled by @a BusHandler itself.
 */
class ScanRequest : public BusRequest {
  friend class BusHandler;

 public:
  /**
   * Constructor.
   * @param deleteOnFinish whether to automatically delete this @a ScanRequest when finished.
   * @param messageMap the @a MessageMap instance.
   * @param messages the @a Message instances to query starting with the primary one.
   * @param slaves the slave addresses to scan.
   * @param busHandler the @a BusHandler instance to notify of final scan result.
   * @param notifyIndex the offset to the index for notifying the scan result.
   */
  ScanRequest(bool deleteOnFinish, MessageMap* messageMap, const deque<Message*>& messages,
      const deque<symbol_t>& slaves, BusHandler* busHandler, size_t notifyIndex = 0)
    : BusRequest(m_master, deleteOnFinish), m_messageMap(messageMap), m_index(0), m_allMessages(messages),
      m_messages(messages), m_slaves(slaves), m_busHandler(busHandler), m_notifyIndex(notifyIndex),
      m_result(RESULT_ERR_NO_SIGNAL) {
    m_message = m_messages.front();
    m_messages.pop_front();
  }

  /**
   * Destructor.
   */
  virtual ~ScanRequest() {}

  /**
   * Prepare the next master data.
   * @param masterAddress the master bus address to use.
   * @return the result code.
   */
  result_t prepare(symbol_t masterAddress);

  // @copydoc
  bool notify(result_t result, const SlaveSymbolString& slave) override;


 private:
  /** the @a MessageMap instance. */
  MessageMap* m_messageMap;

  /** the master data @a MasterSymbolString. */
  MasterSymbolString m_master;

  /** the currently queried @a Message. */
  Message* m_message;

  /** the current part index in @a m_message. */
  size_t m_index;

  /** all secondary @a Message instances. */
  const deque<Message*> m_allMessages;

  /** the remaining secondary @a Message instances. */
  deque<Message*> m_messages;

  /** the slave addresses to scan. */
  deque<symbol_t> m_slaves;

  /** the @a BusHandler instance to notify of final scan result. */
  BusHandler* m_busHandler;

  /** the offset to the index for notifying the scan result. */
  size_t m_notifyIndex;

  /** the overall result of handling the request. */
  result_t m_result;
};


/**
 * An active @a BusRequest that can be waited for.
 */
class ActiveBusRequest : public BusRequest {
  friend class BusHandler;

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
 * Helper class for keeping track of grabbed messages.
 */
class GrabbedMessage {
 public:
  /**
   * Construct a new instance.
   */
  GrabbedMessage() : m_lastTime(0), m_count(0) {}

  /**
   * Copy constructor.
   * @param other the @a GrabbedMessage to copy from.
   */
  GrabbedMessage(const GrabbedMessage& other) : m_count(other.m_count) {
    m_lastMaster = other.m_lastMaster;
    m_lastSlave = other.m_lastSlave;
  }

  /**
   * Set the last received data.
   * @param master the last @a MasterSymbolString.
   * @param slave the last @a SymbolString.
   */
  void setLastData(const MasterSymbolString& master, const SlaveSymbolString& slave);

  /**
   * Get the last received time.
   * @return the last received time.
   */
  time_t getLastTime() const { return m_lastTime; }

  /**
   * Get the last @a MasterSymbolString.
   * @return the last @a MasterSymbolString.
   */
  MasterSymbolString& getLastMasterData() { return m_lastMaster; }

  /**
   * Dump the last received data and message count to the output.
   * @param unknown whether to dump only if this message is unknown.
   * @param messages the @a MessageMap instance for resolving known @a Message instances.
   * @param first whether this is the first message to be added to the output.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the @a ostringstream to format the messages to.
   * @param isDirectMode true for direct mode, false for grab command.
   * @return whether the message was added to the output.
   */
  bool dump(bool unknown, MessageMap* messages, bool first, OutputFormat outputFormat, ostringstream* output,
      bool isDirectMode = false) const;


 private:
  /** the last received time. */
  time_t m_lastTime;

  /** the last @a MasterSymbolString. */
  MasterSymbolString m_lastMaster;

  /** the last @a SlaveSymbolString. */
  SlaveSymbolString m_lastSlave;

  /** the number of times this message was seen. */
  unsigned int m_count;
};


/**
 * Handles input from and output to the bus with respect to the eBUS protocol.
 */
class BusHandler : public WaitThread {
 public:
  /**
   * Construct a new instance.
   * @param device the @a Device instance for accessing the bus.
   * @param messages the @a MessageMap instance with all known @a Message instances.
   * @param ownAddress the own master address.
   * @param answer whether to answer queries for the own master/slave address.
   * @param busLostRetries the number of times a send is repeated due to lost arbitration.
   * @param failedSendRetries the number of times a failed send is repeated (other than lost arbitration).
   * @param busAcquireTimeout the maximum time in milliseconds for bus acquisition.
   * @param slaveRecvTimeout the maximum time in milliseconds an addressed slave is expected to acknowledge.
   * @param lockCount the number of AUTO-SYN symbols before sending is allowed after lost arbitration, or 0 for auto detection.
   * @param generateSyn whether to enable AUTO-SYN symbol generation.
   * @param pollInterval the interval in seconds in which poll messages are cycled, or 0 if disabled.
   */
  BusHandler(Device* device, MessageMap* messages,
      symbol_t ownAddress, bool answer,
      unsigned int busLostRetries, unsigned int failedSendRetries,
      unsigned int busAcquireTimeout, unsigned int slaveRecvTimeout,
      unsigned int lockCount, bool generateSyn,
      unsigned int pollInterval)
    : WaitThread(), m_device(device), m_reconnect(false), m_messages(messages),
      m_ownMasterAddress(ownAddress), m_ownSlaveAddress(getSlaveAddress(ownAddress)),
      m_answer(answer), m_addressConflict(false),
      m_busLostRetries(busLostRetries), m_failedSendRetries(failedSendRetries),
      m_busAcquireTimeout(busAcquireTimeout), m_slaveRecvTimeout(slaveRecvTimeout),
      m_masterCount(device->isReadOnly()?0:1), m_autoLockCount(lockCount == 0),
      m_lockCount(lockCount <= 3 ? 3 : lockCount), m_remainLockCount(m_autoLockCount ? 1 : 0),
      m_generateSynInterval(generateSyn ? SYN_TIMEOUT*getMasterNumber(ownAddress)+SYMBOL_DURATION : 0),
      m_pollInterval(pollInterval), m_symbolLatencyMin(-1), m_symbolLatencyMax(-1), m_arbitrationDelayMin(-1),
      m_arbitrationDelayMax(-1), m_lastReceive(0), m_lastPoll(0),
      m_currentRequest(nullptr), m_currentAnswering(false), m_runningScans(0), m_nextSendPos(0),
      m_symPerSec(0), m_maxSymPerSec(0),
      m_state(bs_noSignal), m_escape(0), m_crc(0), m_crcValid(false), m_repeat(false),
      m_grabMessages(true) {
    memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
    m_lastSynReceiveTime.tv_sec = 0;
    m_lastSynReceiveTime.tv_nsec = 0;
  }

  /**
   * Destructor.
   */
  virtual ~BusHandler() {
    stop();
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
    if (m_currentRequest != nullptr) {
      delete m_currentRequest;
      m_currentRequest = nullptr;
    }
  }

  /**
   * @return the @a Device instance for accessing the bus.
   */
  const Device* getDevice() const { return m_device; }

  /**
   * Clear stored values (e.g. scan results).
   */
  void clear();

  /**
   * Inject a message from outside and treat it as regularly retrieved from the bus.
   * @param master the @a MasterSymbolString with the master data.
   * @param slave the @a SlaveSymbolString with the slave data.
   */
  void injectMessage(const MasterSymbolString& master, const SlaveSymbolString& slave) {
    m_command = master;
    m_response = slave;
    m_addressConflict = true;  // avoid conflict messages
    messageCompleted();
    m_addressConflict = false;
  }

  /**
   * Send a message on the bus and wait for the answer.
   * @param master the @a MasterSymbolString with the master data to send.
   * @param slave the @a SlaveSymbolString that will be filled with retrieved slave data.
   * @return the result code.
   */
  result_t sendAndWait(const MasterSymbolString& master, SlaveSymbolString* slave);

  /**
   * Prepare the master part for the @a Message, send it to the bus and wait for the answer.
   * @param message the @a Message instance.
   * @param inputStr the input @a string from which to read master values (if any).
   * @param dstAddress the destination address to set, or @a SYN to keep the address defined during construction.
   * @param srcAddress the source address to set, or @a SYN for the own master address.
   * @return the result code.
   */
  result_t readFromBus(Message* message, const string& inputStr, symbol_t dstAddress = SYN,
      symbol_t srcAddress = SYN);

  /**
   * Main thread entry.
   */
  virtual void run();

  /**
   * Initiate a scan of the slave addresses.
   * @param full true for a full scan (all slaves), false for scanning only already seen slaves.
   * @param levels the current user's access levels.
   * @return the result code.
   */
  result_t startScan(bool full, const string& levels);

  /**
   * Set the scan result @a string for a scanned slave address.
   * @param dstAddress the scanned slave address.
   * @param index the index of the result to set (starting with 0 for the ident message).
   * @param str the scan result @a string to set, or empty if not a single part of the scan was successful.
   */
  void setScanResult(symbol_t dstAddress, size_t index, const string& str);

  /**
   * Called from @a ScanRequest upon completion.
   */
  void setScanFinished();

  /**
   * Format the scan result for a single slave to the @a ostringstream.
   * @param slave the slave address for which to format the scan result.
   * @param leadingNewline whether to insert a newline before the scan result.
   * @param output the @a ostringstream to format the scan result to.
   * @return true when a scan result was formatted, false otherwise.
   */
  bool formatScanResult(symbol_t slave, bool leadingNewline, ostringstream* output) const;

  /**
   * Format the scan result to the @a ostringstream.
   * @param output the @a ostringstream to format the scan result to.
   */
  void formatScanResult(ostringstream* output) const;

  /**
   * Format information about seen participants to the @a ostringstream.
   * @param output the @a ostringstream to append the info to.
   */
  void formatSeenInfo(ostringstream* output) const;

  /**
   * Format information for running the update check to the @a ostringstream.
   * @param output the @a ostringstream to append the info to.
   */
  void formatUpdateInfo(ostringstream* output) const;

  /**
   * Send a scan message on the bus and wait for the answer.
   * @param dstAddress the destination slave address to send to.
   * @param loadScanConfig true to immediately load the message definitions matching the scan result.
   * @param reload true to fully reload the scan results, false when the slave ID was already retrieved.
   * @return the result code.
   */
  result_t scanAndWait(symbol_t dstAddress, bool loadScanConfig = false, bool reload = false);

  /**
   * Start or stop grabbing unknown messages.
   * @param enable true to enable grabbing, false to disable it.
   * @return true when the grabbing was changed.
   */
  bool enableGrab(bool enable = true);

  /**
   * Return whether grabbing unknown messages is enabled.
   * @return whether grabbing unknown messages is enabled.
   */
  bool isGrabEnabled() { return m_grabMessages; }

  /**
   * Format the grabbed messages to the @a ostringstream.
   * @param unknown whether to dump only unknown messages.
   * @param outputFormat the @a OutputFormat options to use.
   * @param output the @a ostringstream to format the messages to.
   * @param isDirectMode true for direct mode, false for grab command.
   * @param since the start time from which to add received messages (inclusive), or 0 for all.
   * @param until the end time to which to add received messages (exclusive), or 0 for all.
   */
  void formatGrabResult(bool unknown, OutputFormat outputFormat, ostringstream* output, bool isDirectMode = false,
      time_t since = 0, time_t until = 0) const;

  /**
   * Return true when a signal on the bus is available.
   * @return true when a signal on the bus is available.
   */
  bool hasSignal() const { return m_state != bs_noSignal; }

  /**
   * Reconnect the device.
   */
  void reconnect() { m_reconnect = true; }

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
   * Get the next slave address that still needs to be scanned or loaded.
   * @param lastAddress the last returned slave address, or 0 for returning the first one.
   * @param withUnfinished whether to include slave addresses that were not scanned yet.
   * @return the next slave address that still needs to be scanned or loaded, or @a SYN.
   */
  symbol_t getNextScanAddress(symbol_t lastAddress, bool withUnfinished) const;

  /**
   * Set the state of the participant to configuration @a LOADED.
   * @param address the slave address.
   * @param file the file from which the configuration was loaded, or empty if loading was not possible.
   */
  void setScanConfigLoaded(symbol_t address, const string& file);


 private:
  /**
   * Handle the next symbol on the bus.
   * @return RESULT_OK on success, or an error code.
   */
  result_t handleSymbol();

  /**
   * Set a new @a BusState and add a log message if necessary.
   * @param state the new @a BusState.
   * @param result the result code.
   * @param firstRepetition true if the first repetition of a message part is being started.
   * @return the result code.
   */
  result_t setState(BusState state, result_t result, bool firstRepetition = false);

  /**
   * Add a seen bus address.
   * @param address the seen bus address.
   * @return true if a conflict with the own addresses was detected, false otherwise.
   */
  bool addSeenAddress(symbol_t address);

  /**
   * Called to measure the latency between send and receive of a symbol.
   * @param sentTime the time the symbol was sent.
   * @param recvTime the time the symbol was received.
   */
  void measureLatency(struct timespec* sentTime, struct timespec* recvTime);

  /**
   * Called when a message sending or reception was successfully completed.
   */
  void messageCompleted();

  /**
   * Prepare a @a ScanRequest.
   * @param slave the single slave address to scan, or @a SYN for multiple.
   * @param full true for a full scan (all slaves), false for scanning only already seen slaves.
   * @param levels the current user's access levels.
   * @param reload true to force sending the scan message, false to send only if necessary (only for single slave).
   * @param request the created @a ScanRequest (may be nullptr with positive result if scan is not needed).
   * @return the result code.
   */
  result_t prepareScan(symbol_t slave, bool full, const string& levels, bool* reload, ScanRequest** request);

  /** the @a Device instance for accessing the bus. */
  Device* m_device;

  /** set to @p true when the device shall be reconnected. */
  bool m_reconnect;

  /** the @a MessageMap instance with all known @a Message instances. */
  MessageMap* m_messages;

  /** the own master address. */
  const symbol_t m_ownMasterAddress;

  /** the own slave address. */
  const symbol_t m_ownSlaveAddress;

  /** whether to answer queries for the own master/slave address. */
  const bool m_answer;

  /** set to @p true once an address conflict with the own addresses was detected. */
  bool m_addressConflict;

  /** the number of times a send is repeated due to lost arbitration. */
  const unsigned int m_busLostRetries;

  /** the number of times a failed send is repeated (other than lost arbitration). */
  const unsigned int m_failedSendRetries;

  /** the maximum time in milliseconds for bus acquisition. */
  const unsigned int m_busAcquireTimeout;

  /** the maximum time in milliseconds an addressed slave is expected to acknowledge. */
  const unsigned int m_slaveRecvTimeout;

  /** the number of masters already seen. */
  unsigned int m_masterCount;

  /** whether m_lockCount shall be detected automatically. */
  const bool m_autoLockCount;

  /** the number of AUTO-SYN symbols before sending is allowed after lost arbitration. */
  unsigned int m_lockCount;

  /** the remaining number of AUTO-SYN symbols before sending is allowed again. */
  unsigned int m_remainLockCount;

  /** the interval in milliseconds after which to generate an AUTO-SYN symbol, or 0 if disabled. */
  unsigned int m_generateSynInterval;

  /** the interval in seconds in which poll messages are cycled, or 0 if disabled. */
  const unsigned int m_pollInterval;

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

  /** the time of the last received SYN symbol, or 0 for never. */
  struct timespec m_lastSynReceiveTime;

  /** the time of the last received symbol, or 0 for never. */
  time_t m_lastReceive;

  /** the time of the last poll, or 0 for never. */
  time_t m_lastPoll;

  /** the queue of @a BusRequests that shall be handled. */
  Queue<BusRequest*> m_nextRequests;

  /** the currently handled BusRequest, or nullptr. */
  BusRequest* m_currentRequest;

  /** whether currently answering a request from another participant. */
  bool m_currentAnswering;

  /** the queue of @a BusRequests that are already finished. */
  Queue<BusRequest*> m_finishedRequests;

  /** the number of scan request currently running. */
  unsigned int m_runningScans;

  /** the offset of the next symbol that needs to be sent from the command or response,
   * (only relevant if m_request is set and state is @a bs_command or @a bs_response). */
  size_t m_nextSendPos;

  /** the number of received symbols in the last second. */
  unsigned int m_symPerSec;

  /** the maximum number of received symbols per second ever seen. */
  unsigned int m_maxSymPerSec;

  /** the current @a BusState. */
  BusState m_state;

  /** 0 when not escaping/unescaping, or @a ESC when receiving, or the original value when sending. */
  symbol_t m_escape;

  /** the calculated CRC. */
  symbol_t m_crc;

  /** whether the CRC matched. */
  bool m_crcValid;

  /** whether the current message part is being repeated. */
  bool m_repeat;

  /** the received command @a MasterSymbolString. */
  MasterSymbolString m_command;

  /** the received response @a SlaveSymbolString or response to send. */
  SlaveSymbolString m_response;

  /** the participating bus addresses seen so far (0 if not seen yet, or combination of @a SEEN bits). */
  symbol_t m_seenAddresses[256];

  /** the scan results by slave address and index. */
  map<symbol_t, vector<string>> m_scanResults;

  /** whether to grab messages. */
  bool m_grabMessages;

  /** the grabbed messages by key.*/
  map<uint64_t, GrabbedMessage> m_grabbedMessages;
};

}  // namespace ebusd

#endif  // EBUSD_BUSHANDLER_H_

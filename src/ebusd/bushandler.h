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

#ifndef EBUSD_BUSHANDLER_H_
#define EBUSD_BUSHANDLER_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include "ebusd/scan.h"
#include "lib/ebus/message.h"
#include "lib/ebus/data.h"
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/protocol.h"

namespace ebusd {

/** @file ebusd/bushandler.h
 * Classes, functions, and constants related to handling messages on the eBUS.
 */

using std::string;

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
class BusHandler : public ProtocolListener {
 public:
  /**
   * Construct a new instance.
   * @param messages the @a MessageMap instance with all known @a Message instances.
   * @param scanHelper the @a ScanHelper instance.
   * @param pollInterval the interval in seconds in which poll messages are cycled, or 0 if disabled.
   */
  BusHandler(MessageMap* messages, ScanHelper* scanHelper,
      unsigned int pollInterval)
    : m_protocol(nullptr), m_messages(messages), m_scanHelper(scanHelper),
      m_pollInterval(pollInterval), m_lastPoll(0), m_runningScans(0),
      m_grabMessages(true) {
    memset(m_seenAddresses, 0, sizeof(m_seenAddresses));
  }

  /**
   * Destructor.
   */
  virtual ~BusHandler() {
  }

  /**
   * Set the @a ProtocolHandler instance for accessing the bus.
   * @param protocol the @a ProtocolHandler instance for accessing the bus.
   */
  void setProtocol(ProtocolHandler* protocol) { m_protocol = protocol; }

  /**
   * @return the @a ProtocolHandler instance for accessing the bus.
   */
  ProtocolHandler* getProtocol() const { return m_protocol; }

  /**
   * Clear stored values (e.g. scan results).
   */
  void clear();

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
   * Get the number of scan requests currently running.
   * @eturn the number of scan requests currently running.
   */
  unsigned int getRunningScans() const { return m_runningScans; }

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

  // @copydoc
  void notifyProtocolStatus(ProtocolState state, result_t result) override;

  // @copydoc
  result_t notifyProtocolAnswer(const MasterSymbolString& master, SlaveSymbolString* slave) override;

  // @copydoc
  void notifyProtocolSeenAddress(symbol_t address) override;

  // @copydoc
  void notifyProtocolMessage(bool sent, const MasterSymbolString& master, const SlaveSymbolString& slave) override;

 private:
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

  /** the @a ProtocolHandler instance for accessing the bus (loosely coupled but set quickly after construction). */
  ProtocolHandler* m_protocol;

  /** the @a MessageMap instance with all known @a Message instances. */
  MessageMap* m_messages;

  /** the @a ScanHelper instance. */
  ScanHelper* m_scanHelper;

  /** the interval in seconds in which poll messages are cycled, or 0 if disabled. */
  const unsigned int m_pollInterval;

  /** the time of the last poll, or 0 for never. */
  time_t m_lastPoll;

  /** the number of scan requests currently running. */
  unsigned int m_runningScans;

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

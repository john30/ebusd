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

#ifndef LIB_EBUS_PROTOCOL_DIRECT_H_
#define LIB_EBUS_PROTOCOL_DIRECT_H_

#include <map>
#include "lib/ebus/protocol.h"

namespace ebusd {

/** @file lib/ebus/protocol_direct.h
 * Implementation of directly handled eBUS protocol.
 *
 * The following table shows the possible states, symbols, and state transition
 * depending on the kind of message to send/receive:
 * @image html states.png "ebusd direct ProtocolHandler states"
 */

/** the possible bus states. */
enum BusState {
  bs_noSignal,    //!< no signal on the bus
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



/**
 * Directly handles input from and output to eBUS with respect to the eBUS protocol.
 */
class DirectProtocolHandler : public ProtocolHandler {
 public:
  /**
   * Construct a new instance.
   * @param config the configuration to use.
   * @param device the @a Device instance for accessing the bus.
   * @param listener the @a ProtocolListener.
   */
  DirectProtocolHandler(const ebus_protocol_config_t config,
      Device* device, ProtocolListener* listener)
    : ProtocolHandler(config, device, listener),
      m_lockCount(config.lockCount <= 3 ? 3 : config.lockCount),
      m_remainLockCount(config.lockCount == 0 ? 1 : 0),
      m_generateSynInterval(config.generateSyn ? 10*getMasterNumber(config.ownAddress)+SYN_TIMEOUT : 0),
      m_currentRequest(nullptr), m_currentAnswering(false), m_nextSendPos(0),
      m_state(bs_noSignal), m_escape(0), m_crc(0), m_crcValid(false), m_repeat(false) {
    m_lastSynReceiveTime.tv_sec = 0;
    m_lastSynReceiveTime.tv_nsec = 0;
  }

  /**
   * Destructor.
   */
  virtual ~DirectProtocolHandler() {
    join();
    if (m_currentRequest != nullptr) {
      delete m_currentRequest;
      m_currentRequest = nullptr;
    }
  }

  // @copydoc
  void injectMessage(const MasterSymbolString& master, const SlaveSymbolString& slave) override {
    if (isRunning()) {
      return;
    }
    m_command = master;
    m_response = slave;
    m_addressConflict = true;  // avoid conflict messages
    messageCompleted();
    m_addressConflict = false;
  }

  /**
   * Main thread entry.
   */
  virtual void run();

  // @copydoc
  bool hasSignal() const override { return m_state != bs_noSignal; }

  // @copydoc
  bool isAnswering() const override { return !m_answerByKey.empty(); }

  // @copydoc
  bool setAnswer(symbol_t srcAddress, symbol_t dstAddress, symbol_t pb, symbol_t sb,
      const symbol_t* id, size_t idLen, const SlaveSymbolString& answer) override;

  // @copydoc
  bool hasAnswer(symbol_t dstAddress) const override;

 private:
  /**
   * Handle sending the next symbol to the bus.
   * @param recvTimeout pointer to a variable in which to put the timeout for the receive.
   * @param sentSymbol pointer to a variable in which to put the sent symbol.
   * @param sentTime pointer to a variable in which to put the system time when the symbol was sent.
   * @return RESULT_OK on success, RESULT_CONTINUE when a symbol was sent, or an error code.
   */
  result_t handleSend(unsigned int* recvTimeout, symbol_t* sentSymbol, struct timespec* sentTime);

  /**
   * Handle receiving the next symbol from the bus.
   * @param timeout the timeout for the receive.
   * @param sending whether a symbol was sent before entry.
   * @param sentSymbol the sent symbol to verify (if sending).
   * @param sentTime pointer to a variable with the system time when the symbol was sent.
   * @return RESULT_OK on success, RESULT_CONTINUE when further received symbols are buffered,
   * or an error code.
   */
  result_t handleReceive(unsigned int timeout, bool sending, symbol_t sentSymbol, struct timespec* sentTime);

  /**
   * Set a new @a BusState and add a log message if necessary.
   * @param state the new @a BusState.
   * @param result the result code.
   * @param firstRepetition true if the first repetition of a message part is being started.
   * @return the result code.
   */
  result_t setState(BusState state, result_t result, bool firstRepetition = false);

  // @copydoc
  bool addSeenAddress(symbol_t address) override;

  /**
   * Called when a message sending or reception was successfully completed.
   */
  void messageCompleted();

  /**
   * Create a key for storing an answer.
   * @param srcAddress the source address, or @a SYN for any.
   * @param dstAddress the destination address.
   * @param pb the primary ID byte.
   * @param sb the secondary ID byte.
   * @param id optional further ID bytes.
   * @param idLen the length of the further ID bytes.
   * @return a key for storing an answer.
   */
  uint64_t createAnswerKey(symbol_t srcAddress, symbol_t dstAddress, symbol_t pb, symbol_t sb,
      const symbol_t* id, size_t idLen);

  /**
   * Build the answer to the currently received message and store in @a m_response for sending back to requestor.
   * @return @p true on success, @p false if the message is not supposed to be answered.
   */
  bool getAnswer();

  /** the number of AUTO-SYN symbols before sending is allowed after lost arbitration. */
  unsigned int m_lockCount;

  /** the remaining number of AUTO-SYN symbols before sending is allowed again. */
  unsigned int m_remainLockCount;

  /** the interval in milliseconds after which to generate an AUTO-SYN symbol, or 0 if disabled. */
  unsigned int m_generateSynInterval;

  /** the time of the last received SYN symbol, or 0 for never. */
  struct timespec m_lastSynReceiveTime;

  /** the currently handled BusRequest, or nullptr. */
  BusRequest* m_currentRequest;

  /** the answers to give by key. */
  std::map<uint64_t, SlaveSymbolString > m_answerByKey;

  /** whether currently answering a request from another participant. */
  bool m_currentAnswering;

  /** the offset of the next symbol that needs to be sent from the command or response,
   * (only relevant if m_request is set and state is @a bs_command or @a bs_response). */
  size_t m_nextSendPos;

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
};

}  // namespace ebusd

#endif  // LIB_EBUS_PROTOCOL_DIRECT_H_

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022-2024 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_KNXHANDLER_H_
#define EBUSD_KNXHANDLER_H_

#include <map>
#include <string>
#include <list>
#include <vector>
#include <utility>
#include "ebusd/datahandler.h"
#include "ebusd/bushandler.h"
#include "lib/ebus/message.h"
#include "lib/ebus/stringhelper.h"
#include "lib/knx/knx.h"
#include "lib/utils/arg.h"

namespace ebusd {

/** @file ebusd/knxhandler.h
 * A data handler enabling KNX support.
 */

using std::map;
using std::string;
using std::vector;

/**
 * Helper function for getting the arg definition for KNX.
 * @return a pointer to the child argument options, or nullptr.
 */
const argParseChildOpt* knxhandler_getargs();

/**
 * Registration function that is called once during initialization.
 * @param userInfo the @a UserInfo instance.
 * @param busHandler the @a BusHandler instance.
 * @param messages the @a MessageMap instance.
 * @param handlers the @a list to which new @a DataHandler instances shall be added.
 * @return true if registration was successful.
 */
bool knxhandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers);


/** type for KNX APCI values (application control field). */
enum apci_t {
  // within KNX_TRANSFER_GROUP:
  APCI_GROUPVALUE_READ = 0x000,      //!< A_GroupValue_Read-PDU
  APCI_GROUPVALUE_RESPONSE = 0x040,  //!< A_GroupValue_Response-PDU (mask APCI_GROUPVALUE_READ_WRITE_MASK)
  APCI_GROUPVALUE_WRITE = 0x080,     //!< A_GroupValue_Write-PDU (mask APCI_GROUPVALUE_READ_WRITE_MASK)
  APCI_INDIVIDUALADDRESS_READ = 0x100,  //!< A_IndividualAddress_Read-PDU
  APCI_INDIVIDUALADDRESS_RESPONSE = 0x140,  //!< A_IndividualAddress_Response-PDU
  APCI_INDIVIDUALADDRESS_WRITE = 0x0c0,  //!< A_IndividualAddress_Write-PDU
  // within KNX_TRANSFER_CONNECTED:
  APCI_DEVICEDESCRIPTOR_READ = 0x300,  //!< A_DeviceDescriptor_Read-PDU
  APCI_DEVICEDESCRIPTOR_RESPONSE = 0x340,  //!< A_DeviceDescriptor_Read-PDU (mask should be 0x3c0)
  APCI_PROPERTYVALUE_READ = 0x3d5,  //!< A_PropertyValue_Read-PDU
  APCI_PROPERTYVALUE_RESPONSE = 0x3d6,  //!< A_PropertyValue_Response-PDU
  APCI_PROPERTYVALUE_WRITE = 0x3d7,  //!< A_PropertyValue_Write-PDU
  APCI_RESTART = 0x380,  //!< A_Restart-PDU
};

#define APCI_GROUPVALUE_READ_WRITE_MASK 0x3c0

#define FLAG_READ 0x400000
#define FLAG_WRITE 0x800000

/** datatype length flags (byte length on KNX in bits 0-3, extra info in higher bits). */
typedef struct {
  bool hasDivisor: 1;
  bool isFloat: 1;
  bool isSigned: 1;
  bool lastValueSent: 1;
  uint8_t length;  // 0 for 1-6 bits, number of bytes otherwise
  uint32_t lastValue;
} dtlf_t;

#define DTLF_1BIT dtlf_t{.hasDivisor = false, .isFloat = false, .isSigned = false, .lastValueSent = false, .length = 0, .lastValue = 0}
#define DTLF_8BIT dtlf_t{.hasDivisor = false, .isFloat = false, .isSigned = false, .lastValueSent = false, .length = 1, .lastValue = 0}

/** type for global values not associated with an ebus message. */
enum global_t {
  GLOBAL_VERSION = 1,
  GLOBAL_RUNNING = 2,
  GLOBAL_UPTIME = 3,
  GLOBAL_SIGNAL = 4,
  GLOBAL_SCAN = 5,
  GLOBAL_UPDATECHECK = 6,
};

/** type for several group subscription infos. */
typedef struct {
  uint64_t messageKey;  // message key, or 0 for global value
  union {
    uint8_t fieldIndex;  // message field index
    global_t globalIndex;  // global value index
  };
  dtlf_t lengthFlag;  // telegram length and flags
} groupInfo_t;


/**
 * The main class supporting KNX data handling.
 */
class KnxHandler : public DataSink, public DataSource, public WaitThread {
 public:
  /**
   * Constructor.
   * @param userInfo the @a UserInfo instance.
   * @param busHandler the @a BusHandler instance.
   * @param messages the @a MessageMap instance.
   */
  KnxHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages);

 public:
  /**
   * Destructor.
   */
  ~KnxHandler() override;

  // @copydoc
  void startHandler() override;

  // @copydoc
  void notifyUpdateCheckResult(const string& checkResult) override;

  // @copydoc
  void notifyScanStatus(scanStatus_t scanStatus) override;

  /**
   * Send a group value.
   * @param dest the destination group address.
   * @param apci the APCI value.
   * @param lengthFlag the datatype length flag.
   * @param value the value.
   * @param field the message field or nullptr for non field related.
   * @return the result code.
   */
  result_t sendGroupValue(knx_addr_t dest, apci_t apci, dtlf_t& lengthFlag, unsigned int value,
  const SingleDataField *field = nullptr) const;

  /**
   * Send a global value to the registered group address.
   * @param index the global value index to send.
   * @param value the raw value.
   * @param response true to send as response, false to send as write.
   */
  void sendGlobalValue(global_t index, unsigned int value, bool response = false);

  /**
   * Wait for and receive a KNX group telegram.
   * @param maxlen the size of the data buffer.
   * @param buf the data buffer.
   * @param recvlen pointer to a variable in which to store the actually received length.
   * @param src pointer to a variable in which to store the source address.
   * @param dest pointer to a variable in which to store the destination group address.
   * @param wait true to wait up to 2 seconds for a new telegram, false to not wait.
   * @return the result code, either RESULT_OK on success, RESULT_ERR_GENERIC_IO on I/O error (e.g. socket closed),
   * or RESULT_ERR_TIMEOUT if no data is available.
   */
  result_t receiveTelegram(int maxlen, knx_transfer_t* typ, uint8_t *buf, int *recvlen, knx_addr_t *src,
  knx_addr_t *dest, bool wait = true);

  /**
   * Handle a received KNX telegram.
   * @param typ the transfer data type.
   * @param src the source address.
   * @param dest the destination group address.
   * @param len the data length (including the TPCI/APCI octet 6, i.e. transport control field).
   * @param data the data buffer (starting with the TPCI/APCI octet 6, i.e. transport control field).
   */
  void handleReceivedTelegram(knx_transfer_t typ, knx_addr_t src, knx_addr_t dest, int len, const uint8_t *data);

 protected:
  // @copydoc
  void run() override;

  /**
   * Handle a received non-group telegram when the device has an individual address and is programmable.
   * @param typ the transfer data type.
   * @param src the source address (ensured to be non-zero).
   * @param dest the destination address (group or individual according to address type encoded in the transfer data type).
   * @param len the data length (including the TPCI/APCI octet 6, i.e. transport control field).
   * @param data the data buffer (starting with the TPCI/APCI octet 6, i.e. transport control field).
   */
  void handleNonGroupTelegram(knx_transfer_t typ, knx_addr_t src, knx_addr_t dest, int len, const uint8_t *data);

  /**
   * Send a DISCONNECT to the destination and reset the connected state.
   * @param dest the destination individual address to send to.
   */
  void sendNonGroupDisconnect(knx_addr_t dest);

  /**
   * Handle a received group telegram.
   * @param src the source address.
   * @param dest the destination group address.
   * @param len the data length (including the TPCI/APCI octet 6, i.e. transport control field).
   * @param data the data buffer (starting with the TPCI/APCI octet 6, i.e. transport control field).
   */
  void handleGroupTelegram(knx_addr_t src, knx_addr_t dest, int len, const uint8_t *data);

 private:
  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the @a StringReplacers from the integration file. */
  StringReplacers m_replacers;

  /** the group address for relevant message fields before being subscribed to by "circuit/message/field" name. */
  map<string, knx_addr_t> m_messageFieldGroupAddress;

  /**
   * the group addresses that need to be responded to.
   * key is the group address in lower 16 bits, and flags in upper 16 bits with:
   * - read direction in bit 6 (<<16),
   * - write direction in bit 7 (<<16).
   * this way read and write may be mapped to different messages.
   * value contains the message key and additional infos.
   */
  map<uint32_t, groupInfo_t>m_subscribedGroups;

  /** the group address and flags (key of m_subscribedGroups) by subscribed message key. */
  map<uint64_t, list<uint32_t> >m_subscribedMessages;

  /** the group address and flags (key of m_subscribedGroups) by subscribed global values. */
  map<global_t, uint32_t>m_subscribedGlobals;

  /** the time the run thread was entered. */
  time_t m_start;

  /** the knx connection as long as initialized, or nullptr. */
  KnxConnection* m_con;

  /** the time of the last sent individual address response, or 0. */
  time_t m_lastIndividualAddressResponseTime = 0;

  /** the time of the last connection, or 0 if not connected. */
  uint64_t m_lastConnectTime = 0;

  /** the source address of the last connection, or 0. */
  knx_addr_t m_lastConnectSource = 0;

  /** the SeqNo for reception of the last connection. */
  uint8_t m_lastConnectRecvSeq = 0;

  /** the SeqNo for sending of the last connection. */
  uint8_t m_lastConnectSendSeq = 0;

  /** true when last connection is in state OPEN_WAIT. */
  bool m_waitForAck = false;

  /** the last update check result. */
  string m_lastUpdateCheckResult;

  /** the last scan status. */
  scanStatus_t m_lastScanStatus;

  /** set to true when a scan finish was received. */
  bool m_scanFinishReceived;

  /** the last system time when a communication error was logged. */
  time_t m_lastErrorLogTime;
};

}  // namespace ebusd

#endif  // EBUSD_KNXHANDLER_H_

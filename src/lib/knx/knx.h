/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_KNX_KNX_H_
#define LIB_KNX_KNX_H_

#include <cstdint>

// base KNX address type (group or individual)
typedef uint16_t knx_addr_t;

// the transfer types (lower 8 bits of transport control field with sequence=0, plus bit 8 with address type)
enum knx_transfer_t {
  KNX_TRANSFER_NONE = -1, // no transfer available
  KNX_TRANSFER_GROUP = 0x100, // data group or broadcast PDU
  KNX_TRANSFER_TAG_GROUP = 0x104, // data tag group PDU
  KNX_TRANSFER_INDIVIDUAL = 0x000, // data individual PDU
  KNX_TRANSFER_CONNECTED = 0x040, // data connected PDU
  KNX_TRANSFER_CONNECT = 0x080, // connect PDU
  KNX_TRANSFER_DISCONNECT = 0x081, // disconnect PDU
  KNX_TRANSFER_ACK = 0x0c2, // ACK PDU
  KNX_TRANSFER_NAK = 0x0c3, // NAK PDU
};


/**
 * An abstract KNX connection.
 */
class KnxConnection {
 public:
  /**
   * Construct a new instance.
   */
  KnxConnection() {}

  /**
   * Destructor.
   */
  virtual ~KnxConnection() {}

  /**
   * Create a new KnxConnection.
   * @param url the URL to connect to.
   * @return the new KnxConnection, or @a nullptr on error.
   */
  static KnxConnection* create();

  /**
   * Open a connection to the specified URL.
   * @param url the URL to connect to.
   * @return nullptr on success, or an error message.
   */
  virtual const char* open(const char* url) = 0;

  /**
   * @return true if connected, false otherwise.
   */
  virtual bool isConnected() const = 0;

  /**
   * Close the connection.
   */
  virtual void close() = 0;

  /**
   * @return the file descriptor for polling.
   */
  virtual int getPollFd() const = 0;

  /**
   * Get the available data (after the file descriptor was checked for availability).
   * @param size the size of the data buffer.
   * @param data the data buffer to copy the data to.
   * @param len pointer to store the actual data length to.
   * @param src optional pointer to store the source address (if any, depending on poll data type).
   * @param dst optional pointer to store the destination address (if any, depending on poll data type).
   * @return the polled transfer data type.
   */
  virtual knx_transfer_t getPollData(int size, uint8_t* data, int* len, knx_addr_t* src, knx_addr_t* dst) = 0;

  /**
   * Send a group APDU.
   * @param dst the destination address.
   * @param len the APDU length.
   * @param data the APDU data buffer.
   * @return nullptr on success, or an error message.
   */
  virtual const char* sendGroup(knx_addr_t dst, int len, const uint8_t* data) = 0;

  /**
   * @return true if connection allows programming via ETS.
   */
  virtual bool isProgrammable() { return false; };

  /**
   * @return the individual address, or 0 if not programmed yet, or any non-zero value if not programmable.
   */
  virtual knx_addr_t getAddress() { return 0xffff; };

  /**
   * @param address the individual address to set.
   */
  virtual void setAddress(knx_addr_t address) {
    // default implementation does nothing
  }
};

#endif  // LIB_KNX_KNX_H_
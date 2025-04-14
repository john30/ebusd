/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022-2025 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_KNX_KNXD_H_
#define LIB_KNX_KNXD_H_

#include <eibclient.h>
#include "lib/knx/knx.h"

namespace ebusd {

/** @file lib/knx/knxd.h
 * KNXd based implementation of the @a KnxConnection interface.
 */

/**
 * A KnxConnection based on libeibclient using the group communication interface of the connected KNXd.
 * Unfortunately, this does not allow acting as a KNX device, i.e. enter programming mode and make individual address
 * and group association table writable from ETS. As such, an KNXnet/IP implementation is available as well.
 */
class KnxdConnection : public KnxConnection {
 public:
  /**
   * Construct a new instance.
   */
  explicit KnxdConnection(const char *url)
      : KnxConnection(), m_url(url), m_con(nullptr) {}

  /**
   * Destructor.
   */
  virtual ~KnxdConnection() {
    close();
  }

  // @copydoc
  const char* getInfo() const override {
    return "KNXd";
  }

  // @copydoc
  const char* open() override {
    close();
    m_con = EIBSocketURL(m_url);
    if (!m_con) {
      return "open error";
    }
    if (EIBOpen_GroupSocket(m_con, 0) < 0) {
      EIBClose_sync(m_con);
      m_con = nullptr;
      return "open group error";
    }
    return nullptr;
  }

  // @copydoc
  bool isConnected() const override {
    return m_con != nullptr;
  }

  void close() override {
    if (m_con) {
      EIBClose_sync(m_con);
      m_con = nullptr;
    }
  }

  // @copydoc
  int getPollFd() const override {
    return EIB_Poll_FD(m_con);
  }

  // @copydoc
  knx_transfer_t getPollData(int size, uint8_t* data, int* len, knx_addr_t* src, knx_addr_t* dst) override {
    int ret = EIB_Poll_Complete(m_con);
    if (ret == -1) {
      // read failed
      return KNX_TRANSFER_NONE;
    }
    ret = EIBGetGroup_Src(m_con, size, data, src, dst);
    if (ret < 2) {
      return KNX_TRANSFER_NONE;
    }
    if (len) {
      *len = ret;
    }
    return KNX_TRANSFER_GROUP;
  }

  // @copydoc
  const char* sendGroup(knx_addr_t dst, int len, const uint8_t* data) override {
    if (EIBSendGroup(m_con, dst, len, data) < 0) {
      return "send error";
    }
    return nullptr;
  }

  // @copydoc
  const char* sendTyp(knx_transfer_t typ, knx_addr_t dst, int len, const uint8_t* data) override {
    return "not available";
  }

 private:
  /** the URL to connect to. */
  const char* m_url;

  /** the knx structure if connected, or nullptr. */
  EIBConnection* m_con;
};

}  // namespace ebusd

#endif  // LIB_KNX_KNXD_H_

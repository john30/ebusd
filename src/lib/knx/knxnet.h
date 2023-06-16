/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022-2023 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_KNX_KNXNET_H_
#define LIB_KNX_KNXNET_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifndef __CYGWIN__
#include <net/if_arp.h>
#endif
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef __FreeBSD__
  #include <machine/endian.h>
#else
  #include <endian.h>
#endif
#include <string>
#include <cstdio>
#include <ctime>
#include "lib/knx/knx.h"

namespace ebusd {

/** @file lib/knx/knxnet.h
 * KNXnet/IP implementation of the @a KnxConnection interface based on UDP multicast.
 */

using std::string;

// 16 bit unsigned big endian
typedef union __attribute__ ((packed)) {
  uint16_t raw;
  struct {
    uint8_t high;
    uint8_t low;
  };
} uint16be_t;

// 32 bit unsigned big endian
typedef union __attribute__ ((packed)) {
  uint32_t raw;
  struct {
    uint8_t msb1;
    uint8_t msb2;
    uint8_t msb3;
    uint8_t lsb;
  };
} uint32be_t;

// KNXnet/IP header
typedef struct __attribute__ ((packed)) {
  uint8_t headerLength;  // =6
  uint8_t protocolVersion;  // =0x10
  uint16be_t serviceTypeIdentifier;
  uint16be_t totalLength;  // complete length including header
} knxnet_header_t;

/** service types. */
typedef enum {
  SERVICE_TYPE_SEARCH_REQ = 0x0201,
  SERVICE_TYPE_SEARCH_RES = 0x0202,
  SERVICE_TYPE_DESC_REQ = 0x0203,
  SERVICE_TYPE_DESC_RES = 0x0204,
//  SERVICE_TYPE_CONN_REQ = 0x0205,
//  SERVICE_TYPE_CONN_RES = 0x0206,
//  SERVICE_TYPE_CONNSTATE_REQ = 0x0207,
//  SERVICE_TYPE_CONNSTATE_RES = 0x0208,
//  SERVICE_TYPE_DISCONN_REQ = 0x0209,
//  SERVICE_TYPE_DISCONN_RES = 0x020A,
//  SERVICE_TYPE_DEVICE_CFG_REQ = 0x0310,
//  SERVICE_TYPE_DEVICE_CFG_ACK = 0x0311,
//  SERVICE_TYPE_TUNNEL_REQ = 0x0420,
//  SERVICE_TYPE_TUNNEL_ACK = 0x0421,
   SERVICE_TYPE_ROUTE_IND = 0x0530,
   SERVICE_TYPE_ROUTE_LOST = 0x0531,
   SERVICE_TYPE_ROUTE_BUSY = 0x0532,
} knxnet_service_type_t;

// cEMI frame header (external message interface)
typedef struct __attribute__ ((packed)) {
  uint8_t messageCode;
  // optional immediately following additional bytes, usually =0. fixed to 0 in cEMI management messages
  uint8_t additionalInfoLength;
} knxnet_cemi_header_t;

/* cEMI message codes. */
typedef enum {
  // MESSAGE_CODE_BUSMON_IND = 0x2B,
  MESSAGE_CODE_DATA_REQ = 0x11,
  MESSAGE_CODE_DATA_CON = 0x2E,
  MESSAGE_CODE_DATA_IND = 0x29,
//  MESSAGE_CODE_RAW_REQ = 0x10,
//  MESSAGE_CODE_RAW_CON = 0x2D,
//  MESSAGE_CODE_RAW_IND = 0x2F,
  // MESSAGE_CODE_POLLDATA_REQ = 0x13,
  // MESSAGE_CODE_POLLDATA_CON = 0x25,
  // MESSAGE_CODE_DATACONN_REQ = 0x41,
  // MESSAGE_CODE_DATACONN_IND = 0x89,
  // MESSAGE_CODE_DATAIND_REQ = 0x4A,
  // MESSAGE_CODE_DATAIND_IND = 0x94,
  // MESSAGE_CODE_PROPREAD_REQ = 0xFC,
  // MESSAGE_CODE_PROPREAD_CON = 0xFB,
  // MESSAGE_CODE_PROPWRITE_REQ = 0xF6,
  // MESSAGE_CODE_PROPWRITE_CON = 0xF5,
  // MESSAGE_CODE_PROPINFO_IND = 0xF7,
  // MESSAGE_CODE_FUNCPROPCMD_REQ = 0xF8,
  // MESSAGE_CODE_FUNCPROPSTATEREAD_REQ = 0xF9,
  // MESSAGE_CODE_FUNCPROP_CON = 0xFA,
//  MESSAGE_CODE_RESET_IND = 0xF0,
//  MESSAGE_CODE_RESET_REQ = 0xF1,
} knxnet_message_code_t;

// L_Data services header
typedef struct __attribute__ ((packed)) {
  union {
    uint8_t raw;
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
      bool frameType: 1;  // 0=extended, 1=standard
      bool reserved: 1;
      bool repeat: 1;  // 0=repeat, 1=do not repeat
      bool systemBroadcast: 1;  // 0=system broadcast, 1=broadcast
      uint8_t priority: 2;  // 0=system, 1=normal, 2=urgent, 3=low
      bool acknowledgeRequest: 1;  // 1=ack requested
      bool confirm: 1;  // 0=no error, 1=error
#else
      bool confirm: 1;  // 0=no error, 1=error
      bool acknowledgeRequest: 1;  // 1=ack requested
      uint8_t priority: 2;  // 0=system, 1=normal, 2=urgent, 3=low
      bool systemBroadcast: 1;  // 0=system broadcast, 1=broadcast
      bool repeat: 1;  // 0=repeat, 1=do not repeat
      bool reserved: 1;
      bool frameType: 1;  // 0=extended, 1=standard
#endif
    };
  } controlField1;
  union {
    uint8_t raw;
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
      bool addressType: 1;  // 0=individual, 1=group
      uint8_t hopCount: 3;
      uint8_t extendedFrameFormat: 4;  // 0=standard frame, 0xf=escape
#else
      uint8_t extendedFrameFormat: 4;  // 0=standard frame, 0xf=escape
      uint8_t hopCount: 3;
      bool addressType: 1;  // 0=individual, 1=group
#endif
    };
  } controlField2;
  uint16be_t sourceAddress;
  uint16be_t destinationAddress;
  uint8_t informationLength;  // number of NPDU octets (not including the TPCI/APCI octet)
} knxnet_l_data_header_t;


typedef union __attribute__ ((packed)) {
  uint8_t raw;
  struct {
#if __BYTE_ORDER == __BIG_ENDIAN
    bool controlFlag: 1;  // 0=data, 1=control
    bool numbered: 1;  // 1=has sequence, 0=no sequence
    uint8_t sequence: 4;  // optional sequence number
    uint8_t apci: 2;  // highest 2 bits of APCI
#else
    uint8_t apci: 2;  // highest 2 bits of APCI
    uint8_t sequence: 4;  // optional sequence number
    bool numbered: 1;  // 1=has sequence, 0=no sequence
    bool controlFlag: 1;  // 0=data, 1=control
#endif
  };
} knxnet_tpci_apci_t;

typedef struct __attribute__ ((packed)) {
  uint8_t length;
  uint8_t protocolCode;  // 0x01=UDP over IPv4
  uint32be_t ipAddressV4;
  uint16be_t port;
} knxnet_hpai_t;

#define PROTOCOL_CODE_IPV4_UDP 0x01

typedef struct __attribute__ ((packed)) {
  uint8_t length;
  uint8_t descriptionCode;  // 0x01=device info
  uint8_t medium;  // 0x20=IP
  uint8_t status;  // bit 0=programming mode
  uint16be_t individualAddress;
  uint16be_t projInstId;
  uint8_t serial[6];
  in_addr_t multicastAddress;
  uint8_t macAddress[6];
  unsigned char name[30];
} knxnet_dib_devinfo_t;

typedef struct __attribute__ ((packed)) {
  uint8_t length;
  uint8_t descriptionCode;  // 0x02=services
  struct {
    uint8_t familyId;
    uint8_t familyVersion;
  };  // just one for now
} knxnet_dib_services_t;

// the default system port
#define SYSTEM_MULTICAST_PORT 3671

// the default system multicast address 224.0.23.12
#define SYSTEM_MULTICAST_IP 0xe000170c

#define LAST_FRAME_TIMEOUT 2

class LastFrame {
  friend class LastFrames;

 public:
  void set(uint8_t* data, size_t len, size_t lOffset, time_t now) {
    if (len >= sizeof(m_data)) {
      return;
    }
    memcpy(m_data, data, len);
    m_len = len;
    m_lOffset = lOffset;
    m_time = now;
  }

  bool isValid(time_t now) {
    return m_len && m_time >= now-LAST_FRAME_TIMEOUT;
  }

  bool isSameAs(uint8_t* data, size_t len, size_t lOffset, time_t now, bool isSend = false) {
    if (!m_len || len != m_len || lOffset != m_lOffset) {
      return false;
    }
    if (memcmp(data, m_data, len) == 0) {
      m_time = now;
      return true;
    }
    int oldHopCount = (m_data[lOffset+1]&0x70)>>4;
    int newHopCount = (data[lOffset+1]&0x70)>>4;
    if (newHopCount < 6  // top hop count is always tolerated TODO bad idea?
      && memcmp(data, m_data, lOffset+1) == 0  // including first byte of l_data header
      && (data[lOffset+1]&~0x70) == (m_data[lOffset+1]&~0x70)  // ignore hop count
      && (isSend ? newHopCount <= oldHopCount : newHopCount < oldHopCount)  // decremented hop count?
      && memcmp(data+lOffset+2, m_data+lOffset+2, len-(lOffset+2)) == 0
    ) {
      m_time = now;
      return true;
    }
    return false;
  }

  void reset() {
    m_time = 0;
  }

 private:
  /** the last data. */
  uint8_t m_data[256];

  /** the length of the last data, or 0 for none. */
  size_t m_len;

  /** the offset to the L_Data. */
  size_t m_lOffset;

  /** the time of the last data, or 0 for none. */
  time_t m_time;
};


#define CHECK_REPETITION_COUNT 4

class LastFrames {
 public:
  bool isRepetition(uint8_t* data, size_t len, size_t lOffset, time_t now, bool isSend = false) {
    for (int i=0; i < CHECK_REPETITION_COUNT; i++) {
      if (m_lastFrames[i].isValid(now)
      && m_lastFrames[i].isSameAs(data, len, lOffset, now, isSend)) {
        return true;
      }
    }
    return false;
  }

  void add(uint8_t* data, size_t len, size_t lOffset, time_t now) {
    int oldestPos = -1;
    time_t oldestAge = 0;
    for (int i=0; i < CHECK_REPETITION_COUNT; i++) {
      if (!m_lastFrames[i].isValid(now)) {
        m_lastFrames[i].set(data, len, lOffset, now);
        return;
      }
      if (oldestPos < 0 || m_lastFrames[i].m_time < oldestAge) {
        oldestPos = i;
        oldestAge = m_lastFrames[i].m_time;
      }
    }
    m_lastFrames[oldestPos].set(data, len, lOffset, now);
  }

  void reset() {
    for (int i=0; i < CHECK_REPETITION_COUNT; i++) {
      m_lastFrames[i].reset();
    }
  }

 private:
  /** the list of the last telegrams. */
  LastFrame m_lastFrames[CHECK_REPETITION_COUNT];
};

#ifdef DEBUG
#define PRINTF printf

// helper method to log received/sent telegrams
void logTelegram(bool sent, knxnet_cemi_header_t* c, knxnet_l_data_header_t* l, uint8_t* d) {
  bool isGrp = l->controlField2.addressType;
  PRINTF("%s msgcode=%2.2x, %d.%d.%d > %d%c%d%c%d, repeat=%s, ack=%s, hopcnt=%d, prio=%s, frame=%s, %sbroad, "
    "confirm=%s, tpci/apci=%2.2x",
         sent ? "send" : "recv",
         c->messageCode,
         l->sourceAddress.high>>4,
         l->sourceAddress.high&0xf,
         l->sourceAddress.low,
         isGrp ? l->destinationAddress.high>>3 : l->destinationAddress.high>>4,
         isGrp ? '/' : '.',
         isGrp ? l->destinationAddress.high&0x1f : l->destinationAddress.high&0xf,
         isGrp ? '/' : '.',
         l->destinationAddress.low,
         l->controlField1.repeat ? "yes" : "no",
         l->controlField1.acknowledgeRequest ? "yes" : "no",
         l->controlField2.hopCount,
         l->controlField1.priority == 1 ? "normal" : l->controlField1.priority == 2 ? "urgent" :
           l->controlField1.priority == 3 ? "low" : "system",
         l->controlField1.frameType ? "std" : "ext",
         l->controlField1.systemBroadcast ? "" : "sys ",
         l->controlField1.confirm ? "error" : "no err",
         d[0]);
  if (d) {
    PRINTF(", data=");
    for (int i=0; i < l->informationLength; i++) {
      PRINTF("%2.2x ", d[1+i]);
    }
  }
  PRINTF("\n");
}
#else
#define PRINTF(...)
#define logTelegram(...)
#endif


/**
 * A KnxConnection based on IP multicast as alternative to using libeibclient.
 * This is still an incomplete KNXnet/IP implementation.
 */
class KnxNetConnection : public KnxConnection {
 public:
  /**
   * Construct a new instance.
   */
  explicit KnxNetConnection(const char* url)
      : KnxConnection(), m_url(url), m_sock(0), m_programmingMode(false), m_addr(0) {}

  /**
   * Destructor.
   */
  virtual ~KnxNetConnection() {
    close();
  }

  // @copydoc
  const char* getInfo() const override {
    return "KNXnet/IP multicast";
  }

  // @copydoc
  const char* open() override {
    close();
    int ret;
    struct in_addr mcast = {};
    mcast.s_addr = htonl(SYSTEM_MULTICAST_IP);
    m_interface.s_addr = INADDR_ANY;
    m_port = SYSTEM_MULTICAST_PORT;
    if (m_url && m_url[0]) {  // non-empty
      string urlStr = m_url;  // "[mcast][@intf]" for non-default 224.0.23.12:3671)
      if (!urlStr.empty()) {
        auto pos = urlStr.find('@');
        if (pos != string::npos) {
          string intfStr = urlStr.substr(pos+1);
          const char* intfCstr = intfStr.c_str();
          ret = inet_aton(intfCstr, &m_interface);
          if (ret == 0) {
            return "intf addr";
          }
          urlStr = urlStr.substr(0, pos);
        }
      }
      if (!urlStr.empty()) {
        const char *mcastStr = urlStr.c_str();
        ret = inet_aton(mcastStr, &mcast);
        if (ret == 0) {
          return "multicast addr";
        }
      }
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = INADDR_ANY;

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
      return "create socket";
    }

    // set non-blocking
    ret = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (ret != 0) {
      ::close(fd);
      return "non-blocking";
    }

    // set reuse address option
    int optint = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optint, sizeof(optint));
    if (ret != 0) {
      ::close(fd);
      return "reuse";
    }

    // allow multiple processes using the same port for multicast on the same host
    unsigned char optchar = 1;
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &optchar, sizeof(optchar));
    if (ret != 0) {
      ::close(fd);
      return "mcast loop";
    }

    if (m_interface.s_addr != INADDR_ANY) {
      // set outgoing interface to other than default (determined by routing table)
      ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &m_interface, sizeof(m_interface));
      if (ret != 0) {
        ::close(fd);
        return "mcast intf";
      }
    }

    // bind for incoming multicast
    ret = bind(fd, (struct sockaddr*) &address, sizeof(address));
    if (ret != 0) {
      ::close(fd);
      return "bind socket";
    }

    // set the target address for later use by sendto()
    m_multicast = address;
    m_multicast.sin_addr = mcast;

    // join the multicast inbound
    ip_mreq req = {};
    req.imr_multiaddr = mcast;
    req.imr_interface = m_interface;
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) < 0) {
      ::close(fd);
      return "join multicast";
    }

    m_sock = fd;
    return nullptr;
  }

  // @copydoc
  bool isConnected() const override {
    return m_sock != 0;
  }

  // @copydoc
  void close() override {
    if (m_sock) {
      ::close(m_sock);
      m_sock = 0;
    }
  }

  // @copydoc
  int getPollFd() const override {
    return m_sock;
  }

  // @copydoc
  knx_transfer_t getPollData(int size, uint8_t* data, int* recvlen, knx_addr_t* src, knx_addr_t* dst) override {
    uint8_t buf[128];
    ssize_t slen = recv(m_sock, buf, sizeof(buf), 0);
    if (slen < 0 || static_cast<unsigned>(slen) < sizeof(knxnet_header_t)) {
      PRINTF("#skip recv short hdr len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    size_t len = static_cast<unsigned>(slen);
    auto h = (knxnet_header_t*)buf;
    if (h->headerLength != sizeof(knxnet_header_t) || h->protocolVersion != 0x10) {
      PRINTF("#skip recv short/proto len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    switch (htons(h->serviceTypeIdentifier.raw)) {
      case SERVICE_TYPE_ROUTE_IND:
        // expected value
        break;
//      case SERVICE_TYPE_SEARCH_REQ:
//        return KNX_TRANSFER_NONE;
//      case SERVICE_TYPE_DESC_REQ:
//        return KNX_TRANSFER_NONE;
      default:
        PRINTF("#skip recv service=%4.4x\n", htons(h->serviceTypeIdentifier.raw));
        return KNX_TRANSFER_NONE;
    }
    // routing indication
    size_t totalLen = htons(h->totalLength.raw);
    if (len < totalLen || len < sizeof(knxnet_header_t)+sizeof(knxnet_cemi_header_t)) {
      PRINTF("#skip recv short cemi len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    auto c = (knxnet_cemi_header_t*)(((uint8_t*)h)+sizeof(knxnet_header_t));
    if (c->messageCode != MESSAGE_CODE_DATA_IND) {
      PRINTF("#skip recv msgcode=%2.2x\n", c->messageCode);
      return KNX_TRANSFER_NONE;
    }
    auto lOffset = sizeof(knxnet_header_t)+sizeof(knxnet_cemi_header_t)+c->additionalInfoLength;
    ssize_t dataLen = totalLen - (lOffset+sizeof(knxnet_l_data_header_t));
    if (dataLen < 0) {
      PRINTF("#skip recv short data len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    auto l = (knxnet_l_data_header_t*)(((uint8_t*)h)+lOffset);
    auto d = ((uint8_t*)l)+sizeof(knxnet_l_data_header_t);
    if (!l->controlField1.frameType || !l->controlField1.systemBroadcast) {
      // not a regular standard frame broadcast
      PRINTF("#skip recv irregular frame len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    if (m_addr && (!l->controlField2.addressType && htons(l->destinationAddress.raw) != m_addr)) {
      // ignore packets with individual addr destination other than our own
      PRINTF("#skip recv not-own dest len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    if (m_addr && !l->controlField2.addressType && htons(l->sourceAddress.raw) == m_addr) {
      // ignore own source packets
      PRINTF("#skip recv own src len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    if (dataLen < 0 || dataLen < l->informationLength) {
      PRINTF("#skip recv short payload len=%d\n", len);
      return KNX_TRANSFER_NONE;
    }
    // check repeated frames
    time_t now;
    time(&now);
    //    PRINTF("getPoll len=%d, last sent len=%d\n", len, m_lastSentLen);
    if (m_lastRecvFrames.isRepetition(buf, totalLen, lOffset, now)) {
      // last recv packet repeated
      PRINTF("#skip recv last recv len=%d\n", totalLen);
      return KNX_TRANSFER_NONE;
    }
    if (m_lastSentFrames.isRepetition(buf, totalLen, lOffset, now, true)) {
      // last sent packet re-received
      PRINTF("#skip recv last sent len=%d\n", totalLen);
      return KNX_TRANSFER_NONE;
    }
    logTelegram(false, c, l, d);
    m_lastRecvFrames.add(buf, totalLen, lOffset, now);
    // all fine
    int ret = d[0];
    if (l->controlField2.addressType) {
      ret |= 0x100;  // address type group
    }
    if (!(ret&0x80)) {
      ret &= ~0x03;  // remove two apci bits
    }
    if (ret&0x40) {
      ret &= ~0x3c;  // remove sequence number
    }
    *recvlen = size > dataLen ? dataLen : size;
    memcpy(data, d, *recvlen);  // including the TPCI/APCI octet 6
    if (src) {
      *src = htons(l->sourceAddress.raw);
    }
    if (dst) {
      *dst = htons(l->destinationAddress.raw);
    }
    return (knx_transfer_t)ret;
  }

  // @copydoc
  const char* sendGroup(knx_addr_t dst, int len, const uint8_t* data) override {
    return send(KNX_TRANSFER_GROUP, dst, len, data);
  }

  // @copydoc
  const char* sendTyp(knx_transfer_t typ, knx_addr_t dst, int len, const uint8_t* data) override {
    return send(typ, dst, len, data);
  }

  // @copydoc
  bool isProgrammable() const override { return true; };

 private:
  /**
   * Send a message.
   * @param typ the transfer type to send.
   * @param dst the destination address.
   * @param len the APDU length.
   * @param data the APDU data buffer.
   * @return nullptr on success, or an error message.
   */
  const char* send(knx_transfer_t typ, knx_addr_t dst, int len, const uint8_t* data) {
    uint8_t buf[128];
    auto h = (knxnet_header_t*)buf;
    h->headerLength = sizeof(knxnet_header_t);
    h->protocolVersion = 0x10;
    h->serviceTypeIdentifier.raw = htons(SERVICE_TYPE_ROUTE_IND);
    // first byte of data is expected to hold the APCI upper byte:
    size_t totalLen = sizeof(knxnet_header_t)+sizeof(knxnet_cemi_header_t)+sizeof(knxnet_l_data_header_t)+len;
    h->totalLength.raw = htons(totalLen);
    auto c = (knxnet_cemi_header_t*)(buf+sizeof(knxnet_header_t));
    c->messageCode = MESSAGE_CODE_DATA_IND;
    c->additionalInfoLength = 0;
    auto lOffset = sizeof(knxnet_header_t)+sizeof(knxnet_cemi_header_t)+c->additionalInfoLength;
    auto l = (knxnet_l_data_header_t*)(((uint8_t*)h)+lOffset);
    l->controlField1.raw = 0xbc;  // standard frame, no repeat, broadcast, low prio, no ack, no err
    l->controlField2.raw = 0xe0;  // group address, hop count 6, standard frame
    l->controlField2.addressType = (typ&0x100) != 0;
    l->sourceAddress.raw = htons(m_addr);
    l->destinationAddress.raw = htons(dst);
    if (typ&0x100) {
      // ensure at least default individual address
      if (!m_addr) {
        l->sourceAddress.raw = 0xffff;  // for "unregistered device" in S-Mode
      }
    }
    l->informationLength = len-1;  // subtracting the TPCI/APCI
    uint8_t* d = buf+sizeof(knxnet_header_t)+sizeof(knxnet_cemi_header_t)+sizeof(knxnet_l_data_header_t);
    // first byte of data is expected to hold the APCI upper byte, copy remainder:
    memcpy(d, data, len);
    int tpci = typ&0xff;  // TPCI/APCI
    if ((typ&0x080) == 0) {
      tpci |= (d[0]&0x03);  // highest 2 bits of APCI
    }
    if (typ&0x040) {
      tpci |= d[0]&((0x0f) << 2);  // SeqNo
    }
    d[0] = tpci;
    logTelegram(true, c, l, d);
    ssize_t sent = sendto(m_sock, buf, totalLen, MSG_NOSIGNAL, (sockaddr*)&m_multicast, sizeof(m_multicast));
    if (sent < 0) {
      return "send error";
    }
    time_t now;
    time(&now);
    m_lastSentFrames.add(buf, totalLen, lOffset, now);
    return nullptr;
  }

  // copydoc
  knx_addr_t getAddress() const override {
    return m_addr;
  }

  // copydoc
  void setAddress(knx_addr_t address) override {
    m_addr = address;
    // flush duplication check buffers
    m_lastRecvFrames.reset();
    m_lastSentFrames.reset();
  }

  // copydoc
  bool isProgrammingMode() const override {
    return m_programmingMode;
  }

  // copydoc
  void setProgrammingMode(bool on) override {
    m_programmingMode = on;
  }

 private:
  /** the URL to connect to. */
  const char* m_url;

  /** the multicast address to join. */
  struct sockaddr_in m_multicast;

  /** the port to listen to. */
  in_port_t m_port;

  /** the optional interface address to bind to. */
  struct in_addr m_interface;

  /** the socket if connected, or 0. */
  int m_sock;

  /** true while in programming mode. */
  bool m_programmingMode;

  /** the own address, or 0 if not yet set. */
  knx_addr_t m_addr;

  /** the last received frames. */
  LastFrames m_lastRecvFrames;

  /** the last sent frames. */
  LastFrames m_lastSentFrames;
};

}  // namespace ebusd

#endif  // LIB_KNX_KNXNET_H_

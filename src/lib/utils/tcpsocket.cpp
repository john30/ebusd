/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2024 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/utils/tcpsocket.h"
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif

namespace ebusd {

TCPSocket::TCPSocket(int sfd, socketaddress* address) : m_sfd(sfd) {
  char ip[17];
  inet_ntop(AF_INET, (struct in_addr*)&(address->sin_addr.s_addr), ip, (socklen_t)sizeof(ip)-1);
  m_ip = ip;
  m_port = (uint16_t)ntohs(address->sin_port);
}

bool TCPSocket::isValid() {
  return fcntl(m_sfd, F_GETFL) != -1;
}

bool parseIp(const char* server, struct in_addr *sin_addr) {
  if (inet_aton(server, sin_addr) == 1) {
    return true;
  }
  struct hostent* he = gethostbyname(server);
  if (he == nullptr) {
    return false;
  }
  memcpy(sin_addr, he->h_addr_list[0], he->h_length);
  return true;
}

int socketConnect(const char* server, uint16_t port, int udpProto, socketaddress* storeAddress,
int tcpConnToUdpOptions, int tcpKeepAliveInterval, struct in_addr* storeIntf) {
  socketaddress localAddress;
  socketaddress* address = storeAddress ? storeAddress : &localAddress;
  memset(reinterpret_cast<char*>(address), 0, sizeof(*address));

  // parse "address[@intf]"
  const char* pos = strchr(server, '@');
  struct in_addr intf;
  intf.s_addr = INADDR_ANY;
  if (pos) {
    char* str = (char*)strdupa(server);  // (char*) needed for wrong return type on e.g. Alpine
    char* ifa = strchr(str, '@');
    ifa[0] = 0;
    ifa++;
    if (!str[0] || !parseIp(str, &address->sin_addr)) {
      return -1;
    }
    if (!parseIp(ifa, &intf)) {
      return -1;
    }
  } else if (!parseIp(server, &address->sin_addr)) {
    return -1;
  }
  if (storeIntf) {
    *storeIntf = intf;
  }
  address->sin_family = AF_INET;
  address->sin_port = (in_port_t)htons(port);

  int sfd = socket(AF_INET, udpProto ? SOCK_DGRAM : SOCK_STREAM, udpProto);
  if (sfd < 0) {
    return -2;
  }
  int ret = 0;
  if (udpProto) {
    #define RET(chk, next) if (ret >= 0) { ret = chk; if (ret < 0) ret = next;}
    struct sockaddr_in bindAddress = *address;
    // allow multiple processes using the same port for multicast on the same host
    int optint = 1;
    RET(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optint, sizeof(optint)), -3);
#ifdef SO_REUSEPORT
    RET(setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optint, sizeof(optint)), -3);
#endif
    bool isMcast = IN_MULTICAST(ntohl(address->sin_addr.s_addr));
    if (isMcast) {
      // loop-back sent multicast packets
      unsigned char optchar = 1;
      RET(setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &optchar, sizeof(optchar)), -3);
      if (ret >= 0) {
        // join the multicast inbound
        ip_mreq req = {};
        req.imr_multiaddr = address->sin_addr;
        req.imr_interface = intf;
        RET(setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)), -7);
      }
      if (ret >= 0 && intf.s_addr != INADDR_ANY) {
        // set outgoing interface to other than default (determined by routing table)
        RET(setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_IF, &intf, sizeof(intf)), -3);
      }
    }
    bindAddress.sin_addr = intf;
    if (!(tcpConnToUdpOptions&0x01)) {
      bindAddress.sin_port = 0;  // do not bind to same source port for outgoing packets
    }
    RET(bind(sfd, (struct sockaddr*)&bindAddress, sizeof(bindAddress)), -4);
    if (tcpConnToUdpOptions&0x02) {
      // set the default target address for later use by send()
      RET(::connect(sfd, (struct sockaddr*)address, sizeof(*address)), -5);
      if (ret < 0) {
        close(sfd);
        return ret;
      }
    }
    return sfd;
  }
  int value = 1;
  ret = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void*>(&value), sizeof(value));
  if (ret < 0) {
    close(sfd);
    return -3;
  }
  if (tcpKeepAliveInterval > 0) {
    value = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPALIVE");
    }
#ifndef TCP_KEEPIDLE
  #ifdef TCP_KEEPALIVE
    #define TCP_KEEPIDLE TCP_KEEPALIVE
  #else
    #define TCP_KEEPIDLE 4
  #endif
#endif
#ifndef TCP_KEEPINTVL
  #define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
  #define TCP_KEEPCNT 6
#endif
    value = tcpKeepAliveInterval+1;  // send keepalive after interval + 1 seconds of silence
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPIDLE");
    }
    value = tcpKeepAliveInterval;  // send keepalive in given interval
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPINTVL");
    }
    value = 2;  // drop connection after 2 failed keep alive sends
    if (setsockopt(sfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
      perror("setsockopt KEEPCNT");
    }
#ifdef TCP_USER_TIMEOUT
    value = (2+tcpKeepAliveInterval*3)*1000;  // 1 second higher than keepalive timeout
    if (setsockopt(sfd, IPPROTO_TCP, TCP_USER_TIMEOUT, reinterpret_cast<void*>(&value), sizeof(value)) != 0) {
       perror("setsockopt USER_TIMEOUT");
    }
#endif
  }
  if (tcpConnToUdpOptions > 0 && fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) {  // set non-blocking
    close(sfd);
    return -4;
  }
  ret = ::connect(sfd, (struct sockaddr*)address, sizeof(*address));
  if (ret != 0) {
    if (ret < 0 && (tcpConnToUdpOptions <= 0 || errno != EINPROGRESS)) {
      close(sfd);
      return -5;
    }
    if (tcpConnToUdpOptions > 0) {
      ret = socketPoll(sfd, POLLIN|POLLOUT, tcpConnToUdpOptions);
      if (ret <= 0) {
        close(sfd);
        return -6;
      }
      if (fcntl(sfd, F_SETFL, 0) < 0) {  // set blocking again
        close(sfd);
        return -4;
      }
    }
  }
  return sfd;
}

int socketPoll(int sfd, int which, int timeoutSeconds) {
  int ret;
#if defined(HAVE_PPOLL) || defined(HAVE_PSELECT)
  struct timespec tdiff;
  tdiff.tv_sec = timeoutSeconds;
  tdiff.tv_nsec = 0;
#else
  struct timeval tdiff;
  tdiff.tv_sec = timeoutSeconds;
  tdiff.tv_usec = 0;
#endif
#ifdef HAVE_PPOLL
  nfds_t nfds = 1;
  struct pollfd fds[nfds];
  memset(fds, 0, sizeof(fds));
  fds[0].fd = sfd;
  fds[0].events = which;
  ret = ppoll(fds, nfds, &tdiff, nullptr);
  if (ret >= 1 && fds[0].revents & POLLERR) {
    ret = -1;
  } else if (ret >= 1) {
    ret = fds[0].revents;
  }
#else
  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  if (which & POLLIN) {
    FD_SET(sfd, &readfds);
  }
  if (which & POLLOUT) {
    FD_SET(sfd, &writefds);
  }
  FD_SET(sfd, &exceptfds);
#ifdef HAVE_PSELECT
  ret = pselect(sfd + 1, &readfds, &writefds, &exceptfds, &tdiff, nullptr);
#else
  ret = select(sfd + 1, &readfds, &writefds, &exceptfds, &tdiff);
#endif
  if (ret >= 1 && FD_ISSET(sfd, &exceptfds)) {
    ret = -1;
  } else if (ret >= 1) {
    ret = (FD_ISSET(sfd, &readfds) ? POLLIN : 0) | (FD_ISSET(sfd, &writefds) ? POLLOUT : 0);
  }
#endif
  return ret;
}


TCPSocket* TCPSocket::connect(const string& server, const uint16_t& port, int timeout) {
  socketaddress address;
  int sfd = socketConnect(server.c_str(), port, false, &address, timeout);
  if (sfd < 0) {
    return nullptr;
  }
  TCPSocket* s = new TCPSocket(sfd, &address);
  if (timeout > 0) {
    s->setTimeout(timeout);
  }
  return s;
}


int TCPServer::start() {
  if (m_listening) {
    return 0;
  }
  m_lfd = socket(AF_INET, SOCK_STREAM, 0);
  socketaddress address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;
  address.sin_port = (in_port_t)htons(m_port);

  if (!m_address.empty() && inet_pton(AF_INET, m_address.c_str(), &address.sin_addr) != 1) {
    address.sin_addr.s_addr = INADDR_ANY;
  }
  int value = 1;
  setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  int result = bind(m_lfd, (struct sockaddr*)&address, sizeof(address));
  if (result != 0) {
    return result;
  }
  result = listen(m_lfd, 5);
  if (result != 0) {
    return result;
  }
  m_listening = true;
  return result;
}

TCPSocket* TCPServer::newSocket() {
  if (!m_listening) {
    return nullptr;
  }
  socketaddress address;
  socklen_t len = sizeof(address);
  memset(&address, 0, sizeof(address));

  int sfd = accept(m_lfd, (struct sockaddr*)&address, &len);
  if (sfd < 0) {
    return nullptr;
  }
  return new TCPSocket(sfd, &address);
}

size_t readNameRecursive(uint8_t *data, size_t len, size_t pos, size_t maxPos, int maxDepth, char* str, size_t slen,
  size_t* spos) {
  size_t nlen = data[pos++];
  if ((nlen&0xc0) == 0xc0) {
    // pointer
    size_t p = ((nlen&0x3f) << 8) | data[pos];
    if (p >= len || maxDepth < 1) {
      return 0;
    }
    readNameRecursive(data, len, p, len, maxDepth-1, str, slen, spos);
    return 2;
  }
  if (!nlen) {
    return 1;
  }
  if (pos+nlen > maxPos || *spos+1+nlen > slen) {
    return 0;
  }
  if (*spos > 0) {
    str[*spos] = '.';
    *spos += 1;
  }
  memcpy(str+*spos, data+pos, nlen);
  *spos += nlen;
  pos += nlen;
  size_t add;
  if (pos >= maxPos || maxDepth < 1) {
    add = 0;
  } else {
    add = readNameRecursive(data, len, pos, maxPos, maxDepth-1, str, slen, spos);
    if (add == 0) {
      return 0;
    }
  }
  return 1+nlen+add;
}

size_t readName(uint8_t *data, size_t len, size_t pos, size_t maxPos, char* str, size_t slen, size_t* spos) {
  return readNameRecursive(data, len, pos, maxPos, 4, str, slen, spos);
}

typedef struct __attribute__ ((packed)) {
  uint16_t id;
  struct {
#if __BYTE_ORDER == __BIG_ENDIAN
    bool qr: 1;  // 0=query, 1=answer
    uint8_t opcode: 4;  // 0=standard query, 1=inverse query, 2=status request
    bool aa: 1;  // authoritive answer
    bool tc: 1;  // truncation
    bool rd: 1;  // recursion desired
#else
    bool rd: 1;  // recursion desired
    bool tc: 1;  // truncation
    bool aa: 1;  // authoritive answer
    uint8_t opcode: 4;  // 0=standard query, 1=inverse query, 2=status request
    bool qr: 1;  // 0=query, 1=answer
#endif
  };
  struct {
#if __BYTE_ORDER == __BIG_ENDIAN
    bool ra: 1;  // recursion available
    uint8_t z: 3;  // zero
    uint8_t rcode: 4;  // response code: 0=OK
#else
    uint8_t rcode: 4;  // response code: 0=OK
    uint8_t z: 3;  // zero
    bool ra: 1;  // recursion available
#endif
  };
  uint16_t qdCount;  // question section entry count
  uint16_t anCount;  // answer section entry count
  uint16_t nsCount;  // name server section entry count
  uint16_t arCount;  // additional records section entry count
} dns_query_t;

typedef struct __attribute__ ((packed)) {
  uint8_t len;
  // unsigned char *name;
} dns_qname_t;

typedef struct __attribute__ ((packed)) {
  dns_qname_t qname;
  uint16_t qtype;
  uint16_t qclass;  // top bit used for unicast-response
} dns_question_t;

#define DNS_TYPE_A 0x01
#define DNS_TYPE_PTR 0x0c
#define DNS_TYPE_TXT 0x10
#define DNS_TYPE_SRV 0x21
#define DNS_CLASS_AA 0x01

typedef struct __attribute__ ((packed)) {
  dns_qname_t aname;
  uint16_t atype;
  uint16_t aclass;
  uint32_t ttl;
  uint16_t rdLength;
  // uint8_t *rData;
} dns_answer_t;

typedef struct __attribute__ ((packed)) {
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  dns_qname_t target;
} dns_rr_srv_t;

int resolveMdnsOneShot(const char* url, mdns_oneshot_t *result, mdns_oneshot_t *moreResults, size_t *moreCount) {
  memset(result, 0, sizeof(mdns_oneshot_t));
  socketaddress address;
  const char* pos = strchr(url, '@');
  string limitId = string(url);
  string device = "224.0.0.251";
  if (pos) {
    limitId = limitId.substr(0, pos-url);
    device += string(pos);
  }
  int sock = socketConnect(device.c_str(), 5353, IPPROTO_UDP, &address);
  if (sock < 0) {
    return -1;
  }

  uint8_t record[1500];
  memset(record, 0, sizeof(record));
  dns_query_t *dnsr = reinterpret_cast<dns_query_t*>(record);
  dnsr->qdCount = htons(1);
  size_t len = sizeof(dns_query_t);
  dns_question_t *q = reinterpret_cast<dns_question_t*>(reinterpret_cast<uint8_t*>(dnsr)+len);
  const uint8_t serviceName[] = {
    0x06, 0x5f, 0x65, 0x62, 0x75, 0x73, 0x64,  // _ebusd
    0x04, 0x5f, 0x74, 0x63, 0x70,  // _tcp
    0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c,  // local
    0x00
  };
  memcpy(&q->qname.len, serviceName, sizeof(serviceName));
  len += sizeof(serviceName)-1;  // -1 for final empty qname
  q = reinterpret_cast<dns_question_t*>(reinterpret_cast<uint8_t*>(dnsr)+len);
  q->qtype = htons(DNS_TYPE_PTR);
  q->qclass = htons(
    0x8000 |  // unicast response bit
    DNS_CLASS_AA);
  len += sizeof(dns_question_t);
  ssize_t done = sendto(sock, record, len, 0, reinterpret_cast<sockaddr*>(&address), sizeof(address));
#ifdef DEBUG_MDNS
  printf("mdns: sent %ld, err %d\n", done, errno);
#endif
  fcntl(sock, F_SETFL, O_NONBLOCK);
  bool found = false, foundMore = false;
  size_t moreRemain = moreResults && moreCount && *moreCount > 0 ? *moreCount : 0;
  if (moreRemain > 0) {
    *moreCount = 0;
  }
#ifdef DEBUG_MDNS
  socketaddress aaddr;
  socklen_t aaddrlen = 0;
#endif
  for (int i=0; i < (found ? 3 : 5); i++) {  // up to 5 seconds, at least 3 seconds
    int ret = socketPoll(sock, POLLIN, 1);
    done = 0;
    if (ret > 0 && (ret&POLLIN)) {
#ifdef DEBUG_MDNS
      aaddrlen = sizeof(aaddr);
      done = recvfrom(sock, record, sizeof(record), 0, reinterpret_cast<sockaddr*>(&aaddr), &aaddrlen);
#else
      done = recv(sock, record, sizeof(record), 0);
#endif
    }
    if (done == 0 || (done < 0 && errno == EAGAIN) || done < sizeof(dns_query_t)) {
      continue;
    }
    dnsr = reinterpret_cast<dns_query_t*>(record);
    // todo length check
#ifdef DEBUG_MDNS
    printf("mdns: got %d from %2.2x:%d, q=%d, an=%d, ns=%d, ar=%d\n", done, aaddr.sin_addr.s_addr,
      ntohs(aaddr.sin_port), ntohs(dnsr->qdCount), ntohs(dnsr->anCount), ntohs(dnsr->nsCount),
      ntohs(dnsr->arCount));
#endif
    if (dnsr->qdCount || done < sizeof(dns_query_t)+sizeof(serviceName)+4*sizeof(dns_answer_t)+(26+2)+4+1+1+
      sizeof(dns_rr_srv_t)+(2+1+sizeof(mdns_oneshot_t::id)-1+1+5+1+sizeof(mdns_oneshot_t::proto)-1)+4
      // "eBUS Adapter Shield xxxxxx", "id=xxxxxxxxxxxx.proto=ens"
    ) {
      continue;
    }
    int anCnt = ntohs(dnsr->anCount);
    int arCnt = ntohs(dnsr->arCount);
    if (anCnt < 1 || dnsr->nsCount || arCnt < 1) {
      continue;
    }
    len = sizeof(dns_query_t);
    char name[256];
    bool validPort = false;
    struct in_addr validAddress;
    validAddress.s_addr = INADDR_ANY;
    char id[sizeof(mdns_oneshot_t::id)] = {0};
    char proto[sizeof(mdns_oneshot_t::proto)] = {0};
    for (int i=0; i < anCnt+arCnt && len < done; i++) {
      dns_answer_t *a = reinterpret_cast<dns_answer_t*>(reinterpret_cast<uint8_t*>(dnsr)+len);
      if (i == 0) {
        if (memcmp(&a->aname.len, serviceName, sizeof(serviceName)) != 0) {
#ifdef DEBUG_MDNS
          printf("mdns: an 0 mismatch\n");
#endif
          anCnt = 0;
          break;  // skip this one
        }
#ifdef DEBUG_MDNS
        printf("mdns: an 0 match\n");
#endif
        len += sizeof(serviceName)-1;  // -1 for final empty qname
      } else {
        // read name
        size_t pos = 0;
        size_t nlen = readName(record, done, len, done, name, sizeof(name), &pos);
        if (nlen == 0) {
          anCnt = 0;
          break;  // skip this one
        }
        len += nlen-1;  // -1 for final empty qname / right pointer for below
        name[pos] = 0;
#ifdef DEBUG_MDNS
        printf("mdns: a%c %d name=%s\n", i >= anCnt ? 'r' : 'n', i >= anCnt ? i-anCnt : i, name);
#endif
      }
      a = reinterpret_cast<dns_answer_t*>(reinterpret_cast<uint8_t*>(dnsr)+len);
      int atype = ntohs(a->atype);
      int aclass = ntohs(a->aclass);
#ifdef DEBUG_MDNS
      printf("  atype %d, aclass %d\n", atype, aclass);
#endif
      if (i == 0 && (atype != DNS_TYPE_PTR
      || aclass != DNS_CLASS_AA)) {
        anCnt = 0;
        break;  // skip this one
      }
      len += sizeof(dns_answer_t);
      int rdLen = ntohs(a->rdLength);
#ifdef DEBUG_MDNS
      printf("  rd %d @%2.2x = ", rdLen, len);
      for (int i=0; i < rdLen && len+i < done; i++) {
        printf("%2.2x ", reinterpret_cast<uint8_t*>(dnsr)[len+i]);
      }
      printf("\n");
#endif
      if (atype == DNS_TYPE_PTR || atype == DNS_TYPE_TXT) {
        size_t pos = 0;
        if (readName(record, done, len, len+rdLen, name, sizeof(name), &pos) == 0) {
          anCnt = 0;
          break;  // skip this one
        }
        name[pos] = 0;
#ifdef DEBUG_MDNS
        printf("  %s=%s\n", (atype == DNS_TYPE_TXT) ? "txt" : "ptr", name);
#endif
        if (atype == DNS_TYPE_TXT && name[0]) {
          char* sep = strchr(name, '=');
          char* sep2;
          if (sep && strncmp(name, "id", sep-name) == 0) {
            sep2 = strchr(name, '.');
            if (sep2-sep-1 == sizeof(mdns_oneshot_t::id)-1) {
              memcpy(id, sep+1, sizeof(mdns_oneshot_t::id)-1);
            } else {
              sep = nullptr;
            }
            sep = sep ? strchr(sep2+1, '=') : nullptr;
          }
          if (sep && strncmp(sep2+1, "proto", sep-sep2-1) == 0 && (
            pos == sep+1+sizeof(mdns_oneshot_t::proto)-1-name
            || strchr(sep+1, '.') == sep+1+sizeof(mdns_oneshot_t::proto)-1)) {
            memcpy(proto, sep+1, sizeof(mdns_oneshot_t::proto)-1);
          }
        }
      } else if (atype == DNS_TYPE_SRV && rdLen >= sizeof(dns_rr_srv_t)) {
        dns_rr_srv_t *srv = reinterpret_cast<dns_rr_srv_t*>(record+len);
        size_t pos = 0;
        if (readName(record, done, len+sizeof(dns_rr_srv_t)-1, len+rdLen, name, sizeof(name), &pos) == 0) {
          anCnt = 0;
          break;  // skip this one
        }
        name[pos] = 0;
        validPort = ntohs(srv->port) == 9999;
#ifdef DEBUG_MDNS
        printf("  srv port %d target %s\n", ntohs(srv->port), name);
#endif
      } else if (atype == DNS_TYPE_A) {
        // ipv4 address
#ifdef DEBUG_MDNS
        printf("  address %d.%d.%d.%d\n", record[len], record[len+1], record[len+2], record[len+3]);
#endif
        memcpy(reinterpret_cast<uint8_t*>(&validAddress.s_addr), record+len, 4);
      }
      len += rdLen;
    }
    if (!anCnt) {
      continue;
    }
    if (validPort && validAddress.s_addr != INADDR_ANY && validAddress.s_addr != INADDR_NONE && proto[0]) {
      mdns_oneshot_t *storeTo;
      if (!found && (!limitId.length() || limitId.compare(id) == 0)) {
        storeTo = result;
        found = true;
      } else if (found && strcmp(id, result->id) == 0) {
        // skip duplicate answer
        continue;
      } else {
        foundMore = !limitId.length();
        if (moreRemain > 0) {
          storeTo = moreResults++;
          moreRemain--;
          (*moreCount)++;
        } else if (!found) {
          continue;
        } else {
          break;
        }
      }
      storeTo->address = validAddress;
      strncpy(storeTo->id, id, sizeof(mdns_oneshot_t::id));
      strncpy(storeTo->proto, proto, sizeof(mdns_oneshot_t::proto));
      if (found && (limitId.length() || !moreRemain)) {
        break;  // found the desired one or no more space left for others
      }
    }
  }
  close(sock);
  return found ? foundMore ? 2 : 1 : 0;
}

}  // namespace ebusd

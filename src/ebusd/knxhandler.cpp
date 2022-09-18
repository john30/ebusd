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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ebusd/knxhandler.h"
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cmath>
#include <csignal>
#include <deque>
#include "lib/utils/log.h"
#include "lib/ebus/symbol.h"

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif


namespace ebusd {

using std::dec;

// version is coded as:
// 5 bits magic (to be incremented with incompatible changes, not shown)
// 5 bits major, using major directly
// 6 bits minor, using minor multiplied by 10 to have space for micro versioning in future
#define VERSION_INT ((PACKAGE_VERSION_MAJOR << 6) |(PACKAGE_VERSION_MINOR*10))

#define O_URL -2
#define O_AGR (O_URL-1)
#define O_AGW (O_AGR-1)
#define O_INT (O_AGW-1)
#define O_VAR (O_INT-1)

/** the definition of the KNX arguments. */
static const struct argp_option g_knx_argp_options[] = {
  {nullptr,      0, nullptr,      0, "KNX options:", 1 },
  {"knxurl", O_URL, "URL",        0, "URL to open (i.e. \"[multicast][@interface]\" for KNXnet/IP"
#ifdef HAVE_KNXD
                                     " or \"ip:host[:port]\" / \"local:/socketpath\" for knxd"
#endif
                                     ") []", 0 },
  {"knxrage", O_AGR, "SEC",       0, "Maximum age in seconds for using the last value of read messages (0=disable)"
                                     " [5]", 0 },
  {"knxwage", O_AGW, "SEC",       0, "Maximum age in seconds for using the last value for reads on write messages"
                                     " (0=disable), [99999999]", 0 },
  {"knxint", O_INT, "FILE",       0, "Read KNX integration settings from FILE [/etc/ebusd/knx.cfg]", 0 },
  {"knxvar", O_VAR, "NAME=VALUE", 0, "Add a variable to the read KNX integration settings", 0 },

  {nullptr,      0, nullptr,      0, nullptr, 0 },
};

static const char* g_url = nullptr;  //!< URL of KNX daemon
static unsigned int g_maxReadAge = 5;  //!< max age in seconds for using the last value of read messages
// max age in seconds for using the last value for reads on write messages
static unsigned int g_maxWriteAge = 99999999;
static const char* g_integrationFile = nullptr;  //!< the integration settings file
static vector<string>* g_integrationVars = nullptr;  //!< the integration settings variables

/**
 * The KNX argument parsing function.
 * @param key the key from @a g_knx_argp_options.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
static error_t knx_parse_opt(int key, char *arg, struct argp_state *state) {
  result_t result;
  switch (key) {
  case O_URL:  // --knxurl=[multicast][@interface]
    if (arg == nullptr) {  // empty is allowed
      argp_error(state, "invalid knxurl");
      return EINVAL;
    }
    g_url = arg;
    break;

  case O_AGR:  // --knxrage=5
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid knxrage value");
      return EINVAL;
    }
    g_maxReadAge = parseInt(arg, 10, 0, 99999999, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid knxrage");
      return EINVAL;
    }
    break;

  case O_AGW:  // --knxwage=5
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid knxwage value");
      return EINVAL;
    }
    g_maxWriteAge = parseInt(arg, 10, 0, 99999999, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid knxwage");
      return EINVAL;
    }
    break;

  case O_INT:  // --knxint=/etc/ebusd/knx.cfg
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid knxint file");
      return EINVAL;
    }
    g_integrationFile = arg;
    break;

  case O_VAR:  // --knxvar=NAME=VALUE
    if (arg == nullptr || arg[0] == 0 || !strchr(arg, '=')) {
      argp_error(state, "invalid knxvar");
      return EINVAL;
    }
    if (!g_integrationVars) {
      g_integrationVars = new vector<string>();
    }
    g_integrationVars->push_back(string(arg));
    break;

  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static const struct argp g_knx_argp = { g_knx_argp_options, knx_parse_opt, nullptr, nullptr, nullptr, nullptr,
  nullptr };
static const struct argp_child g_knx_argp_child = {&g_knx_argp, 0, "", 1};


const struct argp_child* knxhandler_getargs() {
  return &g_knx_argp_child;
}

bool knxhandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers) {
  if (g_url) {
    handlers->push_back(new KnxHandler(userInfo, busHandler, messages));
  }
  return true;
}

KnxHandler::KnxHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages)
  : DataSink(userInfo, "knx"), DataSource(busHandler), WaitThread(), m_messages(messages),
    m_start(0), m_lastUpdateCheckResult("."),
    m_lastScanStatus(SCAN_STATUS_NONE), m_scanFinishReceived(false), m_lastErrorLogTime(0) {
  m_con = KnxConnection::create(g_url);
  if (g_integrationFile != nullptr) {
    if (!m_replacers.parseFile(g_integrationFile)) {
      logOtherError("knx", "unable to open integration file %s", g_integrationFile);
    }
  }
  if (g_integrationVars) {
    for (auto& str : *g_integrationVars) {
      m_replacers.parseLine(str);
    }
    delete g_integrationVars;
    g_integrationVars = nullptr;
  }
  if (m_con->isProgrammable()) {
    string addrStr = m_replacers.get("address", false);
    knx_addr_t address = 0;
    if (!addrStr.empty()) {
      address = parseAddress(addrStr, false);
      if (!address) {
        logOtherError("knx", "invalid address: %s", addrStr.c_str());
      }
    }
    if (address) {
      m_con->setAddress(address);
    } else {
      logOtherNotice("knx", "address not assigned yet, entering programming mode");
      m_con->setProgrammingMode(true);
    }
  }
  // parse all group to message field assignments
  vector<string> keys = m_replacers.keys();
  int messageCnt = 0, globalCnt = 0;
  for (auto& key : keys) {
    auto pos = key.find('/');
    if (pos == string::npos) {
      continue;
    }
    string val = m_replacers.get(key, false);
    pos = val.find('/');
    if (pos == string::npos) {
      continue;
    }
    auto dest = parseAddress(val);
    if (!dest) {
      logOtherError("knx", "invalid assignment %s to %s", key.c_str(), val.c_str());
      continue;
    }
    if (key.substr(0, 7) != "global/") {
      messageCnt++;
      m_messageFieldGroupAddress[key] = dest;
      continue;
    }
    key = key.substr(7);
    global_t index;
    dtlf_t lengthFlag = DTLF_1BIT;  // default for <=6 bits
    if (key == "version") {
      index = GLOBAL_VERSION;
      lengthFlag.length = 2;
    } else if (key == "running") {
      index = GLOBAL_RUNNING;
    } else if (key == "uptime") {
      index = GLOBAL_UPTIME;
      lengthFlag.length = 4;
    } else if (key == "signal") {
      index = GLOBAL_SIGNAL;
    } else if (key == "scan") {
      index = GLOBAL_SCAN;
    } else if (key == "updatecheck") {
      index = GLOBAL_UPDATECHECK;
    } else {
      logOtherError("knx", "invalid assignment global/%s to %s", key.c_str(), val.c_str());
      continue;
    }
    m_subscribedGlobals[index] = dest|FLAG_READ;
    groupInfo_t grpInfo;
    grpInfo.messageKey = 0;
    grpInfo.globalIndex = index;
    grpInfo.lengthFlag = lengthFlag;
    m_subscribedGroups[dest|FLAG_READ] = grpInfo;
    globalCnt++;
  }
  logOtherInfo("knx", "parsed %d global and %d message assignments", globalCnt, messageCnt);
}

KnxHandler::~KnxHandler() {
  join();
  if (m_con) {
    delete m_con;
    m_con = nullptr;
  }
}

void KnxHandler::startHandler() {
  WaitThread::start("KNX");
}

void KnxHandler::notifyUpdateCheckResult(const string& checkResult) {
  if (checkResult != m_lastUpdateCheckResult) {
    m_lastUpdateCheckResult = checkResult;
    sendGlobalValue(GLOBAL_UPDATECHECK, checkResult.empty() || checkResult == "OK" ? 0 : 1);
  }
}

void KnxHandler::notifyScanStatus(scanStatus_t scanStatus) {
  if (scanStatus == SCAN_STATUS_FINISHED) {
    m_scanFinishReceived = true;
  }
  if (scanStatus != m_lastScanStatus) {
    m_lastScanStatus = scanStatus;
    sendGlobalValue(GLOBAL_SCAN, m_lastScanStatus == SCAN_STATUS_RUNNING ? 1 : 0);
  }
}

result_t getFieldLength(const SingleDataField *field, dtlf_t *length) {
  const auto dt = field->getDataType();
  if (field->isIgnored() || !dt->isNumeric() || dt->isAdjustableLength()) {
    return RESULT_ERR_INVALID_NUM;
  }
  size_t bitCnt = dt->getBitCount();
  if (bitCnt == 1) {
    *length = DTLF_1BIT;
    return RESULT_OK;
  }
  if (bitCnt < 8) {
    *length = DTLF_8BIT;
    return RESULT_OK;
  }
  const auto nt = dynamic_cast<const NumberDataType*>(dt);
  if (nt->getDivisor() != 1) {
    // adjust bit count to 2 octet or 4 octet float DPT
    if (bitCnt >= 24 && bitCnt < 31) {
      bitCnt = 32;
    } else if (bitCnt < 16) {
      bitCnt = 16;
    }
    // TODO uncommon divisor (e.g. >100) may not fit into KNX 2-octet float or truncates precision
  } else if (bitCnt >= 24 && bitCnt < 31) {
    // adjust bit count for non-existent 24 bit KNX type
    bitCnt = 32;
  }
  *length = {{
    .hasDivisor = nt->getDivisor() != 1,
    .isFloat = dt->hasFlag(EXP),
    .isSigned = dt->hasFlag(SIG),
    .lastValueSent = false,
    .length = static_cast<uint8_t>(bitCnt/8),
    .lastValue = 0,
  }};
  return RESULT_OK;
}

uint32_t floatToInt16(float val) {
  // (0.01*m)(2^e) format with sign, 12 bits mantissa (incl. sign), 4 bits exponent
  if (val == 0) {
    return 0;
  }
  bool negative = val < 0;
  if (negative) {
    val = -val;
  }
  val *= 100;
  int exp = ilogb(val)-10;
  if (exp < -10 || exp > 15) {
    return 0x7fff;  // invalid value DPT 9
  }
  auto shift = exp > 0 ? exp : 0;
  auto sig = static_cast<uint32_t>(val * exp2(-shift));
  uint32_t value = static_cast<uint32_t>(shift << 11) | sig;
  if (negative) {
    return value | 0x8000;
  }
  return value;
}

float int16ToFloat(uint16_t val) {
  if (val == 0) {
    return 0;
  }
  if (val == 0x7fff) {
    return static_cast<float>(0xffffffff);  // NaN
  }
  bool negative = val&0x8000;
  int exp = (val>>11)&0xf;
  int sig = val&0x7ff;
  return static_cast<float>(sig * exp2(exp) * (negative ? -0.01 : 0.01));
}

result_t KnxHandler::sendGroupValue(knx_addr_t dest, apci_t apci, dtlf_t& lengthFlag, unsigned int value,
const SingleDataField *field) const {
  if (!m_con || !m_con->isConnected() || !m_con->getAddress()) {
    return RESULT_EMPTY;
  }
  uint8_t data[] = {0, 0, 0, 0, 0, 0};
  data[0] = static_cast<uint8_t>(apci>>8);
  data[1] = static_cast<uint8_t>(apci&0xff);
  int len = 2;
  // convert value to dpt
  if (lengthFlag.isFloat || lengthFlag.hasDivisor) {
    if (!field) {
      return RESULT_ERR_INVALID_NUM;
    }
    auto nt = dynamic_cast<const NumberDataType*>(field->getDataType());
    float fval;
    result_t ret = nt->getFloatFromRawValue(value, &fval);
    if (ret == RESULT_EMPTY) {
      // replacement value:
      if (lengthFlag.length == 2) {
        // shall have 0x7fff for DPT 9
        value = 0x7fff;
      } else {
        return RESULT_ERR_INVALID_NUM;  // not encodable
      }
    } else if (ret != RESULT_OK) {
      return ret;
    } else if (lengthFlag.length == 2) {
      // convert to (0.01*m)(2^e) format with sign, 12 bits mantissa (incl. sign), 4 bits exponent
      value = floatToInt16(fval);
    } else if (lengthFlag.length == 4) {
      // convert to IEEE 754
      value = floatToUint(fval);
    } else {
      return RESULT_ERR_INVALID_NUM;  // not encodable
    }
  }
  // else signed values: fine as long as length is identical
  if (apci == APCI_GROUPVALUE_WRITE && lengthFlag.lastValueSent && lengthFlag.lastValue == value) {
    return RESULT_EMPTY;  // no need to send the same group value again
  }
  lengthFlag.lastValue = value;
  lengthFlag.lastValueSent = true;
  switch (lengthFlag.length) {
    case 0:  // short value <= 6 bit
      data[1] |= static_cast<uint8_t>(value&0x3f);
      break;
    case 1:  // 1 octet
      data[2] = static_cast<uint8_t>(value&0xff);
      break;
    case 2:  // 2 octets
      data[2] = static_cast<uint8_t>(value>>8);
      data[3] = static_cast<uint8_t>(value&0xff);
      break;
    case 4:  // 4 octets
      data[2] = static_cast<uint8_t>(value>>24);
      data[3] = static_cast<uint8_t>(value>>16);
      data[4] = static_cast<uint8_t>(value>>8);
      data[5] = static_cast<uint8_t>(value&0xff);
      break;
    default:
      return RESULT_ERR_INVALID_NUM;
  }
  len += lengthFlag.length;
  const char* err = m_con->sendGroup(dest, len, data);
  if (err) {
    logOtherError("knx", "unable to send %s, dest %4.4x, len %d",
                  apci == APCI_GROUPVALUE_WRITE ? "write" : apci == APCI_GROUPVALUE_READ ? "read" : "response",
                  dest, len);
    return RESULT_ERR_SEND;
  }
  logOtherDebug("knx", "sent %s, dest %4.4x, len %d",
               apci == APCI_GROUPVALUE_WRITE ? "write" : apci == APCI_GROUPVALUE_READ ? "read" : "response",
               dest, len);
  return RESULT_OK;
}

void KnxHandler::sendGlobalValue(global_t index, unsigned int value, bool response) {
  if (!m_con->isConnected() || !m_con->getAddress()) {
    return;
  }
  const auto vit = m_subscribedGlobals.find(index);
  if (vit == m_subscribedGlobals.cend()) {
    return;
  }
  auto git = m_subscribedGroups.find(vit->second);
  if (git == m_subscribedGroups.end()) {
    return;
  }
  sendGroupValue(static_cast<knx_addr_t>(vit->second&0xffff),
                 response ? APCI_GROUPVALUE_RESPONSE : APCI_GROUPVALUE_WRITE,
                 git->second.lengthFlag, value);
}

result_t KnxHandler::receiveTelegram(int maxlen, knx_transfer_t* typ, uint8_t *buf, int *recvlen,
                                     knx_addr_t *src, knx_addr_t *dest) {
  struct timespec tdiff = {
      .tv_sec = 2,
      .tv_nsec = 0,
  };
  if (!m_con->isConnected()) {
    return RESULT_ERR_GENERIC_IO;
  }
  int fd = m_con->getPollFd();
#ifdef HAVE_PPOLL
  nfds_t nfds = 1;
  struct pollfd fds[nfds];
  memset(fds, 0, sizeof(fds));
  fds[0].fd = fd;
  fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
#else
#ifdef HAVE_PSELECT
  fd_set checkfds, exceptfds;
  FD_ZERO(&checkfds);
  FD_SET(fd, &checkfds);
  FD_ZERO(&exceptfds);
  FD_SET(fd, &exceptfds);
#endif
#endif
  int ret;
#ifdef HAVE_PPOLL
  ret = ppoll(fds, nfds, &tdiff, nullptr);
#else
#ifdef HAVE_PSELECT
  fd_set readfds = checkfds;
  ret = pselect(fd + 1, &readfds, nullptr, &exceptfds, &tdiff, nullptr);
#endif
#endif
  bool newData;
#ifdef HAVE_PPOLL
  if (ret < 0 || (ret > 0 && (fds[0].revents & (POLLERR | POLLHUP | POLLRDHUP)))) {
    return RESULT_ERR_GENERIC_IO;
  }
  newData = fds[0].revents & POLLIN;
#else
#ifdef HAVE_PSELECT
  if (ret < 0 || FD_ISSET(fd, &exceptfds)) {
    return RESULT_ERR_GENERIC_IO;
  }
  newData = FD_ISSET(fd, &readfds);
#endif
#endif
  if (!newData) {
    // timeout
    return RESULT_ERR_TIMEOUT;
  }
  *typ = m_con->getPollData(maxlen, buf, recvlen, src, dest);
  return *typ == KNX_TRANSFER_NONE ? RESULT_EMPTY : RESULT_OK;
}

/*
void printResponse(knx_addr_t src, knx_addr_t dest, int len, const uint8_t *data) {
  int tctrl = data[0]>>2;
  int apci = ((data[0]&0x03)<<8) | data[1];
  if ((apci & APCI_GROUPVALUE_READ_MASK) == 0) {
    apci &= ~APCI_GROUPVALUE_READ_MASK;
  }
  int value = len == 2 ? data[1]&0x3f : data[2];  // 6 bits or full octet
  if (len>3) {
    value = (value<<8) | data[3];  // up to 16 bits
  }
  if (len>4) {
    value = (value<<8) | data[4];  // up to 24 bits
  }
  if (len>5) {
    value = (value<<8) | data[5];  // up to 32 bits
  }
  logOtherDebug("knx", "recv from %4.4x to %4.4x, %s (0x%3.3x, tctrl 0x%2.2x), len %d", src, dest,
                apci == APCI_GROUPVALUE_WRITE ? "write" : apci == APCI_GROUPVALUE_READ ? "read"
                  : apci == APCI_GROUPVALUE_RESPONSE ? "response" : "other",
                apci, tctrl, len);
}
*/

void KnxHandler::handleReceivedTelegram(knx_transfer_t typ, knx_addr_t src, knx_addr_t dest, int len,
const uint8_t *data) {
  if (typ == KNX_TRANSFER_GROUP) {
    handleGroupTelegram(src, dest, len, data);
    return;
  }
  if (m_con->isProgrammable() && src && m_con->getAddress()) {
    handleNonGroupTelegram(typ, src, dest, len, data);
  }
}

void KnxHandler::sendNonGroupDisconnect(knx_addr_t dest) {
  uint8_t buf[] = {0x00};
  if (m_con->sendTyp(KNX_TRANSFER_DISCONNECT, dest, 1, buf)) {
    logOtherDebug("knx", "cannot send");
  }
  m_lastConnectTime = 0;  // state=closed
  m_waitForAck = false;
}

// the connection timeout in millis (6 seconds)
#define CONNECTION_TIMEOUT 6000

void KnxHandler::handleNonGroupTelegram(knx_transfer_t typ, knx_addr_t src, knx_addr_t dest, int len,
const uint8_t *data) {
  if (typ == KNX_TRANSFER_NONE) {
    return;
  }
  logOtherNotice("knx", "skipping non-group PDU %3.3x", typ);
}

void KnxHandler::handleGroupTelegram(knx_addr_t src, knx_addr_t dest, int len, const uint8_t *data) {
  time_t now;
  time(&now);
  int apci = ((data[0]&0x03) << 8) | data[1];
  int groupReadWriteApci = apci & APCI_GROUPVALUE_READ_WRITE_MASK;
  if (groupReadWriteApci == APCI_GROUPVALUE_WRITE || groupReadWriteApci == APCI_GROUPVALUE_READ) {
    apci = groupReadWriteApci;
  }
  bool isWrite = apci == APCI_GROUPVALUE_WRITE;
  if (apci != APCI_GROUPVALUE_READ && !isWrite) {
    if (m_con->isProgrammingMode()) {
      if (apci == APCI_INDIVIDUALADDRESS_READ && m_lastIndividualAddressResponseTime < now-3) {  // timeout 3 seconds
        uint8_t buf[] = {APCI_INDIVIDUALADDRESS_RESPONSE >> 8, APCI_INDIVIDUALADDRESS_RESPONSE&0xff};
        logOtherNotice("knx", "answering to A_IndividualAddress_Read");
        if (m_con->sendGroup(0, 2, buf)) {
          logOtherDebug("knx", "cannot send");
        } else {
          m_lastIndividualAddressResponseTime = now;
        }
      } else if (apci == APCI_INDIVIDUALADDRESS_WRITE && len == 4 && !m_con->getAddress() && (data[2]|data[3])) {
        m_con->setAddress(static_cast<knx_addr_t>((data[2] << 8)|data[3]));
        m_lastIndividualAddressResponseTime = 0;
        logOtherNotice("knx", "received new address %x", m_con->getAddress());
      }
    }
    return;  // neither A_GroupValue_Read nor A_GroupValue_Write (A_GroupValue_Response not used at all)
  }
  const auto subKey = static_cast<uint32_t>(dest | (isWrite ? FLAG_WRITE : FLAG_READ));
  auto sit = m_subscribedGroups.find(subKey);
  if (needsLog(lf_other, ll_debug)) {
    logOtherDebug("knx", "received %ssubscribed %s from %4.4x to %4.4x, len %d",
                  sit == m_subscribedGroups.end() ? "un" : "",
                  apci == APCI_GROUPVALUE_WRITE ? "write" : apci == APCI_GROUPVALUE_READ ? "read" : "response",
                  src, dest, len);
  }
  if (sit == m_subscribedGroups.end()) {
    return;  // address+direction not subscribed
  }
  if (sit->second.messageKey == 0) {
    // global values, only readable
    switch (sit->second.globalIndex) {
      case GLOBAL_VERSION:
        sendGlobalValue(GLOBAL_VERSION, VERSION_INT, true);
        break;
      case GLOBAL_RUNNING:
        sendGlobalValue(GLOBAL_RUNNING, 1, true);
        break;
      case GLOBAL_UPTIME:
        sendGlobalValue(GLOBAL_UPTIME, static_cast<unsigned>(time(nullptr) - m_start), true);
        break;
      case GLOBAL_SIGNAL:
        sendGlobalValue(GLOBAL_SIGNAL, m_busHandler->hasSignal() ? 1 : 0, true);
        break;
      case GLOBAL_SCAN:
        sendGlobalValue(GLOBAL_SCAN, m_lastScanStatus == SCAN_STATUS_RUNNING ? 1 : 0, true);
        break;
      case GLOBAL_UPDATECHECK:
        sendGlobalValue(GLOBAL_UPDATECHECK, m_lastUpdateCheckResult.empty() || m_lastUpdateCheckResult == "OK"
        || m_lastUpdateCheckResult == "." ? 0 : 1, true);
        break;
      default:
        return;  // ignore
    }
    return;
  }
  const vector<Message*>* messages = m_messages->getByKey(sit->second.messageKey);
  if (!messages) {
    return;
  }
  Message *msg = nullptr;
  ssize_t fieldIndex = sit->second.fieldIndex;
  const SingleDataField* field = nullptr;
  for (const auto& message : *messages) {
    if (!message->isAvailable() || message->getDstAddress() == SYN) {
      continue;
    }
    if ((message->isWrite() && !message->isPassive()) != isWrite) {
      if (isWrite || message->getLastUpdateTime() <= 0) {
        continue;
      }  // else: allow potential "write-read" association to read the last written value
    }
    field = message->getField(fieldIndex);
    if (!field) {
      continue;
    }
    if (isWrite) {
      msg = message;
      break;  // best candidate
    }
    if (!msg) {
      msg = message;
    } else if (message->getLastUpdateTime() > 0
    && message->getLastUpdateTime() > msg->getLastUpdateTime()) {
      // prefer newer updated, even if it is passive
      msg = message;
    } else if (!message->isPassive()) {
      // prefer active read before passive
      msg = message;
    }
  }
  if (!msg) {
    logOtherInfo("knx", "unable to answer %s request to %4.4x", isWrite ? "write" : "read", dest);
    return;
  }
  result_t res;
  const string circuit = msg->getCircuit(), name = msg->getName(), fieldName = msg->getFieldName(fieldIndex);
  if (isWrite) {
    unsigned int value = len == 2 ? data[1]&0x3f : data[2];  // <=6 bits or full octet
    if (len > 3) {
      value = (value << 8) | data[3];  // up to 16 bits
    }
    if (len > 4) {
      value = (value << 8) | data[4];  // up to 24 bits
    }
    if (len > 5) {
      value = (value << 8) | data[5];  // up to 32 bits
    }
    // note: a write from KNX updates the message and thus re-sends the write later on again during update check
    logOtherNotice("knx", "received write request from %4.4x to %4.4x for %s/%s/%s, value %d",
                   src, dest, circuit.c_str(), name.c_str(), fieldName.c_str(), value);
    // write new field value to bus if possible
    // ugly but least intrusive: format single num field value to string to have it parsed back later on
    ostringstream str;
    // convert value to dpt
    auto lengthFlag = sit->second.lengthFlag;
    if (lengthFlag.isFloat || lengthFlag.hasDivisor) {
      float fval;
      if (lengthFlag.length == 2) {
        // convert from (0.01*m)(2^e) format with sign, 12 bits mantissa (incl. sign), 4 bits exponent
        fval = int16ToFloat(static_cast<uint16_t>(value));
      } else if (lengthFlag.length == 4) {
        // convert from IEEE 754
        bool negative = (value & (1u << 31)) != 0;
        fval = uintToFloat(value, negative);
      } else {
        logOtherNotice("knx", "unable to decode write request from %4.4x to %4.4x for %s/%s/%s, value %d",
                       src, dest, circuit.c_str(), name.c_str(), fieldName.c_str(), value);
        return;  // not decodable
      }
      str << static_cast<float>(fval);
    } else {
      if (lengthFlag.isSigned) {
        // signed values: determine sign
        uint32_t bit = 1 << (lengthFlag.length*8-1);
        if (value & bit) {
          value = -(value&~bit);
        }
        str << static_cast<int>(value);
      } else {
        str << static_cast<uint32_t>(value);
      }
    }
    res = m_busHandler->readFromBus(msg, str.str());
    if (res == RESULT_OK) {
      logOtherDebug("knx", "wrote %s %s", circuit.c_str(), name.c_str());
    } else {
      logOtherError("knx", "write %s %s: %s", circuit.c_str(), name.c_str(), getResultCode(res));
    }
    return;
  }
  logOtherNotice("knx", "received read request from %4.4x to %4.4x for %s/%s/%s",
                 src, dest, circuit.c_str(), name.c_str(), fieldName.c_str());
  if (msg->isWrite() && !msg->isPassive()) {  // reading last value of a write message
    if (now >= msg->getLastUpdateTime() + g_maxWriteAge) {
      logOtherInfo("knx", "unable to answer read request to %4.4x on write message", dest);
      return;  // impossible to answer
    }
  } else if (now >= msg->getLastUpdateTime() + g_maxReadAge) {
    res = m_busHandler->readFromBus(msg, "");
    if (res != RESULT_OK) {
      return;
    }
  }
  unsigned int value = 0;
  res = msg->decodeLastDataNumField(nullptr, fieldIndex, &value);
  if (res == RESULT_OK) {
    logOtherDebug("knx", "read %s %s", circuit.c_str(), name.c_str());
    res = sendGroupValue(dest, APCI_GROUPVALUE_RESPONSE, sit->second.lengthFlag, value, field);
  } else {
    logOtherError("knx", "read %s %s: %s", circuit.c_str(), name.c_str(), getResultCode(res));
  }
}

// interval in seconds for sending the uptime value
#define UPTIME_INTERVAL 3600

void KnxHandler::run() {
  time_t lastTaskRun, now, lastSignal = 0, lastUptime = 0, lastUpdates = 0;
  bool signal = false;
  result_t result = RESULT_OK;
  time(&now);
  m_start = lastTaskRun = now;
  uint8_t data[256];
  int len = 0;
  time_t definitionsSince = 0;
  while (isRunning()) {
    bool wasConnected = m_con->isConnected();
    bool needsWait = true;
    if (!wasConnected) {
      const char* err = m_con->open();
      if (!err) {
        m_lastErrorLogTime = 0;
        logOtherNotice("knx", "connected to %s", m_con->getInfo());
        sendGlobalValue(GLOBAL_VERSION, VERSION_INT);
        sendGlobalValue(GLOBAL_RUNNING, 1);
      }
      if (err) {
        m_con->close();
        time(&now);
        if (now > m_lastErrorLogTime + 10) {  // log at most every 10 seconds
          m_lastErrorLogTime = now;
          logOtherError("knx", err);
        }
      }
    }
    bool reconnected = !wasConnected && m_con->isConnected();
    time(&now);
    bool sendSignal = reconnected;
    if (now < m_start) {
      // clock skew
      if (now < lastSignal) {
        lastSignal -= lastTaskRun-now;
      }
      lastTaskRun = now;
    } else if (now > lastTaskRun+(m_scanFinishReceived ? 1 : 15)) {
      m_scanFinishReceived = false;
      if (m_con->isConnected()) {
        sendSignal = true;
        if (now > lastUptime + UPTIME_INTERVAL) {
          lastUptime = now;
          sendGlobalValue(GLOBAL_UPTIME, static_cast<unsigned int>(now - m_start));
        }
      }
      if (m_con->isConnected() && definitionsSince == 0) {
        definitionsSince = 1;
      }
      if (m_con->isConnected()) {
        deque<Message*> messages;
        m_messages->findAll("", "", m_levels, false, true, true, true, true, true, 0, 0, true, &messages);
        int addCnt = 0;
        for (const auto& message : messages) {
          const auto mit = m_subscribedMessages.find(message->getKey());
          if (mit != m_subscribedMessages.cend()) {
            continue;  // already subscribed
          }
          if (message->getDstAddress() == SYN) {
            continue;  // not usable in absence of destination address
          }
          bool isWrite = message->isWrite() && !message->isPassive();  // from KNX perspective
          if (message->getCreateTime() <= definitionsSince) {  // only newer defined
            continue;
          }
          ssize_t fieldCount = static_cast<signed>(message->getFieldCount());
          if (isWrite && fieldCount > 1) {
            // impossible with more than one field
            continue;
          }
          bool added = false;
          for (ssize_t index = 0; index < fieldCount; index++) {
            const SingleDataField* field = message->getField(index);
            if (!field || field->isIgnored()) {
              continue;
            }
            string fieldName = message->getFieldName(index);
            if (fieldName.empty() && fieldCount == 1) {
              fieldName = "0";  // might occur for unnamed single field sets
            }
            string key = message->getCircuit()+"/"+message->getName()+"/"+fieldName;
            const auto git = m_messageFieldGroupAddress.find(key);
            if (git == m_messageFieldGroupAddress.cend()) {
              continue;
            }
            // determine field length in telegram
            dtlf_t lengthFlag = {};
            result = getFieldLength(field, &lengthFlag);
            if (result != RESULT_OK) {
              continue;
            }
            // store association
            knx_addr_t dest = git->second;
            auto subKey = static_cast<uint32_t>(dest | (isWrite ? FLAG_WRITE : FLAG_READ));
            auto sit = m_subscribedGroups.find(subKey);
            if (sit != m_subscribedGroups.cend()) {
              if (isWrite) {
                logOtherDebug("knx", "ignored already subscribed %s", key.c_str());
                continue;
              }
              if (sit->second.messageKey == message->getKey()) {
                continue;
              }  // else: overwrite "write-read" with readable message
              logOtherDebug("knx", "replacing write-read association %s to %4.4x", key.c_str(), dest);
            }
            groupInfo_t grpInfo;
            grpInfo.messageKey = message->getKey();
            grpInfo.globalIndex = static_cast<global_t>(index);
            grpInfo.lengthFlag = lengthFlag;
            m_subscribedGroups[subKey] = grpInfo;
            m_subscribedMessages[message->getKey()].push_back(subKey);
            logOtherDebug("knx", "added %s association %s to %4.4x", isWrite ? "write" : "read", key.c_str(), dest);
            if (isWrite) {
              // add "write-read" association to allow reading the last written value of a writable message
              // when there is no readable message set directly yet
              subKey = static_cast<uint32_t>(dest | FLAG_READ);
              sit = m_subscribedGroups.find(subKey);
              if (sit == m_subscribedGroups.cend()) {
                m_subscribedGroups[subKey] = grpInfo;
                logOtherDebug("knx", "added write-read association %s to %4.4x", key.c_str(), dest);
              }
            }
            added = true;
            addCnt++;
          }
          if (!added) {
            continue;
          }
          if (message->getLastUpdateTime() > message->getCreateTime()) {
            // ensure data is published as well
            m_updatedMessages[message->getKey()]++;
          } else if (message->isWrite()) {
            // publish data for read pendant of write message
            Message* read = m_messages->find(message->getCircuit(), message->getName(), "", false);
            if (read && read->getLastUpdateTime() > 0) {
              m_updatedMessages[read->getKey()]++;
            }
          }
        }
        if (addCnt > 0) {
          logOtherInfo("knx", "added %d associations, %d active now", addCnt, m_subscribedGroups.size());
        }
        definitionsSince = now;
        needsWait = true;
      }
      time(&lastTaskRun);
    }
    if (sendSignal) {
      if (m_busHandler->hasSignal()) {
        lastSignal = now;
        if (!signal || reconnected) {
          signal = true;
          sendGlobalValue(GLOBAL_SIGNAL, 1);
        }
      } else {
        if (signal || reconnected) {
          signal = false;
          sendGlobalValue(GLOBAL_SIGNAL, 0);
        }
      }
    }
    if (m_con->isConnected()) {
      if (reconnected) {
        // reset the state machine
        m_lastConnectTime = 0;
        m_waitForAck = false;
      }
      handleReceivedTelegram(KNX_TRANSFER_NONE, 1, 0, 0, data);  // check timeout
      knx_addr_t src, dest;
      knx_transfer_t typ;
      // APDU data starting with octet 6 according to spec, contains 2 bits of application layer
      result_t res = RESULT_OK;
      do {
        res = receiveTelegram(sizeof(data), &typ, data, &len, &src, &dest);
        if (res != RESULT_OK) {
          if (res == RESULT_ERR_GENERIC_IO) {
            m_con->close();
          }
        } else {
          needsWait = false;
          handleReceivedTelegram(typ, src, dest, len, data);
        }
      } while (res == RESULT_OK);
    }
    if (!m_updatedMessages.empty()) {
      m_messages->lock();
      if (m_con->isConnected()) {
        for (auto it = m_updatedMessages.begin(); it != m_updatedMessages.end(); ) {
          const vector<Message*>* messages = m_messages->getByKey(it->first);
          if (!messages) {
            continue;
          }
          for (const auto& message : *messages) {
            if (message->getLastChangeTime() <= 0) {
              continue;
            }
            const auto mit = m_subscribedMessages.find(message->getKey());
            if (mit == m_subscribedMessages.cend()) {
              continue;
            }
            if (!(message->getDataHandlerState()&2)) {
              message->setDataHandlerState(2, true);  // first update still needed
            } else if (message->getLastChangeTime() <= lastUpdates) {
              continue;
            }
            for (auto destFlags : mit->second) {
              auto sit = m_subscribedGroups.find(destFlags);
              if (sit == m_subscribedGroups.end()) {
                continue;
              }
              ssize_t index = sit->second.fieldIndex;
              const SingleDataField *field = message->getField(index);
              if (!field || field->isIgnored()) {
                continue;
              }
              knx_addr_t dest = destFlags&0xffff;
              unsigned int value = 0;
              result = message->decodeLastDataNumField(nullptr, index, &value);
              sendGroupValue(dest, APCI_GROUPVALUE_WRITE, sit->second.lengthFlag, value, field);
            }
          }
          it = m_updatedMessages.erase(it);
        }
        time(&lastUpdates);
      } else {
        m_updatedMessages.clear();
      }
      m_messages->unlock();
    }
    if ((!m_con->isConnected() && !Wait(5)) || (needsWait && !Wait(0, 100))
    ) {
      break;
    }
  }
  sendGlobalValue(GLOBAL_RUNNING, 0);
  sendGlobalValue(GLOBAL_SIGNAL, 0);
  sendGlobalValue(GLOBAL_SCAN, 0);
}

}  // namespace ebusd

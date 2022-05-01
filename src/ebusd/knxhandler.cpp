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

namespace ebusd {

using std::dec;

// version is coded as:
// 5 bits magic (to be incremented with incompatible changes, not shown)
// 5 bits major, using major directly
// 6 bits minor, using minor multiplied by 10 to have space for micro versioning in future
#define VERSION_INT ((PACKAGE_VERSION_MAJOR<<6)|(PACKAGE_VERSION_MINOR*10))

#define O_URL 1
#define O_INT (O_URL+1)
#define O_VAR (O_INT+1)

/** the definition of the KNX arguments. */
static const struct argp_option g_knx_argp_options[] = {
  {nullptr,      0, nullptr,      0, "KNX options:", 1 },
  {"knxurl", O_URL,   "URL",      0, "Connect to KNX daemon on URL (i.e. \"ip:host:[port]\" or \"local:/socketpath\") []", 0 },
  {"knxint", O_INT,  "FILE",      0, "Read KNX integration settings from FILE [/etc/ebusd/knx.cfg]", 0 },
  {"knxvar", O_VAR, "NAME=VALUE", 0, "Add a variable to the read KNX integration settings", 0 },

  {nullptr,      0, nullptr,      0, nullptr, 0 },
};

static const char* g_url = nullptr;  //!< URL of KNX daemon
static const char* g_integrationFile = nullptr;  //!< the integration settings file
static vector<string>* g_integrationVars = nullptr;  //!< the integration settings variables

/**
 * The KNX argument parsing function.
 * @param key the key from @a g_knx_argp_options.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
static error_t knx_parse_opt(int key, char *arg, struct argp_state *state) {
  switch (key) {
  case O_URL:  // --knxurl=localhost
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid knxurl");
      return EINVAL;
    }
    g_url = arg;
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
    m_start(0), m_con(nullptr), m_lastUpdateCheckResult("."),
    m_lastScanStatus(SCAN_STATUS_NONE), m_scanFinishReceived(false), m_lastErrorLogTime(0) {
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
  // parse all group to message field assignments
  vector<string> keys = m_replacers.keys();
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
    auto pos2 = val.find('/', pos+1);
    result_t res = RESULT_OK;
    unsigned int v;
    v = parseInt(val.substr(0, pos).c_str(), 10, 0, 0x1f, &res);
    if (res != RESULT_OK) {
      continue;
    }
    auto dest = static_cast<eibaddr_t>(v << 11);
    if (pos2 == string::npos) {
      // 2 level
      v = parseInt(val.substr(pos+1).c_str(), 10, 0, 0x7ff, &res);
      if (res != RESULT_OK) {
        continue;
      }
      dest |= static_cast<eibaddr_t>(v);
    } else {
      // 3 level
      v = parseInt(val.substr(pos+1, pos2).c_str(), 10, 0, 0x07, &res);
      if (res != RESULT_OK) {
        continue;
      }
      dest |= static_cast<eibaddr_t>(v << 8);
      v = parseInt(val.substr(pos+1, pos2).c_str(), 10, 0, 0xff, &res);
      if (res != RESULT_OK) {
        continue;
      }
      dest |= static_cast<eibaddr_t>(v);
    }
    if (key.substr(0, 7) != "global/") {
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
      continue;
    }
    m_subscribedGlobals[index] = dest|FLAG_READ;
    m_subscribedGroups[dest|FLAG_READ] = {
        .messageKey = 0,
        .globalIndex = index,
        .lengthFlag = lengthFlag,
    };
  }
}

KnxHandler::~KnxHandler() {
  join();
  if (m_con) {
    EIBClose(m_con);
    m_con = nullptr;
  }
}

void KnxHandler::startHandler() {
  WaitThread::start("KNX");
}

void KnxHandler::notifyUpdateCheckResult(const string& checkResult) {
  if (checkResult != m_lastUpdateCheckResult) {
    m_lastUpdateCheckResult = checkResult;
    sendGlobalValue(GLOBAL_UPDATECHECK, checkResult.empty() || checkResult=="OK" ? 0 : 1);
  }
}

void KnxHandler::notifyScanStatus(scanStatus_t scanStatus) {
  if (scanStatus == SCAN_STATUS_FINISHED) {
    m_scanFinishReceived = true;
  }
  if (scanStatus != m_lastScanStatus) {
    m_lastScanStatus = scanStatus;
    sendGlobalValue(GLOBAL_SCAN, m_lastScanStatus==SCAN_STATUS_RUNNING ? 1 : 0);
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
  if (nt->getDivisor()!=1) {
    // adjust bit count to 2 octet or 4 octet float DPT
    if (bitCnt>=24 && bitCnt<31) {
      bitCnt = 32;
    } else if (bitCnt<16) {
      bitCnt = 16;
    }
    // TODO uncommon divisor (e.g. >100) may not fit into KNX 2-octet float or truncates precision
  } else if (bitCnt>=24 && bitCnt<31) {
    // adjust bit count for non-existent 24 bit KNX type
    bitCnt = 32;
  }
  *length = {{
    .hasDivisor = nt->getDivisor()!=1,
    .isFloat = dt->hasFlag(EXP),
    .isSigned = dt->hasFlag(SIG),
    .length = static_cast<uint8_t>(bitCnt/8),
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
    return static_cast<float>(0xffffffff); // NaN
  }
  bool negative = val&0x8000;
  int exp = (val>>11)&0xf;
  int sig = val&0x7ff;
  return static_cast<float>(sig * exp2(exp) * (negative ? -0.01 : 0.01));
}

result_t KnxHandler::sendGroupValue(eibaddr_t dest, apci_t apci, dtlf_t& lengthFlag, unsigned int value, const SingleDataField *field) const {
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
      if (lengthFlag.length==2) {
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
  if (apci==APCI_GROUPVALUE_WRITE && lengthFlag.lastValueSent && lengthFlag.lastValue==value) {
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
  if (EIBSendGroup(m_con, dest, len, data) < 0) {
    return RESULT_ERR_SEND;
  }
  return RESULT_OK;
}

void KnxHandler::sendGlobalValue(global_t index, unsigned int value, bool response) {
  if (!m_con) {
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
  sendGroupValue(static_cast<eibaddr_t>(vit->second&0xffff),
                 response ? APCI_GROUPVALUE_RESPONSE : APCI_GROUPVALUE_WRITE,
                 git->second.lengthFlag, value);
}

result_t KnxHandler::receiveTelegram(int maxlen, uint8_t *buf, int *recvlen,
                                  eibaddr_t *src, eibaddr_t *dest) {
  struct timespec tdiff = {
      .tv_sec = 2,
      .tv_nsec = 0,
  };
  int fd = EIB_Poll_FD(m_con);
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
  int len = EIB_Poll_Complete(m_con);
  if (len == -1) {
    // read failed
    return RESULT_ERR_GENERIC_IO;
  }
  if (!newData) {
    // timeout
    return RESULT_ERR_TIMEOUT;
  }
  len = EIBGetGroup_Src(m_con, maxlen, buf, src, dest);
  if (len < 0) {
    return RESULT_ERR_GENERIC_IO;
  }
  if (len < 2) {
    return RESULT_ERR_GENERIC_IO;
  }
  *recvlen = len;
  return RESULT_OK;
}

void printResponse(eibaddr_t src, eibaddr_t dest, int len, const uint8_t *data) {
  int apci = ((data[0]&0x03)<<2) | ((data[1]&0xc0)>>6);
  int value = len==2 ? data[1]&0x3f : data[2]; // 6 bits or full octet
  if (len>3) {
    value = (value<<8) | data[3]; // up to 16 bits
  }
  if (len>4) {
    value = (value<<8) | data[4]; // up to 24 bits
  }
  if (len>5) {
    value = (value<<8) | data[5]; // up to 32 bits
  }
  logOtherDebug("knx", "recv from %4.4x to %4.4x, %d=%s, len %d, value %d", src, dest,
                apci, apci==0?"read":apci==2?"write":apci==1?"resp":"other", len, value);
}

void KnxHandler::handleReceivedTelegram(eibaddr_t src, eibaddr_t dest, int len, const uint8_t *data) {
  int apci = ((data[0]&0x03)<<8) | data[1];
  if ((apci & APCI_GROUPVALUE_READ_MASK) == 0) {
    apci &= ~APCI_GROUPVALUE_READ_MASK;
  }
  bool isWrite = apci==APCI_GROUPVALUE_WRITE;
  if (apci!=APCI_GROUPVALUE_READ && !isWrite) {
    return;  // neither A_GroupValue_Read nor A_GroupValue_Write (A_GroupValue_Response not used at all)
  }
  const auto subKey = static_cast<uint32_t>(dest | (isWrite ? FLAG_WRITE : FLAG_READ));
  auto sit = m_subscribedGroups.find(subKey);
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
        sendGlobalValue(GLOBAL_SCAN, m_lastScanStatus==SCAN_STATUS_RUNNING ? 1 : 0, true);
        break;
      case GLOBAL_UPDATECHECK:
        sendGlobalValue(GLOBAL_UPDATECHECK, m_lastUpdateCheckResult.empty() || m_lastUpdateCheckResult=="OK" || m_lastUpdateCheckResult=="." ? 0 : 1, true);
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
      continue;
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
    return;
  }
  result_t res;
  const string circuit = msg->getCircuit(), name = msg->getName(), fieldName = msg->getFieldName(fieldIndex);
  if (isWrite) {
    unsigned int value = len==2 ? data[1]&0x3f : data[2]; // <=6 bits or full octet
    if (len>3) {
      value = (value<<8) | data[3]; // up to 16 bits
    }
    if (len>4) {
      value = (value<<8) | data[4]; // up to 24 bits
    }
    if (len>5) {
      value = (value<<8) | data[5]; // up to 32 bits
    }
    // TODO write from KNX updates the message and thus re-sends the write later on again
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
        fval = uintToFloat(value);
      } else {
        return;  // not decodable
      }
      str << static_cast<float>(fval);
    } else {
      if (lengthFlag.isSigned) {
        // signed values: determine sign
        uint32_t bit = 1<<(lengthFlag.length*8-1);
        if (value & bit) {
          value = -(value&~bit);
        }
        str << static_cast<int>(value);
      } else {
        str << static_cast<uint32_t>(value);
      }
    }
    res = m_busHandler->readFromBus(msg, str.str());
    if (res != RESULT_OK) {
      logOtherError("knx", "write %s %s: %s", circuit.c_str(), name.c_str(), getResultCode(res));
    }
    return;
  }
  logOtherNotice("knx", "received read request from %4.4x to %4.4x for %s/%s/%s",
                 src, dest, circuit.c_str(), name.c_str(), fieldName.c_str());
  if (msg->getLastUpdateTime() <= 0) {  // TODO adjustable max age
    res = m_busHandler->readFromBus(msg, "");
    if (res != RESULT_OK) {
      return;
    }
  }
  unsigned int value = 0;
  res = msg->decodeLastDataNumField(nullptr, fieldIndex, &value);
  if (res == RESULT_OK) {
    res = sendGroupValue(dest, APCI_GROUPVALUE_RESPONSE, sit->second.lengthFlag, value, field);
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
  uint8_t data[] = {0, 0, 0, 0, 0, 0, 0, 0};
  int len = 0;
  time_t definitionsSince = 0;
  while (isRunning()) {
    bool wasConnected = m_con != nullptr;
    bool needsWait = true;
    if (!m_con) {
      m_con = EIBSocketURL(g_url);
      const char* err = nullptr;
      if (!m_con) {
        err = "open error";
      } else if (EIBOpen_GroupSocket(m_con, 0) < 0) {
        err = "open group error";
        EIBClose_sync(m_con);
        m_con = nullptr;
      } else {
        m_lastErrorLogTime = 0;
        logOtherNotice("knx", "connected");
        sendGlobalValue(GLOBAL_VERSION, VERSION_INT);
        sendGlobalValue(GLOBAL_RUNNING, 1);
      }
      if (err) {
        time(&now);
        if (now > m_lastErrorLogTime + 10) {  // log at most every 10 seconds
          m_lastErrorLogTime = now;
          logOtherError("knx", err);
        }
      }
    }
    bool reconnected = !wasConnected && m_con != nullptr;
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
      if (m_con) {
        sendSignal = true;
        if (now > lastUptime + UPTIME_INTERVAL) {
          lastUptime = now;
          sendGlobalValue(GLOBAL_UPTIME, static_cast<unsigned int>(now - m_start));
        }
      }
      if (m_con && definitionsSince == 0) {
        definitionsSince = 1;
      }
      if (m_con) {
        deque<Message*> messages;
        m_messages->findAll("", "", m_levels, false, true, true, true, true, true, 0, 0, true, &messages);
        for (const auto& message : messages) {
          const auto mit = m_subscribedMessages.find(message->getKey());
          if (mit != m_subscribedMessages.cend()) {
            continue;  // already subscribed
          }
          if (message->getDstAddress() == SYN) {
            continue;
          }
          bool isWrite = message->isWrite() && !message->isPassive();  // from KNX perspective
          if (message->getCreateTime() <= definitionsSince) {  // only newer defined
            continue;
          }
          ssize_t fieldCount = static_cast<signed>(message->getFieldCount());
          if (isWrite && fieldCount>1) {
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
            // TODO add "foreign" associations as well, i.e. read for a write msg and write for read msg?
            eibaddr_t dest = git->second;
            auto subKey = static_cast<uint32_t>(dest | (isWrite ? FLAG_WRITE : FLAG_READ));
            auto sit = m_subscribedGroups.find(subKey);
            if (sit != m_subscribedGroups.cend()) {
              continue;
            }
            m_subscribedGroups[subKey] = {
                .messageKey = message->getKey(),
                .fieldIndex = static_cast<uint8_t>(index),
                .lengthFlag = lengthFlag,
            };
            m_subscribedMessages[message->getKey()].push_back(subKey);
            added = true;
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
    if (m_con) {
      eibaddr_t src, dest;
      // APDU data starting with octet 6 according to spec, contains 2 bits of application layer
      result_t res = RESULT_OK;
      do {
        res = receiveTelegram(8, data, &len, &src, &dest);
        if (res != RESULT_OK) {
          if (res == RESULT_ERR_GENERIC_IO) {
            EIBClose_sync(m_con);
            m_con = nullptr;
          }
        } else {
          needsWait = false;
          printResponse(src, dest, len, data);
          handleReceivedTelegram(src, dest, len, data);
        }
      } while (res == RESULT_OK);
    }
    if (!m_updatedMessages.empty()) {
      m_messages->lock();
      if (m_con) {
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
              bool isWrite = (destFlags&FLAG_WRITE)!=0;  // from KNX perspective
              auto sit = m_subscribedGroups.find(destFlags);
              if (sit == m_subscribedGroups.end()) {
                continue;
              }
              ssize_t index = sit->second.fieldIndex;
              const SingleDataField *field = message->getField(index);
              if (!field || field->isIgnored()) {
                continue;
              }
              eibaddr_t dest = destFlags&0xffff;
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
    if ((!m_con && !Wait(5)) || (needsWait && !Wait(1))) {
      break;
    }
  }
  sendGlobalValue(GLOBAL_RUNNING, 0);
  sendGlobalValue(GLOBAL_SIGNAL, 0);
  sendGlobalValue(GLOBAL_SCAN, 0);
}

}  // namespace ebusd

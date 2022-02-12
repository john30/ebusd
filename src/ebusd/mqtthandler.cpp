/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2022 John Baier <ebusd@ebusd.eu>
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

#include "ebusd/mqtthandler.h"
#include <csignal>
#include <deque>
#include "lib/utils/log.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

using std::dec;

#define O_HOST 1
#define O_PORT (O_HOST+1)
#define O_CLID (O_PORT+1)
#define O_USER (O_CLID+1)
#define O_PASS (O_USER+1)
#define O_TOPI (O_PASS+1)
#define O_GTOP (O_TOPI+1)
#define O_RETA (O_GTOP+1)
#define O_PQOS (O_RETA+1)
#define O_INTF (O_PQOS+1)
#define O_IVAR (O_INTF+1)
#define O_JSON (O_IVAR+1)
#define O_LOGL (O_JSON+1)
#define O_VERS (O_LOGL+1)
#define O_IGIN (O_VERS+1)
#define O_CHGS (O_IGIN+1)
#define O_CAFI (O_CHGS+1)
#define O_CERT (O_CAFI+1)
#define O_KEYF (O_CERT+1)
#define O_KEPA (O_KEYF+1)
#define O_INSE (O_KEPA+1)
#define O_VERB (O_INSE+1)

/** the definition of the MQTT arguments. */
static const struct argp_option g_mqtt_argp_options[] = {
  {nullptr,        0,      nullptr,       0, "MQTT options:", 1 },
  {"mqtthost",     O_HOST, "HOST",        0, "Connect to MQTT broker on HOST [localhost]", 0 },
  {"mqttport",     O_PORT, "PORT",        0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]", 0 },
  {"mqttclientid", O_CLID, "ID",          0, "Set client ID for connection to MQTT broker [" PACKAGE_NAME "_"
   PACKAGE_VERSION "_<pid>]", 0 },
  {"mqttuser",     O_USER, "USER",        0, "Connect as USER to MQTT broker (no default)", 0 },
  {"mqttpass",     O_PASS, "PASSWORD",    0, "Use PASSWORD when connecting to MQTT broker (no default)", 0 },
  {"mqtttopic",    O_TOPI, "TOPIC",       0,
   "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]", 0 },
  {"mqttglobal",   O_GTOP, "TOPIC",       0, "Use TOPIC for global data (default is \"global/\" suffix to mqtttopic prefix)", 0 },
  {"mqttretain",   O_RETA, nullptr,       0, "Retain all topics instead of only selected global ones", 0 },
  {"mqttqos",      O_PQOS, "QOS",         0, "Set the QoS value for all topics (0-2) [0]", 0 },
  {"mqttint",      O_INTF, "FILE",        0, "Read MQTT integration settings from FILE (no default)", 0 },
  {"mqttvar",      O_IVAR, "NAME=VALUE[,...]", 0, "Add variable(s) to the read MQTT integration settings", 0 },
  {"mqttjson",     O_JSON, nullptr,       0, "Publish in JSON format instead of strings", 0 },
  {"mqttverbose",  O_VERB, nullptr,       0, "Publish all available attributes", 0 },
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
  {"mqttlog",      O_LOGL, nullptr,       0, "Log library events", 0 },
#endif
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1004001)
  {"mqttversion",  O_VERS, "VERSION",     0, "Use protocol VERSION [3.1]", 0 },
#endif
  {"mqttignoreinvalid", O_IGIN, nullptr, 0,
   "Ignore invalid parameters during init (e.g. for DNS not resolvable yet)", 0 },
  {"mqttchanges",  O_CHGS, nullptr,       0, "Whether to only publish changed messages instead of all received", 0 },

#if (LIBMOSQUITTO_MAJOR >= 1)
  {"mqttca",       O_CAFI, "CA",          0, "Use CA file or dir (ending with '/') for MQTT TLS (no default)", 0 },
  {"mqttcert",     O_CERT, "CERTFILE",    0, "Use CERTFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkey",      O_KEYF, "KEYFILE",     0, "Use KEYFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkeypass",  O_KEPA, "PASSWORD",    0, "Use PASSWORD for the encrypted KEYFILE (no default)", 0 },
  {"mqttinsecure", O_INSE, nullptr,       0, "Allow insecure TLS connection (e.g. using a self signed certificate)", 0 },
#endif

  {nullptr,        0,      nullptr,       0, nullptr, 0 },
};

static const char* g_host = "localhost";  //!< host name of MQTT broker [localhost]
static uint16_t g_port = 0;               //!< optional port of MQTT broker, 0 to disable [0]
static const char* g_clientId = nullptr;  //!< optional clientid override for MQTT broker
static const char* g_username = nullptr;  //!< optional user name for MQTT broker (no default)
static const char* g_password = nullptr;  //!< optional password for MQTT broker (no default)
static const char* g_topic = nullptr;     //!< optional topic template
static const char* g_globalTopic = nullptr; //!< optional global topic
static const char* g_integrationFile = nullptr;  //!< the integration settings file
static const char* g_integrationVars = nullptr;  //!< the integration settings variables
static bool g_retain = false;             //!< whether to retail all topics
static int g_qos = 0;                     //!< the qos value for all topics
static OutputFormat g_publishFormat = OF_NONE;  //!< the OutputFormat for publishing messages
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
static bool g_logFromLib = false;         //!< log library events
#endif
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1004001)
static int g_version = MQTT_PROTOCOL_V31;  //!< protocol version to use
#endif
static bool g_ignoreInvalidParams = false;  //!< ignore invalid parameters during init
static bool g_onlyChanges = false;        //!< whether to only publish changed messages instead of all received

#if (LIBMOSQUITTO_MAJOR >= 1)
static const char* g_cafile = nullptr;    //!< CA file for TLS
static const char* g_capath = nullptr;    //!< CA path for TLS
static const char* g_certfile = nullptr;  //!< client certificate file for TLS
static const char* g_keyfile = nullptr;   //!< client key file for TLS
static const char* g_keypass = nullptr;   //!< client key file password for TLS
static bool g_insecure = false;           //!< whether to allow insecure TLS connection
#endif

static char* replaceSecret(char *arg) {
  char* ret = strdup(arg);
  int cnt = 0;
  while (*arg && cnt++ < 256) {
    *arg++ = ' ';
  }
  return ret;
}

/**
 * The MQTT argument parsing function.
 * @param key the key from @a g_mqtt_argp_options.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
static error_t mqtt_parse_opt(int key, char *arg, struct argp_state *state) {
  result_t result = RESULT_OK;

  switch (key) {
  case O_HOST:  // --mqtthost=localhost
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid mqtthost");
      return EINVAL;
    }
    g_host = arg;
    break;

  case O_PORT:  // --mqttport=1883
    g_port = (uint16_t)parseInt(arg, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid mqttport");
      return EINVAL;
    }
    break;

  case O_CLID:  // --mqttclientid=clientid
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid mqttclientid");
      return EINVAL;
    }
    g_clientId = arg;
    break;

  case O_USER:  // --mqttuser=username
    if (arg == nullptr) {
      argp_error(state, "invalid mqttuser");
      return EINVAL;
    }
    g_username = arg;
    break;

  case O_PASS:  // --mqttpass=password
    if (arg == nullptr) {
      argp_error(state, "invalid mqttpass");
      return EINVAL;
    }
    g_password = replaceSecret(arg);
    break;

  case O_TOPI:  // --mqtttopic=ebusd
    if (arg == nullptr || arg[0] == 0 || strchr(arg, '+') || arg[strlen(arg)-1] == '/') {
      argp_error(state, "invalid mqtttopic");
      return EINVAL;
    } else {
      char *pos = strchr(arg, '#');
      if (pos && (pos == arg || pos[1])) {  // allow # only at very last position (to indicate not using any default)
        argp_error(state, "invalid mqtttopic");
        return EINVAL;
      }
    }
    if (g_topic) {
      argp_error(state, "duplicate mqtttopic");
      return EINVAL;
    } else {
      MqttReplacer replacer;
      if (!replacer.parse(arg, true)) {
        argp_error(state, "malformed mqtttopic");
        return EINVAL;
      }
      g_topic = arg;
    }
    break;

  case O_GTOP:  // --mqttglobal=global/
    if (arg == nullptr || strchr(arg, '+') || strchr(arg, '#')) {
      argp_error(state, "invalid mqttglobal");
      return EINVAL;
    }
    g_globalTopic = arg;
    break;

  case O_RETA:  // --mqttretain
    g_retain = true;
    break;

  case O_PQOS:  // --mqttqos=0
    g_qos = parseSignedInt(arg, 10, 0, 2, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid mqttqos value");
      return EINVAL;
    }
    break;

  case O_INTF:  // --mqttint=/etc/ebusd/mqttint.cfg
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid mqttint file");
      return EINVAL;
    }
    g_integrationFile = arg;
    break;

  case O_IVAR:  // --mqttvar=NAME=VALUE[,NAME=VALUE]*
    if (arg == nullptr || arg[0] == 0 || !strchr(arg, '=')) {
      argp_error(state, "invalid mqttvar");
      return EINVAL;
    }
    g_integrationVars = arg;
    break;

  case O_JSON:  // --mqttjson
    g_publishFormat |= OF_JSON|OF_NAMES;
    break;

  case O_VERB:  // --mqttverbose
    g_publishFormat |= OF_NAMES|OF_UNITS|OF_COMMENTS|OF_ALL_ATTRS;
    break;

#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
  case O_LOGL:
    g_logFromLib = true;
    break;
#endif

#if (LIBMOSQUITTO_VERSION_NUMBER >= 1004001)
  case O_VERS:  // --mqttversion=3.1.1
    if (arg == nullptr || arg[0] == 0 || (strcmp(arg, "3.1") != 0 && strcmp(arg, "3.1.1") != 0)) {
      argp_error(state, "invalid mqttversion");
      return EINVAL;
    }
    g_version = strcmp(arg, "3.1.1") == 0 ? MQTT_PROTOCOL_V311 : MQTT_PROTOCOL_V31;
    break;
#endif

  case O_IGIN:
    g_ignoreInvalidParams = true;
    break;

  case O_CHGS:
    g_onlyChanges = true;
    break;

#if (LIBMOSQUITTO_MAJOR >= 1)
    case O_CAFI:  // --mqttca=file or --mqttca=dir/
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid mqttca");
        return EINVAL;
      }
      if (arg[strlen(arg)-1] == '/') {
        g_cafile = nullptr;
        g_capath = arg;
      } else {
        g_cafile = arg;
        g_capath = nullptr;
      }
      break;

    case O_CERT:  // --mqttcert=CERTFILE
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid mqttcert");
        return EINVAL;
      }
      g_certfile = arg;
      break;

    case O_KEYF:  // --mqttkey=KEYFILE
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid mqttkey");
        return EINVAL;
      }
      g_keyfile = arg;
      break;

    case O_KEPA:  // --mqttkeypass=PASSWORD
      if (arg == nullptr) {
        argp_error(state, "invalid mqttkeypass");
        return EINVAL;
      }
      g_keypass = replaceSecret(arg);
      break;
    case O_INSE:  // --mqttinsecure
      g_insecure = true;
      break;
#endif

  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static const struct argp g_mqtt_argp = { g_mqtt_argp_options, mqtt_parse_opt, nullptr, nullptr, nullptr, nullptr,
  nullptr };
static const struct argp_child g_mqtt_argp_child = {&g_mqtt_argp, 0, "", 1};


const struct argp_child* mqtthandler_getargs() {
  return &g_mqtt_argp_child;
}

bool check(int code, const char* method) {
  if (code == MOSQ_ERR_SUCCESS) {
    return true;
  }
  if (code == MOSQ_ERR_ERRNO) {
    char* error = strerror(errno);
    logOtherError("mqtt", "%s: errno %d=%s", method, errno, error);
    return false;
  }
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
  const char* msg = mosquitto_strerror(code);
  logOtherError("mqtt", "%s: %s", method, msg);
#else
  logOtherError("mqtt", "%s: error code %d", method, code);
#endif
  return false;
}

bool mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers) {
  if (g_port > 0) {
    int major = -1;
    int minor = -1;
    int revision = -1;
    mosquitto_lib_version(&major, &minor, &revision);
    if (major < LIBMOSQUITTO_MAJOR) {
      logOtherError("mqtt", "invalid mosquitto version %d instead of %d", major, LIBMOSQUITTO_MAJOR);
      return false;
    }
    logOtherInfo("mqtt", "mosquitto version %d.%d.%d (compiled with %d.%d.%d)", major, minor, revision,
      LIBMOSQUITTO_MAJOR, LIBMOSQUITTO_MINOR, LIBMOSQUITTO_REVISION);
    handlers->push_back(new MqttHandler(userInfo, busHandler, messages));
  }
  return true;
}

/** the known topic field names. */
static const char* knownFieldNames[] = {
  "circuit",
  "name",
  "field",
};

/** the number of known field names. */
static const size_t knownFieldCount = sizeof(knownFieldNames) / sizeof(char*);

std::pair<string, int> makeField(const string& name, bool isField) {
  if (!isField) {
    return {name, -1};
  }
  for (int idx = 0; idx < (int)knownFieldCount; idx++) {
    if (name==knownFieldNames[idx]) {
      return {name, idx};
    }
  }
  return {name, knownFieldCount};
}

void addPart(ostringstream& stack, int inField, vector<std::pair<string, int>>& parts) {
  string str = stack.str();
  if (inField == 1 && str == "_") {
    inField = 0; // single "%_" pattern to reduce to "_"
  } else if (inField == 2) {
    str = "%{" + str;
    inField = 0;
  }
  if (inField == 0 && str.empty()) {
    return;
  }
  stack.str("");
  if (inField == 0 && !parts.empty() && parts[parts.size()-1].second < 0) {
    // append constant to previous constant
    parts[parts.size()-1].first += str;
    return;
  }
  parts.push_back(makeField(str, inField>0));
}

bool MqttReplacer::parse(const string& templateStr, bool onlyKnown, bool noKnownDuplicates, bool emptyIfMissing) {
  m_parts.clear();
  int inField = 0; // 1 after '%', 2 after '%{'
  ostringstream stack;
  for (auto ch : templateStr) {
    bool empty = stack.tellp()<=0;
    if (ch=='%') {
      if (inField==1 && empty) {  // %% for plain %
        inField = 0;
        stack << ch;
      } else {
        addPart(stack, inField, m_parts);
        inField = 1;
      }
    } else if (ch=='{' && inField==1 && empty) {
      inField = 2;
    } else if (ch=='}' && inField==2) {
      addPart(stack, 1, m_parts);
      inField = 0;
    } else {
      if (inField>0 && !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')) {
        // invalid field character
        addPart(stack, inField, m_parts);
        inField = 0;
      }
      stack << ch;
    }
  }
  addPart(stack, inField, m_parts);
  if (onlyKnown || noKnownDuplicates) {
    int foundMask = 0;
    int knownCount = knownFieldCount;
    for (const auto &it : m_parts) {
      if (it.second < 0) {
        continue;  // unknown field
      }
      if (onlyKnown && it.second >= knownCount) {
        return false;
      }
      if (noKnownDuplicates && it.second < knownCount) {
        int bit = 1<<it.second;
        if (foundMask & bit) {
          return false;  // duplicate known field
        }
        foundMask |= bit;
      }
    }
  }
  m_emptyIfMissing = emptyIfMissing;
  return true;
}

void MqttReplacer::normalize(string& str) {
  transform(str.begin(), str.end(), str.begin(), [](unsigned char c){
    return isalnum(c) ? c : '_';
  });
}

const string MqttReplacer::str() const {
  ostringstream ret;
  for (const auto &it: m_parts) {
    if (it.second >= 0) {
      ret << '%';
    }
    ret << it.first;
  }
  return ret.str();
}

void MqttReplacer::ensureDefault() {
  if (m_parts.empty()) {
    m_parts.emplace_back(string(PACKAGE) + "/", -1);
  } else if (m_parts.size()==1 && m_parts[0].second<0 && m_parts[0].first.find('/')==string::npos) {
    m_parts[0] = {m_parts[0].first + "/", -1}; // ensure trailing slash
  }
  if (!has("circuit")) {
    m_parts.emplace_back("circuit", 0);  // index of circuit in knownFieldNames
    m_parts.emplace_back("/", -1);
  }
  if (!has("name")) {
    m_parts.emplace_back("name", 1);  // index of name in knownFieldNames
  }
}

bool MqttReplacer::empty() const {
  return m_parts.empty();
}

bool MqttReplacer::has(const string& field) const {
  for (const auto &it: m_parts) {
    if (it.second >= 0 && it.first == field) {
      return true;
    }
  }
  return false;
}

string MqttReplacer::get(const map<string, string>& values, bool untilFirstEmpty, bool onlyAlphanum) const {
  ostringstream ret;
  for (const auto &it: m_parts) {
    if (it.second < 0) {
      ret << it.first;
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos==values.cend()) {
      if (untilFirstEmpty) {
        break;
      }
      if (m_emptyIfMissing) {
        return "";
      }
    } else if (pos->second.empty()) {
      if (untilFirstEmpty) {
        break;
      }
      if (m_emptyIfMissing) {
        return "";
      }
    } else {
      ret << pos->second;
    }
  }
  if (!onlyAlphanum) {
    return ret.str();
  }
  string str = ret.str();
  normalize(str);
  return str;
}

string MqttReplacer::get(const string& circuit, const string& name, const string& fieldName) const {
  map <string, string> values;
  values["circuit"] = circuit;
  values["name"] = name;
  if (!fieldName.empty()) {
    values["field"] = fieldName;
  }
  return get(values, true);
}

string MqttReplacer::get(const Message* message, const string& fieldName) const {
  map<string, string> values;
  values["circuit"] = message->getCircuit();
  values["name"] = message->getName();
  if (!fieldName.empty()) {
    values["field"] = fieldName;
  }
  return get(message->getCircuit(), message->getName(), fieldName);
}

bool MqttReplacer::isReducable(const map<string, string>& values) const {
  for (const auto &it: m_parts) {
    if (it.second < 0) {
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos==values.cend()) {
      return false;
    }
  }
  return true;
}

void MqttReplacer::compress(const map<string, string>& values) {
  bool lastConstant = false;
  for (auto it = m_parts.begin(); it != m_parts.end(); ) {
    bool isConstant = it->second < 0;
    if (!isConstant) {
      const auto pos = values.find(it->first);
      if (pos!=values.cend()) {
        it->second = -1;
        it->first = pos->second;
        isConstant = true;
      }
    }
    if (!lastConstant || !isConstant) {
      lastConstant = isConstant;
      ++it;
      continue;
    }
    (it-1)->first += it->first;
    it = m_parts.erase(it);
  }
}

bool MqttReplacer::reduce(const map<string, string>& values, string& result, bool onlyAlphanum) const {
  ostringstream ret;
  for (const auto &it: m_parts) {
    if (it.second < 0) {
      ret << it.first;
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos==values.cend()) {
      if (m_emptyIfMissing) {
        result = "";
      } else {
        result = ret.str();
      }
      return false;
    }
    if (m_emptyIfMissing && pos->second.empty()) {
      result = "";
      return true;
    }
    ret << pos->second;
  }
  result = ret.str();
  if (onlyAlphanum) {
    normalize(result);
  }
  return true;
}

bool MqttReplacer::checkMatch() const {
  bool lastField = false;
  for (const auto& part : m_parts) {
    bool field = part.second >= 0;
    if (field && lastField) {
      return false;
    }
    lastField = field;
  }
  return true;
}

ssize_t MqttReplacer::matchTopic(const string& topic, string* circuit, string* name, string* field) const {
  size_t last = 0;
  size_t count = m_parts.size();
  size_t idx;
  for (idx = 0; idx < count; idx++) {
    const auto part = m_parts[idx];
    if (part.second < 0) {
      if (topic.substr(last, part.first.length()) != part.first) {
        return static_cast<ssize_t>(idx);
      }
      last += part.first.length();
      continue;
    }
    string value;
    if (idx+1 < count) {
      size_t pos = topic.find(m_parts[idx+1].first, last);
      if (pos == string::npos) {
        // next part not found
        return -static_cast<ssize_t>(idx)-1;
      }
      value = topic.substr(last, pos-last);
    } else {
      // last part is a field name
      if (topic.find('/', last) != string::npos) {
        // non-name in remainder found
        return -static_cast<ssize_t>(idx)-1;
      }
      value = topic.substr(last);
    }
    last += value.length();
    switch (part.second) {
      case 0: *circuit = value; break;
      case 1: *name = value; break;
      case 2: *field = value; break;
      default: // unknown field
        break;
    }
  }
  return static_cast<ssize_t>(idx);
}

static const string EMPTY = "";

const string& MqttReplacers::operator[](const string& key) const {
  auto itc = m_constants.find(key);
  if (itc==m_constants.end()) {
    return EMPTY;
  }
  return itc->second;
}

bool MqttReplacers::uses(const string& field) const {
  for (const auto &it : m_replacers) {
    if (it.second.has(field)) {
      return true;
    }
  }
  return false;
}

MqttReplacer& MqttReplacers::get(const string& key) {
  MqttReplacer& ret = m_replacers[key];
  auto it = m_constants.find(key);
  if (it != m_constants.end()) {
    // constant with the same name found
    if (ret.empty()) {
      ret.parse(it->second);  // convert to replacer
    }
    m_constants.erase(it);
  }
  return ret;
}

MqttReplacer MqttReplacers::get(const string& key) const {
  const auto& it = m_replacers.find(key);
  if (it != m_replacers.cend()) {
    return it->second;
  }
  return MqttReplacer();
}

string MqttReplacers::get(const string& key, bool untilFirstEmpty, bool onlyAlphanum, const string& fallbackKey) const {
  auto itc = m_constants.find(key);
  if (itc!=m_constants.end()) {
    return itc->second;
  }
  auto itv = m_replacers.find(key);
  if (itv!=m_replacers.end()) {
    return itv->second.get(m_constants, untilFirstEmpty, onlyAlphanum);
  }
  if (!fallbackKey.empty()) {
    itc = m_constants.find(fallbackKey);
    if (itc!=m_constants.end()) {
      return itc->second;
    }
    itv = m_replacers.find(fallbackKey);
    if (itv!=m_replacers.end()) {
      return itv->second.get(m_constants, untilFirstEmpty, onlyAlphanum);
    }
  }
  return "";
}

bool MqttReplacers::set(const string& key, const string& value, bool removeReplacer) {
  m_constants[key] = value;
  if (removeReplacer) {
    m_replacers.erase(key);
  }
  if (key.find_first_of("-_")!=string::npos) {
    return false;
  }
  string upper = key;
  transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  if (upper==key) {
    return false;
  }
  string val = value;
  MqttReplacer::normalize(val);
  m_constants[upper] = val;
  if (removeReplacer) {
    m_replacers.erase(upper);
  }
  return true;
}

void MqttReplacers::set(const string& key, int value) {
  std::ostringstream str;
  str << static_cast<signed>(value);
  m_constants[key] = str.str();
}

void MqttReplacers::reduce(bool compress) {
  // iterate through variables and reduce as many to constants as possible
  bool reduced = false;
  do {
    reduced = false;
    for (auto it = m_replacers.begin(); it != m_replacers.end(); ) {
      string str;
      if (!it->second.isReducable(m_constants)
      || !it->second.reduce(m_constants, str)) {
        if (compress) {
          it->second.compress(m_constants);
        }
        ++it;
        continue;
      }
      bool restart = set(it->first, str, false);
      it = m_replacers.erase(it);
      reduced = true;
      if (restart) {
        string upper = it->first;
        transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (m_replacers.erase(upper)>0) {
          break;  // restart as iterator is now invalid
        }
      }
    }
  } while (reduced);
}


#if (LIBMOSQUITTO_MAJOR >= 1)
int on_keypassword(char *buf, int size, int rwflag, void *userdata) {
  if (!g_keypass) {
    return 0;
  }
  int len = static_cast<int>(strlen(g_keypass));
  if (len > size) {
    len = size;
  }
  memcpy(buf, g_keypass, len);
  return len;
}
#endif

void on_connect(
#if (LIBMOSQUITTO_MAJOR >= 1)
  struct mosquitto *mosq,
#endif
  void *obj, int rc) {
  if (rc == 0) {
    logOtherNotice("mqtt", "connection established");
    MqttHandler* handler = reinterpret_cast<MqttHandler*>(obj);
    if (handler) {
      handler->notifyConnected();
    }
  } else {
    if (rc >= 1 && rc <= 3) {
      logOtherError("mqtt", "connection refused: %s",
                    rc == 1 ? "wrong protocol" : (rc == 2 ? "wrong username/password" : "broker down"));
    } else {
      logOtherError("mqtt", "connection refused: %d", rc);
    }
  }
}

#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
void on_log(struct mosquitto *mosq, void *obj, int level, const char* msg) {
  switch (level) {
  case MOSQ_LOG_DEBUG:
    logOtherDebug("mqtt", "log %s", msg);
    break;
  case MOSQ_LOG_INFO:
    logOtherInfo("mqtt", "log %s", msg);
    break;
  case MOSQ_LOG_NOTICE:
    logOtherNotice("mqtt", "log %s", msg);
    break;
  case MOSQ_LOG_WARNING:
    logOtherNotice("mqtt", "log warning %s", msg);
    break;
  case MOSQ_LOG_ERR:
    logOtherError("mqtt", "log %s", msg);
    break;
  default:
    logOtherError("mqtt", "log other %s", msg);
    break;
  }
}
#endif

void on_message(
#if (LIBMOSQUITTO_MAJOR >= 1)
  struct mosquitto *mosq,
#endif
  void *obj, const struct mosquitto_message *message) {
  MqttHandler* handler = reinterpret_cast<MqttHandler*>(obj);
  if (!handler || !message || !handler->isRunning()) {
    return;
  }
  string topic(message->topic);
  string data(message->payloadlen > 0 ? reinterpret_cast<char*>(message->payload) : "");
  handler->notifyTopic(topic, data);
}

void MqttHandler::parseIntegration(const string& line) {
  if (line.empty()) {
    return;
  }
  size_t pos = line.find('=');
  if (pos==string::npos || pos==0) {
    return;
  }
  bool emptyIfMissing = false;
  string key;
  if (line[pos-1] == '?') {
    emptyIfMissing = true;
    key = line.substr(0, pos-1);
  } else {
    key = line.substr(0, pos);
  }
  FileReader::trim(&key);
  string value = line.substr(pos+1);
  FileReader::trim(&value);
  if (value.find('%')==string::npos) {
    m_replacers.set(key, value); // constant value
  } else {
    // simple variable
    m_replacers.get(key).parse(value, false, false, emptyIfMissing);
  }
}

/**
 * possible data type names.
 */
static const char* typeNames[] = {
    "number", "list", "string", "date", "time", "datetime",
};

/**
 * possible message direction names by (write*2) + (passive*1).
 */
static const char* directionNames[] = {
    "r", "u", "w", "uw",
};

string removeTrailingNonTopicPart(const string& str) {
  size_t pos = str.find_last_not_of("/_");
  if (pos==string::npos) {
    return str;
  }
  return str.substr(0, pos + 1);
}

void splitFields(const string& str, vector<string>* row);

MqttHandler::MqttHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages)
  : DataSink(userInfo, "mqtt"), DataSource(busHandler), WaitThread(), m_messages(messages), m_connected(false),
    m_initialConnectFailed(false), m_lastUpdateCheckResult("."), m_lastScanStatus("."), m_lastErrorLogTime(0) {
  m_definitionsSince = 0;
  m_mosquitto = nullptr;
  bool hasIntegration = false;
  if (g_integrationFile != nullptr) {
    std::ifstream stream;
    stream.open(g_integrationFile, std::ifstream::in);
    if (!stream.is_open()) {
      logOtherError("mqtt", "unable to open integration file %s", g_integrationFile);
    } else {
      string line, last;
      while (stream.peek() != EOF && getline(stream, line)) {
        if (line.empty()) {
          parseIntegration(last);
          last = "";
          continue;
        }
        if (line[0] == '#') {
          // only ignore it to allow commented lines in the middle of e.g. payload
          continue;
        }
        if (last.empty()) {
          last = line;
        } else if (line[0] == '\t' || line[0] == ' ') { // continuation
          last += "\n" + line;
        } else {
          parseIntegration(last);
          last = line;
        }
      }
      stream.close();
      parseIntegration(last);
      hasIntegration = true;
      if (g_integrationVars) {
        vector<string> strs;
        splitFields(g_integrationVars, &strs);
        for (auto& str : strs) {
          size_t pos = str.find('=');
          if (pos==string::npos || pos==0) {
            continue;
          }
          m_replacers.set(str.substr(0, pos), str.substr(pos+1));
        }
      }
    }
  }
  // determine topic and prefix
  MqttReplacer& topic = m_replacers.get("topic");
  if (g_topic) {
    string str = g_topic;
    bool noDefault = str[str.size()-1]=='#';
    if (noDefault) {
      str.resize(str.size()-1);
    }
    bool parse = true;
    if (hasIntegration && !topic.empty()) {
      // topic defined in cmdline and integration file.
      if (str.find('%')==string::npos) {
        // cmdline topic is only the prefix, use it
        m_replacers.set("prefix", str);
        m_replacers.set("prefixn", removeTrailingNonTopicPart(str));
        parse = false;
      } // else: cmdline topic is more than just a prefix => override integration topic completely
    }
    if (parse) {
      if (!topic.parse(str, true, true)) {
        logOtherNotice("mqtt", "unknown or duplicate topic parts potentially prevent matching incoming topics");
        topic.parse(str, true);
      } else if (!topic.checkMatch()) {
        logOtherNotice("mqtt", "missing separators between topic parts potentially prevent matching incoming topics");
      }
    }
    if (!noDefault) {
      topic.ensureDefault();
    }
  } else {
    topic.ensureDefault();
  }
  m_staticTopic = !topic.has("name");
  m_publishByField = !m_staticTopic && topic.has("field");
  if (hasIntegration) {
    m_replacers.set("version", PACKAGE_VERSION);
    if (m_replacers["prefix"].empty()) {
      string line = m_replacers.get("topic", true);
      m_replacers.set("prefix", line);
      m_replacers.set("prefixn", removeTrailingNonTopicPart(line));
    }
    m_replacers.reduce(true);
    if (!m_replacers["type_switch-names"].empty() || m_replacers.uses("type_switch")) {
      ostringstream ostr;
      for (int i=-1; i<static_cast<signed>(sizeof(directionNames)/sizeof(char*)); i++) {
        for (auto typeName : typeNames) {
          ostr.str("");
          if (i>=0) {
            ostr << directionNames[i] << '-';
          }
          ostr << typeName;
          const string suffix = ostr.str();
          string str = m_replacers.get("type_switch-" + suffix, false, false, "type_switch");
          if (str.empty()) {
            continue;
          }
          str += '\n'; // add trailing newline to ease the split
          size_t from = 0;
          do {
            size_t pos = str.find('\n', from);
            string line = str.substr(from, pos - from);
            from = pos + 1;
            FileReader::trim(&line);
            if (!line.empty()) {
              pos = line.find('=');
              if (pos != string::npos && pos > 0) {
                string left = line.substr(0, pos);
                FileReader::trim(&left);
                string right = line.substr(pos + 1);
                FileReader::trim(&right);
                FileReader::tolower(&right);
                m_typeSwitches[suffix].push_back({left, right});
              }
            }
          } while (from < str.length());
        }
      }
    }
  }
  m_hasDefinitionTopic = !m_replacers.get("definition-topic", true, false).empty();
  m_hasDefinitionFieldsPayload = m_replacers.uses("fields_payload");
  m_subscribeConfigRestartTopic = m_replacers.get("config_restart-topic", false, false);
  m_subscribeConfigRestartPayload = m_replacers.get("config_restart-payload", false, false);
  if (g_globalTopic) {
    m_globalTopic.parse(g_globalTopic);
  } else {
    m_globalTopic.parse(getTopic(nullptr, "%circuit/%name"));
  }
  if (m_globalTopic.has("circuit")) {
    map<string, string> values;
    values["circuit"] = "global";
    m_globalTopic.compress(values);
  }
  m_subscribeTopic = getTopic(nullptr, "#");
  if (check(mosquitto_lib_init(), "unable to initialize")) {
    signal(SIGPIPE, SIG_IGN);  // needed before libmosquitto v. 1.1.3
    ostringstream clientId;
    if (g_clientId) {
      clientId << g_clientId;
    } else {
      clientId << PACKAGE_NAME << '_' << PACKAGE_VERSION << '_' << static_cast<unsigned>(getpid());
    }
#if (LIBMOSQUITTO_MAJOR >= 1)
    m_mosquitto = mosquitto_new(clientId.str().c_str(), true, this);
#else
    m_mosquitto = mosquitto_new(clientId.str().c_str(), this);
#endif
    if (!m_mosquitto) {
      logOtherError("mqtt", "unable to instantiate");
    }
  }
  if (m_mosquitto) {
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1004001)
    check(mosquitto_threaded_set(m_mosquitto, true), "threaded_set");
    check(mosquitto_opts_set(m_mosquitto, MOSQ_OPT_PROTOCOL_VERSION, reinterpret_cast<void*>(&g_version)),
       "opts_set protocol version");
#endif
    if (g_username || g_password) {
      if (!g_username) {
        g_username = PACKAGE;
      }
      if (mosquitto_username_pw_set(m_mosquitto, g_username, g_password) != MOSQ_ERR_SUCCESS) {
        logOtherError("mqtt", "unable to set username/password, trying without");
      }
    }
    string willTopic = m_globalTopic.get("", "running");
    string willData = "false";
    size_t len = willData.length();
#if (LIBMOSQUITTO_MAJOR >= 1)
    mosquitto_will_set(m_mosquitto, willTopic.c_str(), (uint32_t)len,
        reinterpret_cast<const uint8_t*>(willData.c_str()), 0, true);
#else
    mosquitto_will_set(m_mosquitto, true, willTopic.c_str(), (uint32_t)len,
        reinterpret_cast<const uint8_t*>(willData.c_str()), 0, true);
#endif

#if (LIBMOSQUITTO_MAJOR >= 1)
    if (g_cafile || g_capath) {
      int ret;
      ret = mosquitto_tls_set(m_mosquitto, g_cafile, g_capath, g_certfile, g_keyfile, on_keypassword);
      if (ret != MOSQ_ERR_SUCCESS) {
        logOtherError("mqtt", "unable to set TLS: %d", ret);
      } else if (g_insecure) {
        ret = mosquitto_tls_insecure_set(m_mosquitto, true);
        if (ret != MOSQ_ERR_SUCCESS) {
          logOtherError("mqtt", "unable to set TLS insecure: %d", ret);
        }
      }
    }
#endif
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
    if (g_logFromLib) {
      mosquitto_log_callback_set(m_mosquitto, on_log);
    }
#endif
    mosquitto_connect_callback_set(m_mosquitto, on_connect);
    mosquitto_message_callback_set(m_mosquitto, on_message);
    int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
    ret = mosquitto_connect(m_mosquitto, g_host, g_port, 60);
#else
    ret = mosquitto_connect(m_mosquitto, g_host, g_port, 60, true);
#endif
    if (ret == MOSQ_ERR_INVAL && !g_ignoreInvalidParams) {
      logOtherError("mqtt", "unable to connect (invalid parameters)");
      mosquitto_destroy(m_mosquitto);
      m_mosquitto = nullptr;
    } else if (!check(ret, "unable to connect, retrying")) {
      m_connected = false;
      m_initialConnectFailed = g_ignoreInvalidParams;
    } else {
      m_connected = true;  // assume success until connect_callback says otherwise
      logOtherDebug("mqtt", "connection requested");
    }
  }
}

MqttHandler::~MqttHandler() {
  join();
  if (m_mosquitto) {
    mosquitto_destroy(m_mosquitto);
    m_mosquitto = nullptr;
  }
  mosquitto_lib_cleanup();
}

void MqttHandler::start() {
  if (m_mosquitto) {
    WaitThread::start("MQTT");
  }
}

void MqttHandler::notifyConnected() {
  if (m_mosquitto && isRunning()) {
    const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
    if (m_globalTopic.has("name")) {
      publishTopic(m_globalTopic.get("", "version"), sep + (PACKAGE_STRING "." REVISION) + sep, true);
    }
    publishTopic(m_globalTopic.get("", "running"), "true", true);
    if (!m_staticTopic) {
      check(mosquitto_subscribe(m_mosquitto, nullptr, m_subscribeTopic.c_str(), 0), "subscribe");
      if (!m_subscribeConfigRestartTopic.empty()) {
        check(mosquitto_subscribe(m_mosquitto, nullptr, m_subscribeConfigRestartTopic.c_str(), 0), "subscribe definition");
      }
    }
  }
}

void MqttHandler::notifyTopic(const string& topic, const string& data) {
  size_t pos = topic.rfind('/');
  if (pos == string::npos) {
    return;
  }
  if (!m_subscribeConfigRestartTopic.empty() && topic == m_subscribeConfigRestartTopic) {
    if (m_subscribeConfigRestartPayload.empty() || data == m_subscribeConfigRestartPayload) {
      m_definitionsSince = 0;
    }
    return;
  }
  string direction = topic.substr(pos+1);
  if (direction.empty()) {
    return;
  }
  pos = direction.find('?');
  string args;
  if (pos != string::npos) {
    // support e.g. ?1 in topic for poll prio
    args = direction.substr(pos+1);
    direction = direction.substr(0, pos);
  }
  bool isWrite = direction == "set";
  bool isList = !isWrite && direction == "list";
  if (!isWrite && !isList && direction != "get") {
    return;
  }

  logOtherDebug("mqtt", "received topic %s with data %s", topic.c_str(), data.c_str());
  string circuit, name, field;
  ssize_t match = m_replacers.get("topic").matchTopic(topic.substr(0, pos), &circuit, &name, &field);
  if (match<0 && !isList) {
    logOtherError("mqtt", "received unmatchable topic %s", topic.c_str());
  }
  if (isList) {
    logOtherInfo("mqtt", "received list topic for %s %s", circuit.c_str(), name.c_str());
    deque<Message*> messages;
    bool circuitPrefix = circuit.length() > 0 && circuit.find_last_of('*') == circuit.length()-1;
    if (circuitPrefix) {
      circuit = circuit.substr(0, circuit.length()-1);
    }
    bool namePrefix = name.length() > 0 && name.find_last_of('*') == name.length()-1;
    if (namePrefix) {
      name = name.substr(0, name.length()-1);
    }
    m_messages->findAll(circuit, name, m_levels, !(circuitPrefix || namePrefix), true, true,
                        true, true, true, 0, 0, false, &messages);
    bool onlyWithData = !data.empty();
    for (const auto& message : messages) {
      if ((circuitPrefix && (
          message->getCircuit().substr(0, circuit.length()) != circuit
          || (!namePrefix && name.length() > 0 && message->getName() != name)))
      || (namePrefix && (
          message->getName().substr(0, name.length()) != name
          || (!circuitPrefix && circuit.length() > 0 && message->getCircuit() != circuit)))
      ) {
        continue;
      }
      time_t lastup = message->getLastUpdateTime();
      if (onlyWithData && lastup == 0) {
        continue;
      }
      ostringstream ostream;
      publishMessage(message, &ostream, true);
    }
    return;
  }
  if (name.empty()) {
    return;
  }
  logOtherInfo("mqtt", "received %s topic for %s %s", direction.c_str(), circuit.c_str(), name.c_str());
  Message* message = m_messages->find(circuit, name, m_levels, isWrite);
  if (message == nullptr) {
    message = m_messages->find(circuit, name, m_levels, isWrite, true);
  }
  if (message == nullptr) {
    logOtherError("mqtt", "%s message %s %s not found", isWrite?"write":"read", circuit.c_str(), name.c_str());
    return;
  }
  if (!message->isPassive()) {
    string useData = data;
    if (!isWrite && !data.empty()) {
      pos = useData.find_last_of('?');
      if (pos != string::npos && pos > 0 && useData[pos-1] != UI_FIELD_SEPARATOR) {
        pos = string::npos;
      }
      if (pos != string::npos) {
        if (args.empty()) {
          args = useData.substr(pos + 1);
        }
        useData = useData.substr(0, pos > 0 ? pos - 1 : pos);
      }
    }
    if (!args.empty()) {
      result_t ret = RESULT_OK;
      auto pollPriority = (size_t)parseInt(args.c_str(), 10, 1, 9, &ret);
      if (ret == RESULT_OK && pollPriority > 0 && message->setPollPriority(pollPriority)) {
        m_messages->addPollMessage(false, message);
      }
    }
    result_t result = m_busHandler->readFromBus(message, useData);
    if (result != RESULT_OK) {
      logOtherError("mqtt", "%s %s %s: %s", isWrite?"write":"read", circuit.c_str(), name.c_str(),
          getResultCode(result));
      return;
    }
    logOtherNotice("mqtt", "%s %s %s: %s", isWrite?"write":"read", circuit.c_str(), name.c_str(), data.c_str());
  }
  ostringstream ostream;
  publishMessage(message, &ostream);
}

void MqttHandler::notifyUpdateCheckResult(const string& checkResult) {
  if (checkResult != m_lastUpdateCheckResult) {
    m_lastUpdateCheckResult = checkResult;
    if (m_globalTopic.has("name")) {
      const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
      publishTopic(m_globalTopic.get("", "updatecheck"), sep + (checkResult.empty() ? "OK" : checkResult) + sep, true);
    }
  }
}

void MqttHandler::notifyScanStatus(const string& scanStatus) {
  if (scanStatus != m_lastScanStatus) {
    m_lastScanStatus = scanStatus;
    if (m_globalTopic.has("name")) {
      const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
      publishTopic(m_globalTopic.get("", "scan"), sep + (scanStatus.empty() ? "OK" : scanStatus) + sep, true);
    }
  }
}

bool parseBool(const string& str) {
  return !str.empty() && !(str=="0" || str=="no" || str=="false");
}

void splitFields(const string& str, vector<string>* row) {
  std::istringstream istr;
  istr.str(str);
  unsigned int lineNo = 0;
  FileReader::splitFields(&istr, row, &lineNo);
  if (row->size() == 1 && (*row)[0].empty()) {
    row->clear();
  }
}

void MqttHandler::run() {
  time_t lastTaskRun, now, start, lastSignal = 0, lastUpdates = 0;
  bool signal = false;
  bool globalHasName = m_globalTopic.has("name");
  string signalTopic = m_globalTopic.get("", "signal");
  string uptimeTopic = m_globalTopic.get("", "uptime");
  ostringstream updates;
  unsigned int filterPriority = 0;
  unsigned int filterSeen = 0;
  string filterCircuit, filterName, filterLevel, filterField, filterDirection;
  vector<string> typeSwitchNames;
  if (m_hasDefinitionTopic) {
    result_t result = RESULT_OK;
    filterPriority = parseInt(m_replacers["filter-priority"].c_str(), 10, 0, 9, &result);
    if (result != RESULT_OK) {
      filterPriority = 0;
    }
    filterSeen = parseInt(m_replacers["filter-seen"].c_str(), 10, 0, 9, &result);
    if (result != RESULT_OK) {
      filterSeen = 0;
    }
    filterCircuit = m_replacers["filter-circuit"];
    FileReader::tolower(&filterCircuit);
    filterName = m_replacers["filter-name"];
    FileReader::tolower(&filterName);
    filterLevel = m_replacers["filter-level"];
    FileReader::tolower(&filterLevel);
    filterField = m_replacers["filter-field"];
    FileReader::tolower(&filterField);
    filterDirection = m_replacers["filter-direction"];
    FileReader::tolower(&filterDirection);
    if (!m_typeSwitches.empty()) {
      splitFields(m_replacers["type_switch-names"], &typeSwitchNames);
    }
  }
  time(&now);
  start = lastTaskRun = now;
  bool allowReconnect = false;
  while (isRunning()) {
    bool wasConnected = m_connected;
    bool needsWait = handleTraffic(allowReconnect);
    bool reconnected = !wasConnected && m_connected;
    allowReconnect = false;
    time(&now);
    bool sendSignal = reconnected;
    if (now < start) {
      // clock skew
      if (now < lastSignal) {
        lastSignal -= lastTaskRun-now;
      }
      lastTaskRun = now;
    } else if (now > lastTaskRun+15) {
      allowReconnect = true;
      if (m_connected) {
        sendSignal = true;
        time_t uptime = now - start;
        updates.str("");
        updates.clear();
        if (globalHasName) {
          updates << dec << static_cast<unsigned>(uptime);
          publishTopic(uptimeTopic, updates.str());
        }
      }
      if (m_connected && m_definitionsSince == 0) {
        publishDefinition(m_replacers, "def_global_running-", m_globalTopic.get("", "running"), "global", "running", "def_global-");
        if (globalHasName) {
          publishDefinition(m_replacers, "def_global_version-", m_globalTopic.get("", "version"), "global", "version",
                            "def_global-");
          publishDefinition(m_replacers, "def_global_signal-", signalTopic, "global", "signal", "def_global-");
          publishDefinition(m_replacers, "def_global_uptime-", uptimeTopic, "global", "uptime", "def_global-");
          publishDefinition(m_replacers, "def_global_updatecheck-", m_globalTopic.get("", "updatecheck"), "global",
                            "updatecheck", "def_global-");
          publishDefinition(m_replacers, "def_global_scan-", m_globalTopic.get("", "scan"), "global", "scan",
                            "def_global-");
        }
        m_definitionsSince = 1;
      }
      if (m_connected && m_hasDefinitionTopic) {
        ostringstream ostr;
        deque<Message*> messages;
        m_messages->findAll("", "", m_levels, false, true, true, true, true, true, 0, 0, false, &messages);
        for (const auto& message : messages) {
          bool checkPollAdjust = false;
          if (filterSeen > 0) {
            if (message->getLastUpdateTime()==0) {
              if (message->isPassive()) {
                // only wait for data on passive messages
                continue;  // no data ever
              }
              if (!message->isWrite()) {
                // only wait for data on read messages or set their poll prio
                if (filterSeen > 1 && (!message->getPollPriority() || message->getPollPriority() > filterSeen)) {
                  // check for poll prio adjustment after all other filters
                  checkPollAdjust = true;
                } else {
                  continue;  // no poll adjustment
                }
              }
            }
            if (message->getDataHandlerState()==1) {
              // already seen in the past, check for poll prio update
              if (m_definitionsSince>1 && message->getCreateTime() <= m_definitionsSince) {
                continue;
              }
            }
            message->setDataHandlerState(1);
          } else if (message->getCreateTime() <= m_definitionsSince) {  // only newer defined
            continue;
          }
          if (!FileReader::matches(message->getCircuit(), filterCircuit, true, true)
          || !FileReader::matches(message->getName(), filterName, true, true)
          || !FileReader::matches(message->getLevel(), filterLevel, true, true)) {
            continue;
          }
          const string direction = directionNames[(message->isWrite() ? 2 : 0) + (message->isPassive() ? 1 : 0)];
          if (!FileReader::matches(direction, filterDirection, true, true)) {
            continue;
          }
          if ((checkPollAdjust && !message->setPollPriority(filterSeen))
          || (filterPriority>0 && (message->getPollPriority()==0 || message->getPollPriority()>filterPriority))) {
            continue;
          }

          MqttReplacers msgValues = m_replacers;  // need a copy here as the contents are manipulated
          msgValues.set("circuit", message->getCircuit());
          msgValues.set("name", message->getName());
          msgValues.set("priority", static_cast<int>(message->getPollPriority()));
          msgValues.set("level", message->getLevel());
          msgValues.set("direction", direction);
          msgValues.set("messagecomment", message->getAttribute("comment"));
          msgValues.reduce(true);
          string str = msgValues.get("direction_map-"+direction, false, false);
          msgValues.set("direction_map", str);
          msgValues.reduce(true);
          ostringstream fields;
          size_t fieldCount = message->getFieldCount();
          for (size_t index = 0; index < fieldCount; index++) {
            const SingleDataField* field = message->getField(index);
            if (!field || field->isIgnored()) {
              continue;
            }
            string fieldName = message->getFieldName(index);
            if (fieldName.empty() && fieldCount == 1) {
              fieldName = "0";  // might occur for unnamed single field sets
            }
            if (!FileReader::matches(fieldName, filterField, true, true)) {
              continue;
            }
            const DataType* dataType = field->getDataType();
            string typeStr;
            if (dataType->isNumeric()) {
              if (field->isList()) {
                typeStr = "list";
              } else {
                typeStr = "number";
              }
            } else if (dataType->hasFlag(DAT)) {
              auto dt = dynamic_cast<const DateTimeDataType*>(dataType);
              if (dt->hasDate()) {
                typeStr = dt->hasDate() ? "datetime" : "date";
              } else {
                typeStr = "time";
              }
            } else {
              typeStr = "string";
            }
            ostr.str("");
            ostr << "type_map-" << direction << "-" << typeStr;
            str = msgValues.get(ostr.str(), false, false);
            if (str.empty()) {
              ostr.str("");
              ostr << "type_map-" << typeStr;
              str = msgValues.get(ostr.str(), false, false);
            }
            if (str.empty()) {
              continue;
            }
            MqttReplacers values = msgValues;  // need a copy here as the contents are manipulated
            values.set("index", static_cast<signed>(index));
            values.set("field", fieldName);
            values.set("fieldname", field->getName(-1));
            values.set("type", typeStr);
            values.set("type_map", str);
            values.set("basetype", dataType->getId());
            values.set("comment", field->getAttribute("comment"));
            values.set("unit", field->getAttribute("unit"));
            if (dataType->isNumeric()) {
              auto dt = dynamic_cast<const NumberDataType*>(dataType);
              ostr.str("");
              if (dt->getMinMax(false, OF_NONE, &ostr) == RESULT_OK) {
                values.set("min", ostr.str());
                ostr.str("");
              };
              if (dt->getMinMax(true, OF_NONE, &ostr) == RESULT_OK) {
                values.set("max", ostr.str());
              };
            }
            if (!m_typeSwitches.empty()) {
              values.reduce(true);
              str = values.get("type_switch-by", false, false);
              string typeSwitch;
              for (int i=0; i<2; i++) {
                ostr.str("");
                if (i==0) {
                  ostr << direction << '-';
                }
                ostr << typeStr;
                const string key = ostr.str();
                for (auto const &check: m_typeSwitches[key]) {
                  if (FileReader::matches(str, check.second, true, true)) {
                    typeSwitch = check.first;
                    i = 2; // early exit
                    break;
                  }
                }
              }
              values.set("type_switch", typeSwitch);
              if (!typeSwitchNames.empty()) {
                vector<string> strs;
                splitFields(typeSwitch, &strs);
                for (size_t pos = 0; pos<strs.size() && pos<typeSwitchNames.size(); pos++) {
                  values.set(typeSwitchNames[pos], strs[pos]);
                }
              }
            }
            values.reduce(true);
            string typePartSuffix = values["type_part-by"];
            if (typePartSuffix.empty()) {
              typePartSuffix = typeStr;
            }
            str = values.get("type_part-" + typePartSuffix, false, false);
            values.set("type_part", str);
            values.reduce();
            if (m_hasDefinitionFieldsPayload) {
              string value = values["field_payload"];
              if (!value.empty()) {
                if (fields.tellp()>0) {
                  fields << values["field-separator"];
                }
                fields << value;
              }
              continue;
            }
            publishDefinition(values);
          }
          if (fields.tellp()>0) {
            msgValues.set("fields_payload", fields.str());
            publishDefinition(msgValues);
          }
          if (filterSeen && message->getLastUpdateTime()>message->getCreateTime()) {
            // ensure data is published as well
            m_updatedMessages[message->getKey()]++;
          } else if (filterSeen && direction=="w") {
            // publish data for read pendant of write message
            Message* read = m_messages->find(message->getCircuit(), message->getName(), "", true);
            if (read && read->getLastUpdateTime()>0) {
              m_updatedMessages[read->getKey()]++;
            }
          }
        }
        time(&m_definitionsSince);
        needsWait = true;
      }
      time(&lastTaskRun);
    }
    if (sendSignal) {
      if (m_busHandler->hasSignal()) {
        lastSignal = now;
        if (!signal || reconnected) {
          signal = true;
          if (globalHasName) {
            publishTopic(signalTopic, "true", true);
          }
        }
      } else {
        if (signal || reconnected) {
          signal = false;
          if (globalHasName) {
            publishTopic(signalTopic, "false", true);
          }
        }
      }
    }
    if (!m_updatedMessages.empty()) {
      m_messages->lock();
      if (m_connected) {
        for (auto it = m_updatedMessages.begin(); it != m_updatedMessages.end(); ) {
          const vector<Message*>* messages = m_messages->getByKey(it->first);
          if (messages) {
            for (const auto& message : *messages) {
              if (message->getLastChangeTime() > 0 && message->isAvailable()
              && (!g_onlyChanges || message->getLastChangeTime() > lastUpdates)) {
                updates.str("");
                updates.clear();
                updates << dec;
                publishMessage(message, &updates);
              }
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
    if ((!m_connected && !Wait(5)) || (needsWait && !Wait(1))) {
      break;
    }
  }
  if (globalHasName) {
    publishTopic(signalTopic, "false", true);
    publishTopic(m_globalTopic.get("", "scan"), "", true);  // clear retain of scan status
  }
}

void MqttHandler::publishDefinition(MqttReplacers values, const string& prefix, const string& topic,
                                    const string& circuit, const string& name, const string& fallbackPrefix) {
  bool reduce = false;
  if (!topic.empty()) {
    values.set("topic", topic);
    reduce = true;
  }
  if (!circuit.empty()) {
    values.set("circuit", circuit);
    reduce = true;
  }
  if (!name.empty()) {
    values.set("name", name);
    reduce = true;
  }
  if (reduce) {
    values.reduce();
  }
  bool noFallback = fallbackPrefix.empty();
  string defTopic = values.get(prefix+"topic", false, false,
                               noFallback ? "" : fallbackPrefix+"topic");
  if (defTopic.empty()) {
    return;
  }
  string payload = values.get(prefix+"payload", false, false,
                              noFallback ? "" : fallbackPrefix+"payload");
  string retainStr = values.get(prefix+"retain", false, false,
                                noFallback ? "" : fallbackPrefix+"retain");
  bool retain = parseBool(retainStr);
  publishTopic(defTopic, payload, retain);
}

void MqttHandler::publishDefinition(const MqttReplacers& values) {
  string defTopic = values.get("definition-topic", false);
  if (defTopic.empty()) {
    if (needsLog(lf_other, ll_debug)) {
      const string str = values.get("definition-topic").str();
      logOtherDebug("mqtt", "cannot publish incomplete definition topic %s", str.c_str());
    }
    return;
  }
  string payload = values.get("definition-payload", false);
  string retainStr = values.get("definition-retain", false);
  bool retain = parseBool(retainStr);
  publishTopic(defTopic, payload, retain);
}

bool MqttHandler::handleTraffic(bool allowReconnect) {
  if (!m_mosquitto) {
    return false;
  }
  int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
  ret = mosquitto_loop(m_mosquitto, -1, 1);  // waits up to 1 second for network traffic
#else
  ret = mosquitto_loop(m_mosquitto, -1);  // waits up to 1 second for network traffic
#endif
  if (!m_connected && (ret == MOSQ_ERR_NO_CONN || ret == MOSQ_ERR_CONN_LOST) && allowReconnect) {
    if (m_initialConnectFailed) {
#if (LIBMOSQUITTO_MAJOR >= 1)
      ret = mosquitto_connect(m_mosquitto, g_host, g_port, 60);
#else
      ret = mosquitto_connect(m_mosquitto, g_host, g_port, 60, true);
#endif
      if (ret == MOSQ_ERR_INVAL) {
        logOtherError("mqtt", "unable to connect (invalid parameters), retrying");
      }
      if (ret == MOSQ_ERR_SUCCESS) {
        m_initialConnectFailed = false;
      }
    } else {
      ret = mosquitto_reconnect(m_mosquitto);
    }
  }
  if (!m_connected && ret == MOSQ_ERR_SUCCESS) {
    m_connected = true;
    logOtherNotice("mqtt", "connection re-established");
  }
  if (!m_connected || ret == MOSQ_ERR_SUCCESS) {
    return false;
  }
  if (ret == MOSQ_ERR_NO_CONN || ret == MOSQ_ERR_CONN_LOST || ret == MOSQ_ERR_CONN_REFUSED) {
    logOtherError("mqtt", "communication error: %s", ret == MOSQ_ERR_NO_CONN ? "not connected"
                  : (ret == MOSQ_ERR_CONN_LOST ? "connection lost" : "connection refused"));
    m_connected = false;
  } else {
    time_t now;
    time(&now);
    if (now > m_lastErrorLogTime + 10) {  // log at most every 10 seconds
      m_lastErrorLogTime = now;
      check(ret, "communication error");
    }
  }
  return true;
}

string MqttHandler::getTopic(const Message* message, const string& suffix, const string& fieldName) {
  if (!message || m_staticTopic) {
    return m_replacers.get("topic", true) + suffix;
  }
  return m_replacers.get("topic").get(message, fieldName) + suffix;
}

void MqttHandler::publishMessage(const Message* message, ostringstream* updates, bool includeWithoutData) {
  OutputFormat outputFormat = g_publishFormat;
  bool json = outputFormat & OF_JSON;
  bool noData = includeWithoutData && message->getLastUpdateTime() == 0;
  if (!m_publishByField) {
    if (noData) {
      publishEmptyTopic(getTopic(message));  // alternatively: , json ? "null" : "");
      return;
    }
    if (json) {
      *updates << "{";
      if (m_staticTopic) {
        *updates << "\"circuit\":\"" << message->getCircuit() << "\",\"name\":\"" << message->getName() << "\",\"fields\":{";
      }
    } else {
      *updates << message->getCircuit() << UI_FIELD_SEPARATOR << message->getName() << UI_FIELD_SEPARATOR;
    }
    result_t result = message->decodeLastData(false, nullptr, -1, outputFormat, updates);
    if (result != RESULT_OK) {
      logOtherError("mqtt", "decode %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
          getResultCode(result));
      return;
    }
    if (json) {
      if (m_staticTopic) {
        *updates << "}";
      }
      *updates << "}";
    }
    publishTopic(getTopic(message), updates->str());
    return;
  }
  if (json && !(outputFormat & OF_ALL_ATTRS)) {
    outputFormat |= OF_SHORT;
  }
  for (size_t index = 0; index < message->getFieldCount(); index++) {
    string name = message->getFieldName(index);
    if (noData) {
      publishEmptyTopic(getTopic(message, "", name));  // alternatively: , json ? "null" : "");
      continue;
    }
    result_t result = message->decodeLastData(false, nullptr, index, outputFormat, updates);
    if (result != RESULT_OK) {
      logOtherError("mqtt", "decode %s %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
          name.c_str(), getResultCode(result));
      return;
    }
    publishTopic(getTopic(message, "", name), updates->str());
    updates->str("");
    updates->clear();
  }
}

void MqttHandler::publishTopic(const string& topic, const string& data, bool retain) {
  const char* topicStr = topic.c_str();
  const char* dataStr = data.c_str();
  const size_t len = strlen(dataStr);
  logOtherDebug("mqtt", "publish %s %s", topicStr, dataStr);
  check(mosquitto_publish(m_mosquitto, nullptr, topicStr, (uint32_t)len,
      reinterpret_cast<const uint8_t*>(dataStr), g_qos, g_retain || retain), "publish");
}

void MqttHandler::publishEmptyTopic(const string& topic) {
  const char* topicStr = topic.c_str();
  logOtherDebug("mqtt", "publish empty %s", topicStr);
  check(mosquitto_publish(m_mosquitto, nullptr, topicStr, 0, nullptr, 0, g_retain), "publish empty");
}

}  // namespace ebusd

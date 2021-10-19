/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2021 John Baier <ebusd@ebusd.eu>
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

namespace ebusd {

using std::dec;

#define O_HOST 1
#define O_PORT (O_HOST+1)
#define O_CLID (O_PORT+1)
#define O_USER (O_CLID+1)
#define O_PASS (O_USER+1)
#define O_TOPI (O_PASS+1)
#define O_RETA (O_TOPI+1)
#define O_JSON (O_RETA+1)
#define O_LOGL (O_JSON+1)
#define O_VERS (O_LOGL+1)
#define O_IGIN (O_VERS+1)
#define O_CHGS (O_IGIN+1)
#define O_CAFI (O_CHGS+1)
#define O_CERT (O_CAFI+1)
#define O_KEYF (O_CERT+1)
#define O_KEPA (O_KEYF+1)
#define O_INSE (O_KEPA+1)

/** the definition of the MQTT arguments. */
static const struct argp_option g_mqtt_argp_options[] = {
  {nullptr,        0,      nullptr,       0, "MQTT options:", 1 },
  {"mqtthost",     O_HOST, "HOST",        0, "Connect to MQTT broker on HOST [localhost]", 0 },
  {"mqttport",     O_PORT, "PORT",        0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]", 0 },
  {"mqttclientid", O_CLID, "ID",         0, "Set client ID for connection to MQTT broker [" PACKAGE_NAME "_"
   PACKAGE_VERSION "_<pid>]", 0 },
  {"mqttuser",     O_USER, "USER",        0, "Connect as USER to MQTT broker (no default)", 0 },
  {"mqttpass",     O_PASS, "PASSWORD",    0, "Use PASSWORD when connecting to MQTT broker (no default)", 0 },
  {"mqtttopic",    O_TOPI, "TOPIC",       0,
   "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]", 0 },
  {"mqttretain",   O_RETA, nullptr,       0, "Retain all topics instead of only selected global ones", 0 },
  {"mqttjson",     O_JSON, nullptr,       0, "Publish in JSON format instead of strings", 0 },
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
  {"mqttinsecure", O_INSE, nullptr,      0, "Allow insecure TLS connection (e.g. using a self signed certificate)", 0 },
#endif

  {nullptr,        0,      nullptr,       0, nullptr, 0 },
};

static const char* g_host = "localhost";  //!< host name of MQTT broker [localhost]
static uint16_t g_port = 0;               //!< optional port of MQTT broker, 0 to disable [0]
static const char* g_clientId = nullptr;  //!< optional clientid override for MQTT broker
static const char* g_username = nullptr;  //!< optional user name for MQTT broker (no default)
static const char* g_password = nullptr;  //!< optional password for MQTT broker (no default)
/** the MQTT topic string parts. */
static vector<string> g_topicStrs;
/** the MQTT topic field parts. */
static vector<string> g_topicFields;
static bool g_retain = false;             //!< whether to retail all topics
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

bool parseTopic(const string& topic, vector<string>* strs, vector<string>* fields);

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
    if (arg == nullptr || arg[0] == 0 || strchr(arg, '#') || strchr(arg, '+') || arg[strlen(arg)-1] == '/') {
      argp_error(state, "invalid mqtttopic");
      return EINVAL;
    }
    if (!parseTopic(arg, &g_topicStrs, &g_topicFields)) {
      argp_error(state, "malformed mqtttopic");
    }
    break;

  case O_RETA:  // --mqttretain
    g_retain = true;
    break;

  case O_JSON:  // --mqttjson
    g_publishFormat |= OF_JSON|OF_NAMES|OF_UNITS|OF_COMMENTS|OF_ALL_ATTRS;
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
  if (g_topicStrs.empty()) {
    g_topicStrs.push_back(PACKAGE);
  }
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


/**
 * Parse the topic template.
 * @param topic the topic template.
 * @param strs the @a vector to which the string parts shall be added.
 * @param fields the @a vector to which the field parts shall be added.
 * @return true on success, false on malformed topic template.
 */
bool parseTopic(const string& topic, vector<string>* strs, vector<string>* fields) {
  size_t lastpos = 0;
  size_t end = topic.length();
  vector<string> columns;
  strs->clear();
  fields->clear();
  for (size_t pos=topic.find('%', lastpos); pos != string::npos; ) {
    size_t idx = knownFieldCount;
    size_t len = 0;
    for (size_t i = 0; i < knownFieldCount; i++) {
      len = strlen(knownFieldNames[i]);
      if (topic.substr(pos+1, len) == knownFieldNames[i]) {
        idx = i;
        break;
      }
    }
    if (idx == knownFieldCount) {  // TODO could allow custom attributes here
      return false;
    }
    string fieldName = knownFieldNames[idx];
    for (const auto& it : *fields) {
      if (it == fieldName) {
        return false;  // duplicate column
      }
    }
    strs->push_back(topic.substr(lastpos, pos-lastpos));
    fields->push_back(fieldName);
    lastpos = pos+1+len;
    pos = topic.find('%', lastpos);
  }
  if (lastpos < end) {
    strs->push_back(topic.substr(lastpos, end-lastpos));
  }
  return true;
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


MqttHandler::MqttHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages)
  : DataSink(userInfo, "mqtt"), DataSource(busHandler), WaitThread(), m_messages(messages), m_connected(false),
    m_initialConnectFailed(false), m_lastUpdateCheckResult("."), m_lastScanStatus("."), m_lastErrorLogTime(0) {
  m_publishByField = false;
  m_mosquitto = nullptr;
  if (g_topicFields.empty()) {
    if (g_topicStrs.empty()) {
      g_topicStrs.push_back("");
    } else {
      string str = g_topicStrs[0];
      if (str.empty() || str[str.length()-1] != '/') {
        g_topicStrs[0] = str+"/";
      }
    }
    g_topicFields.push_back("circuit");
    g_topicStrs.push_back("/");
    g_topicFields.push_back("name");
  } else {
    for (size_t i = 0; i < g_topicFields.size(); i++) {
      if (g_topicFields[i] == "field") {
        m_publishByField = true;
        break;
      }
    }
  }
  m_globalTopic = getTopic(nullptr, "global/");
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
    string willTopic = m_globalTopic+"running";
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
    publishTopic(m_globalTopic+"version", sep + (PACKAGE_STRING "." REVISION) + sep, true);
    publishTopic(m_globalTopic+"running", "true", true);
    check(mosquitto_subscribe(m_mosquitto, nullptr, m_subscribeTopic.c_str(), 0), "subscribe");
  }
}

void MqttHandler::notifyTopic(const string& topic, const string& data) {
  size_t pos = topic.rfind('/');
  if (pos == string::npos) {
    return;
  }
  string direction = topic.substr(pos+1);
  if (direction.empty()) {
    return;
  }
  bool isWrite = direction == "set";
  bool isList = !isWrite && direction == "list";
  if (!isWrite && !isList && direction != "get") {
    return;
  }

  logOtherDebug("mqtt", "received topic %s with data %s", topic.c_str(), data.c_str());
  string remain = topic.substr(0, pos);
  size_t last = 0;
  string circuit, name;
  bool finalField = false;
  for (size_t idx = 0; idx < g_topicStrs.size()+1 && !finalField; idx++) {
    string field;
    string chk;
    if (idx < g_topicStrs.size()) {
      chk = g_topicStrs[idx];
      pos = remain.find(chk, last);
      if (pos == string::npos) {
        if (!isList) {
          return;
        }
        if (idx == 0 && remain+"/" == chk) {  // check for only first prefix, e.g. "ebusd/"
          break;
        }
        pos = remain.size();
        finalField = true;
      }
    } else if (idx-1 < g_topicFields.size()) {
      pos = remain.size();
    } else if (last < remain.size()) {
      if (!isList) {
        return;
      }
      break;
    } else {
      break;
    }
    field = remain.substr(last, pos-last);
    last = pos+chk.size();
    if (idx == 0) {
      if (pos > 0) {
        return;
      }
    } else {
      if (field.empty()) {
        if (!isList) {
          return;
        }
        continue;
      }
      string fieldName = g_topicFields[idx-1];
      if (fieldName == "circuit") {
        circuit = field;
      } else if (fieldName == "name") {
        name = field;
      } else if (fieldName == "field") {
        // field = field;  // TODO add support for writing a single field
      } else {
        return;
      }
    }
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
    for (const auto message : messages) {
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
      size_t pos = useData.find_last_of('?');
      if (pos != string::npos && pos > 0 && useData[pos-1] != UI_FIELD_SEPARATOR) {
        pos = string::npos;
      }
      if (pos != string::npos) {
        string args = useData.substr(pos + 1);
        useData = useData.substr(0, pos > 0 ? pos - 1 : pos);
        if (!args.empty()) {
          result_t ret = RESULT_OK;
          size_t pollPriority = (size_t)parseInt(args.c_str(), 10, 1, 9, &ret);
          if (ret == RESULT_OK && pollPriority > 0 && message->setPollPriority(pollPriority)) {
            m_messages->addPollMessage(false, message);
          }
        }
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
    const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
    publishTopic(m_globalTopic+"updatecheck", sep + (checkResult.empty() ? "OK" : checkResult) + sep, true);
  }
}

void MqttHandler::notifyScanStatus(const string& scanStatus) {
  if (scanStatus != m_lastScanStatus) {
    m_lastScanStatus = scanStatus;
    const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
    publishTopic(m_globalTopic+"scan", sep + (scanStatus.empty() ? "OK" : scanStatus) + sep, true);
  }
}

void MqttHandler::run() {
  time_t lastTaskRun, now, start, lastSignal = 0, lastUpdates = 0;
  bool signal = false;
  string signalTopic = m_globalTopic+"signal";
  string uptimeTopic = m_globalTopic+"uptime";
  ostringstream updates;

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
      sendSignal = true;
      time_t uptime = now-start;
      updates.str("");
      updates.clear();
      updates << dec << static_cast<unsigned>(uptime);
      publishTopic(uptimeTopic, updates.str());
      time(&lastTaskRun);
    }
    if (sendSignal) {
      if (m_busHandler->hasSignal()) {
        lastSignal = now;
        if (!signal || reconnected) {
          signal = true;
          publishTopic(signalTopic, "true", true);
        }
      } else {
        if (signal || reconnected) {
          signal = false;
          publishTopic(signalTopic, "false", true);
        }
      }
    }
    if (!m_updatedMessages.empty()) {
      m_messages->lock();
      if (m_connected) {
        for (auto it = m_updatedMessages.begin(); it != m_updatedMessages.end(); ) {
          const vector<Message*>* messages = m_messages->getByKey(it->first);
          if (messages) {
            for (auto message : *messages) {
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
  publishTopic(signalTopic, "false", true);
  publishTopic(m_globalTopic+"scan", "", true);  // clear retain of scan status
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
  ostringstream ret;
  for (size_t i = 0; i < g_topicStrs.size(); i++) {
    ret << g_topicStrs[i];
    if (!message) {
      break;
    }
    if (i < g_topicFields.size()) {
      if (g_topicFields[i] == "field") {
        ret << fieldName;
      } else {
        message->dumpField(g_topicFields[i], false, OF_NONE, &ret);
      }
    }
  }
  if (!suffix.empty()) {
    ret << suffix;
  }
  return ret.str();
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
    }
    result_t result = message->decodeLastData(false, nullptr, -1, outputFormat, updates);
    if (result != RESULT_OK) {
      logOtherError("mqtt", "decode %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
          getResultCode(result));
      return;
    }
    message->appendAttributes(outputFormat, updates)
    if (json) {
      *updates << "}";
    }
    publishTopic(getTopic(message), updates->str());
    return;
  }
  if (json) {
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
    message->appendAttributes(outputFormat, updates);
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
      reinterpret_cast<const uint8_t*>(dataStr), 0, g_retain || retain), "publish");
}

void MqttHandler::publishEmptyTopic(const string& topic) {
  const char* topicStr = topic.c_str();
  logOtherDebug("mqtt", "publish empty %s", topicStr);
  check(mosquitto_publish(m_mosquitto, nullptr, topicStr, 0, nullptr, 0, g_retain), "publish empty");
}

}  // namespace ebusd

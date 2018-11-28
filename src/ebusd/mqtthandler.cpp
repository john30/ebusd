/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2018 John Baier <ebusd@ebusd.eu>
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
#include "lib/utils/log.h"

namespace ebusd {

using std::dec;

#define O_HOST 1
#define O_PORT (O_HOST+1)
#define O_USER (O_PORT+1)
#define O_PASS (O_USER+1)
#define O_TOPI (O_PASS+1)
#define O_RETA (O_TOPI+1)
#define O_JSON (O_RETA+1)
#define O_V311 (O_JSON+1)
#define O_IGIN (O_V311+1)
#define O_CAFI (O_IGIN+1)
#define O_CERT (O_CAFI+1)
#define O_KEYF (O_CERT+1)
#define O_KEPA (O_KEYF+1)

/** the definition of the MQTT arguments. */
static const struct argp_option g_mqtt_argp_options[] = {
  {nullptr,       0,      nullptr,       0, "MQTT options:", 1 },
  {"mqtthost",    O_HOST, "HOST",        0, "Connect to MQTT broker on HOST [localhost]", 0 },
  {"mqttport",    O_PORT, "PORT",        0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]", 0 },
  {"mqttuser",    O_USER, "USER",        0, "Connect as USER to MQTT broker (no default)", 0 },
  {"mqttpass",    O_PASS, "PASSWORD",    0, "Use PASSWORD when connecting to MQTT broker (no default)", 0 },
  {"mqtttopic",   O_TOPI, "TOPIC",       0,
   "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]", 0 },
  {"mqttretain",   O_RETA, nullptr,       0, "Retain all topics instead of only selected global ones", 0 },
  {"mqttjson",     O_JSON, nullptr,       0, "Publish in JSON format instead of strings", 0 },
  {"mqttv311",     O_V311, nullptr,       0, "Use MQTT protocol 3.1.1 rather than 3.1", 0 },
  {"mqttignoreinvalid", O_IGIN, nullptr,  0,
   "Ignore invalid parameters during init (e.g. for DNS not resolvable yet)", 0 },

#if (LIBMOSQUITTO_MAJOR >= 1)
  {"mqttca",      O_CAFI, "CA",          0, "Use CA file or dir (ending with '/') for MQTT TLS (no default)", 0 },
  {"mqttcert",    O_CERT, "CERTFILE",    0, "Use CERTFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkey",     O_KEYF, "KEYFILE",     0, "Use KEYFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkeypass", O_KEPA, "PASSWORD",    0, "Use PASSWORD for the encrypted KEYFILE (no default)", 0 },
#endif

  {nullptr,       0,      nullptr,       0, nullptr, 0 },
};

static const char* g_host = "localhost";  //!< host name of MQTT broker [localhost]
static uint16_t g_port = 0;               //!< optional port of MQTT broker, 0 to disable [0]
static const char* g_username = nullptr;  //!< optional user name for MQTT broker (no default)
static const char* g_password = nullptr;  //!< optional password for MQTT broker (no default)
/** the MQTT topic string parts. */
static vector<string> g_topicStrs;
/** the MQTT topic field parts. */
static vector<string> g_topicFields;
static bool g_retain = false;             //!< whether to retail all topics
static OutputFormat g_publishFormat = 0;  //!< the OutputFormat for publishing messages
static bool g_ignoreInvalidParams = false;  //!< ignore invalid parameters during init
static bool g_isMqttV311 = false;

#if (LIBMOSQUITTO_MAJOR >= 1)
static const char* g_cafile = nullptr;    //!< CA file for TLS
static const char* g_capath = nullptr;    //!< CA path for TLS
static const char* g_certfile = nullptr;  //!< client certificate file for TLS
static const char* g_keyfile = nullptr;   //!< client key file for TLS
static const char* g_keypass = nullptr;   //!< client key file password for TLS
#endif

bool parseTopic(const string& topic, vector<string>* strs, vector<string>* fields);

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
    g_password = arg;
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
    g_publishFormat |= OF_JSON|OF_NAMES;
    break;

  case O_V311:  // --mqttv311
    g_isMqttV311 = true;
    break;

  case O_IGIN:
    g_ignoreInvalidParams = true;
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
      g_keypass = arg;
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

bool mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers) {
  if (g_port > 0) {
    int major = -1;
    mosquitto_lib_version(&major, nullptr, nullptr);
    if (major != LIBMOSQUITTO_MAJOR) {
      logOtherError("mqtt", "invalid mosquitto version %d instead of %d", major, LIBMOSQUITTO_MAJOR);
      return false;
    }
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


MqttHandler::MqttHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages)
  : DataSink(userInfo, "mqtt"), DataSource(busHandler), WaitThread(), m_messages(messages), m_connected(false),
    m_initialConnectFailed(false), m_lastUpdateCheckResult(".") {
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
  m_mosquitto = nullptr;
  if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) {
    logOtherError("mqtt", "unable to initialize");
  } else {
    signal(SIGPIPE, SIG_IGN);  // needed before libmosquitto v. 1.1.3
    ostringstream clientId;
    clientId << PACKAGE_NAME << '_' << PACKAGE_VERSION << '_' << static_cast<unsigned>(getpid());
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
    /*mosquitto_log_init(m_mosquitto, MOSQ_LOG_DEBUG | MOSQ_LOG_ERR | MOSQ_LOG_WARNING
                | MOSQ_LOG_NOTICE | MOSQ_LOG_INFO, MOSQ_LOG_STDERR);*/
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

    if (g_isMqttV311) {
      // set the MQTT protocol version to 3.1.1 as libmosquitto defaults to 3.1 otherwise
      int mqttProtocolVersion = MQTT_PROTOCOL_V311;

      if (mosquitto_opts_set(m_mosquitto, MOSQ_OPT_PROTOCOL_VERSION, &mqttProtocolVersion) != MOSQ_ERR_SUCCESS) {
          logOtherError("mqtt", "unable to set MQTT protocol version, defaulting to V31");
      }
    }
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
      }
    }
#endif
    mosquitto_connect_callback_set(m_mosquitto, on_connect);
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
    } else if (ret != MOSQ_ERR_SUCCESS) {
      m_connected = false;
      m_initialConnectFailed = g_ignoreInvalidParams;
      char* error;
      if (ret != MOSQ_ERR_ERRNO) {
        error = (char*)("unkown mosquitto error");
      } else {
        error = strerror(errno);
      }
      logOtherError("mqtt", "unable to connect, retrying: %s", error);
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
    mosquitto_subscribe(m_mosquitto, nullptr, m_subscribeTopic.c_str(), 0);
  }
}

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

void MqttHandler::notifyTopic(const string& topic, const string& data) {
  size_t pos = topic.rfind('/');
  if (pos == string::npos) {
    return;
  }
  string direction = topic.substr(pos+1);
  bool isWrite = false;
  if (direction.empty()) {
    return;
  }
  isWrite = direction == "set";
  if (!isWrite && direction != "get") {
    return;
  }

  logOtherDebug("mqtt", "received topic %s", topic.c_str(), data.c_str());
  string remain = topic.substr(0, pos);
  size_t last = 0;
  string circuit, name;
  size_t idx;
  for (idx = 0; idx < g_topicStrs.size()+1; idx++) {
    string field;
    string chk;
    if (idx < g_topicStrs.size()) {
      chk = g_topicStrs[idx];
      pos = remain.find(chk, last);
      if (pos == string::npos) {
        return;
      }
    } else if (idx-1 < g_topicFields.size()) {
      pos = remain.size();
    } else if (last < remain.size()) {
      return;
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
        return;
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
  if (circuit.empty() || name.empty()) {
    return;
  }
  logOtherInfo("mqtt", "received topic for %s %s", circuit.c_str(), name.c_str());
  Message* message = m_messages->find(circuit, name, m_levels, isWrite);
  if (message == nullptr) {
    message = m_messages->find(circuit, name, m_levels, isWrite, true);
  }
  if (message == nullptr) {
    logOtherError("mqtt", "%s message %s %s not found", isWrite?"write":"read", circuit.c_str(), name.c_str());
    return;
  }
  if (!message->isPassive()) {
    result_t result = m_busHandler->readFromBus(message, data);
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

void MqttHandler::run() {
  time_t lastTaskRun, now, start, lastSignal = 0;
  bool signal = false;
  string signalTopic = m_globalTopic+"signal";
  string uptimeTopic = m_globalTopic+"uptime";
  ostringstream updates;

  time(&now);
  start = lastTaskRun = now;
  const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
  publishTopic(m_globalTopic+"version", sep + (PACKAGE_STRING "." REVISION) + sep, true);
  publishTopic(m_globalTopic+"running", "true", true);
  publishTopic(signalTopic, "false");
  mosquitto_message_callback_set(m_mosquitto, on_message);
  bool allowReconnect = false;
  while (isRunning()) {
    handleTraffic(allowReconnect);
    allowReconnect = false;
    time(&now);
    if (now < start) {
      // clock skew
      if (now < lastSignal) {
        lastSignal -= lastTaskRun-now;
      }
      lastTaskRun = now;
    } else if (now > lastTaskRun+15) {
      allowReconnect = true;
      if (m_busHandler->hasSignal()) {
        lastSignal = now;
        if (!signal) {
          signal = true;
          publishTopic(signalTopic, "true");
        }
      } else {
        if (signal) {
          signal = false;
          publishTopic(signalTopic, "false");
        }
      }
      time_t uptime = now-start;
      updates.str("");
      updates.clear();
      updates << dec << static_cast<unsigned>(uptime);
      publishTopic(uptimeTopic, updates.str());
      time(&lastTaskRun);
    }
    if (!m_updatedMessages.empty()) {
      if (m_connected) {
        m_messages->lock();
        for (auto it = m_updatedMessages.begin(); it != m_updatedMessages.end(); ) {
          const vector<Message*>* messages = m_messages->getByKey(it->first);
          if (messages) {
            updates.str("");
            updates.clear();
            updates << dec;
            for (auto message : *messages) {
              if (message->getLastChangeTime() > 0 && message->isAvailable()) {
                publishMessage(message, &updates);
              }
            }
          }
          it = m_updatedMessages.erase(it);
        }
        m_messages->unlock();
      } else {
        m_updatedMessages.clear();
      }
    }
    if (!m_connected && !Wait(5)) {
      break;
    }
  }
}

void MqttHandler::handleTraffic(bool allowReconnect) {
  if (m_mosquitto) {
    int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
    ret = mosquitto_loop(m_mosquitto, -1, 1);  // waits up to 1 second for network traffic
#else
    ret = mosquitto_loop(m_mosquitto, -1);  // waits up to 1 second for network traffic
#endif
    if (!m_connected && ret == MOSQ_ERR_NO_CONN && allowReconnect) {
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
      return;
    }
    if (ret == MOSQ_ERR_NO_CONN || ret == MOSQ_ERR_CONN_LOST || ret == MOSQ_ERR_CONN_REFUSED) {
      logOtherError("mqtt", "communication error: %s", ret == MOSQ_ERR_NO_CONN ? "not connected"
                    : (ret == MOSQ_ERR_CONN_LOST ? "connection lost" : "connection refused"));
      m_connected = false;
    } else {
      logOtherError("mqtt", "communication error: %d", ret);
    }
  }
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
        message->dumpField(g_topicFields[i], false, &ret);
      }
    }
  }
  if (!suffix.empty()) {
    ret << suffix;
  }
  return ret.str();
}

void MqttHandler::publishMessage(const Message* message, ostringstream* updates) {
  OutputFormat outputFormat = g_publishFormat;
  bool json = outputFormat & OF_JSON;
  if (!m_publishByField) {
    if (json) {
      *updates << "{";
    }
    result_t result = message->decodeLastData(false, nullptr, -1, outputFormat, updates);
    if (result != RESULT_OK) {
      logOtherError("mqtt", "decode %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
          getResultCode(result));
      return;
    }
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
  logOtherDebug("mqtt", "publish %s %s", topic.c_str(), data.c_str());
  mosquitto_publish(m_mosquitto, nullptr, topic.c_str(), (uint32_t)data.size(),
      reinterpret_cast<const uint8_t*>(data.c_str()), 0, g_retain || retain);
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2017 John Baier <ebusd@ebusd.eu>
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
#define O_CAFI (O_JSON+1)
#define O_CERT (O_CAFI+1)
#define O_KEYF (O_CERT+1)
#define O_KEPA (O_KEYF+1)

/** the definition of the MQTT arguments. */
static const struct argp_option g_mqtt_argp_options[] = {
  {NULL,          0,      NULL,       0, "MQTT options:", 1 },
  {"mqtthost",    O_HOST, "HOST",     0, "Connect to MQTT broker on HOST [localhost]", 0 },
  {"mqttport",    O_PORT, "PORT",     0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]", 0 },
  {"mqttuser",    O_USER, "USER",     0, "Connect as USER to MQTT broker (no default)", 0 },
  {"mqttpass",    O_PASS, "PASSWORD", 0, "Use PASSWORD when connecting to MQTT broker (no default)", 0 },
  {"mqtttopic",   O_TOPI, "TOPIC",    0, "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]",
      0 },
  {"mqttretain",  O_RETA, NULL,       0, "Retain all topics instead of only selected global ones", 0 },
  {"mqttjson",    O_JSON, NULL,       0, "Publish in JSON format instead of strings", 0 },

#if (LIBMOSQUITTO_MAJOR >= 1)
  {"mqttca",      O_CAFI, "CA",       0, "Use CA file or dir (ending with '/') for MQTT TLS (no default)", 0 },
  {"mqttcert",    O_CERT, "CERTFILE", 0, "Use CERTFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkey",     O_KEYF, "KEYFILE",  0, "Use KEYFILE for MQTT TLS client certificate (no default)", 0 },
  {"mqttkeypass", O_KEPA, "PASSWORD", 0, "Use PASSWORD for the encrypted KEYFILE (no default)", 0 },
#endif

  {NULL,          0, NULL,       0, NULL, 0 },
};

static const char* g_host = "localhost";  //!< host name of MQTT broker [localhost]
static uint16_t g_port = 0;               //!< optional port of MQTT broker, 0 to disable [0]
static const char* g_username = NULL;     //!< optional user name for MQTT broker (no default)
static const char* g_password = NULL;     //!< optional password for MQTT broker (no default)
static const char* g_topic = PACKAGE;     //!< MQTT topic to use (prefix if without wildcards) [ebusd]
static bool g_retain = false;             //!< whether to retail all topics
static OutputFormat g_publishFormat = 0;  //!< the OutputFormat for publishing messages

#if (LIBMOSQUITTO_MAJOR >= 1)
static const char* g_cafile = NULL;    //!< CA file for TLS
static const char* g_capath = NULL;    //!< CA path for TLS
static const char* g_certfile = NULL;  //!< client certificate file for TLS
static const char* g_keyfile = NULL;   //!< client key file for TLS
static const char* g_keypass = NULL;   //!< client key file password for TLS
#endif


/**
 * The MQTT argument parsing function.
 * @param key the key from @a mqtt_argp_options.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
static error_t mqtt_parse_opt(int key, char *arg, struct argp_state *state) {
  result_t result = RESULT_OK;

  switch (key) {
  case O_HOST:  // --mqtthost=localhost
    if (arg == NULL || arg[0] == 0) {
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
    if (arg == NULL) {
      argp_error(state, "invalid mqttuser");
      return EINVAL;
    }
    g_username = arg;
    break;

  case O_PASS:  // --mqttpass=password
    if (arg == NULL) {
      argp_error(state, "invalid mqttpass");
      return EINVAL;
    }
    g_password = arg;
    break;

  case O_TOPI:  // --mqtttopic=ebusd
    if (arg == NULL || arg[0] == 0 || strchr(arg, '#') || strchr(arg, '+') || arg[strlen(arg)-1] == '/') {
      argp_error(state, "invalid mqtttopic");
      return EINVAL;
    }
    g_topic = arg;
    break;

  case O_RETA:  // --mqttretain
    g_retain = true;
    break;

  case O_JSON:  // --mqttjson
    g_publishFormat |= OF_JSON|OF_NAMES;
    break;

#if (LIBMOSQUITTO_MAJOR >= 1)
    case O_CAFI:  // --mqttca=file or --mqttca=dir/
      if (arg == NULL || arg[0] == 0) {
        argp_error(state, "invalid mqttca");
        return EINVAL;
      }
      if (arg[strlen(arg)-1] == '/') {
        g_cafile = NULL;
        g_capath = arg;
      } else {
        g_cafile = arg;
        g_capath = NULL;
      }
      break;

    case O_CERT:  // --mqttcert=CERTFILE
      if (arg == NULL || arg[0] == 0) {
        argp_error(state, "invalid mqttcert");
        return EINVAL;
      }
      g_certfile = arg;
      break;

    case O_KEYF:  // --mqttkey=KEYFILE
      if (arg == NULL || arg[0] == 0) {
        argp_error(state, "invalid mqttkey");
        return EINVAL;
      }
      g_keyfile = arg;
      break;

    case O_KEPA:  // --mqttkeypass=PASSWORD
      if (arg == NULL) {
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

static const struct argp g_mqtt_argp = { g_mqtt_argp_options, mqtt_parse_opt, NULL, NULL, NULL, NULL, NULL };
static const struct argp_child g_mqtt_argp_child = {&g_mqtt_argp, 0, "", 1};

const struct argp_child* mqtthandler_getargs() {
  return &g_mqtt_argp_child;
}

DataHandler* mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages) {
  return new MqttHandler(userInfo, busHandler, messages);
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
  : DataSink(userInfo, "mqtt"), DataSource(busHandler), Thread(), m_messages(messages), m_connected(false),
    m_lastUpdateCheckResult(".") {
  bool enabled = g_port != 0;
  m_publishByField = false;
  m_mosquitto = NULL;
  if (enabled && !parseTopic(g_topic, &m_topicStrs, &m_topicFields)) {
    logOtherError("mqtt", "malformed topic %s", g_topic);
    return;
  }
  if (!enabled) {
    return;
  }
  int major = -1;
  mosquitto_lib_version(&major, NULL, NULL);
  if (major != LIBMOSQUITTO_MAJOR) {
    logOtherError("mqtt", "invalid mosquitto version %d instead of %d", major, LIBMOSQUITTO_MAJOR);
    return;
  }
  if (m_topicFields.empty()) {
    if (m_topicStrs.empty()) {
      m_topicStrs.push_back("");
    } else {
      string str = m_topicStrs[0];
      if (str.empty() || str[str.length()-1] != '/') {
        m_topicStrs[0] = str+"/";
      }
    }
    m_topicFields.push_back("circuit");
    m_topicStrs.push_back("/");
    m_topicFields.push_back("name");
  } else {
    for (size_t i = 0; i < m_topicFields.size(); i++) {
      if (m_topicFields[i] == "field") {
        m_publishByField = true;
        break;
      }
    }
  }
  m_globalTopic = getTopic(NULL, "global/");
  m_mosquitto = NULL;
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
#if (LIBMOSQUITTO_MAJOR >= 1)
    if (mosquitto_connect(m_mosquitto, g_host, g_port, 60) != MOSQ_ERR_SUCCESS) {
#else
    if (mosquitto_connect(m_mosquitto, g_host, g_port, 60, true) != MOSQ_ERR_SUCCESS) {
#endif
      logOtherError("mqtt", "unable to connect");
      mosquitto_destroy(m_mosquitto);
      m_mosquitto = NULL;
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
    m_mosquitto = NULL;
  }
  mosquitto_lib_cleanup();
}

void MqttHandler::start() {
  if (m_mosquitto) {
    Thread::start("MQTT");
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
  for (idx = 0; idx < m_topicStrs.size()+1; idx++) {
    string field;
    string chk;
    if (idx < m_topicStrs.size()) {
      chk = m_topicStrs[idx];
      pos = remain.find(chk, last);
      if (pos == string::npos) {
        return;
      }
    } else if (idx-1 < m_topicFields.size()) {
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
      string fieldName = m_topicFields[idx-1];
      if (fieldName == "circuit") {
        circuit = field;
      } else if (fieldName == "name") {
        name = field;
      } else if (fieldName == "fields") {
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
  if (message == NULL) {
    message = m_messages->find(circuit, name, m_levels, isWrite, true);
  }
  if (message == NULL) {
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
  string subTopic = getTopic(NULL, "#");
  mosquitto_subscribe(m_mosquitto, NULL, subTopic.c_str(), 0);
  while (isRunning()) {
    handleTraffic();
    time(&now);
    if (now < start) {
      // clock skew
      if (now < lastSignal) {
        lastSignal -= lastTaskRun-now;
      }
      lastTaskRun = now;
    } else if (now > lastTaskRun+15) {
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
    if (m_connected && !m_updatedMessages.empty()) {
      for (const auto it : m_updatedMessages) {
        updates.str("");
        updates.clear();
        updates << dec;
        publishMessage(it.first, &updates);  // TODO avoid using the message while reload command is executed
      }
    }
    m_updatedMessages.clear();
  }
}

void MqttHandler::handleTraffic() {
  if (m_mosquitto) {
    int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
    ret = mosquitto_loop(m_mosquitto, -1, 1);
#else
    ret = mosquitto_loop(m_mosquitto, -1);
#endif
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
  for (size_t i = 0; i < m_topicStrs.size(); i++) {
    ret << m_topicStrs[i];
    if (!message) {
      break;
    }
    if (i < m_topicFields.size()) {
      if (m_topicFields[i] == "field") {
        ret << fieldName;
      } else {
        message->dumpField(m_topicFields[i], false, &ret);
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
    result_t result = message->decodeLastData(false, NULL, -1, outputFormat, updates);
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
    result_t result = message->decodeLastData(false, NULL, index, outputFormat, updates);
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
  mosquitto_publish(m_mosquitto, NULL, topic.c_str(), (uint32_t)data.size(),
      reinterpret_cast<const uint8_t*>(data.c_str()), 0, g_retain || retain);
}

}  // namespace ebusd

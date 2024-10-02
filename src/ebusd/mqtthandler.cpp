/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2024 John Baier <ebusd@ebusd.eu>
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
#include <algorithm>
#include <utility>
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
static const argDef g_mqtt_argDefs[] = {
  {nullptr,        0,      nullptr,      0, "MQTT options:"},
  {"mqtthost",     O_HOST, "HOST",       0, "Connect to MQTT broker on HOST [localhost]"},
  {"mqttport",     O_PORT, "PORT",       0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]"},
  {"mqttclientid", O_CLID, "ID",         0, "Set client ID for connection to MQTT broker [" PACKAGE_NAME "_"
   PACKAGE_VERSION "_<pid>]"},
  {"mqttuser",     O_USER, "USER",       0, "Connect as USER to MQTT broker (no default)"},
  {"mqttpass",     O_PASS, "PASSWORD",   0, "Use PASSWORD when connecting to MQTT broker (no default)"},
  {"mqtttopic",    O_TOPI, "TOPIC",      0,
   "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]"},
  {"mqttglobal",   O_GTOP, "TOPIC",      0,
   "Use TOPIC for global data (default is \"global/\" suffix to mqtttopic prefix)"},
  {"mqttretain",   O_RETA, nullptr,      0, "Retain all topics instead of only selected global ones"},
  {"mqttqos",      O_PQOS, "QOS",        0, "Set the QoS value for all topics (0-2) [0]"},
  {"mqttint",      O_INTF, "FILE",       0, "Read MQTT integration settings from FILE (no default)"},
  {"mqttvar",      O_IVAR, "NAME[+]=VALUE[,...]", 0, "Add variable(s) to the read MQTT integration settings "
   "(append to already existing value with \"NAME+=VALUE\")"},
  {"mqttjson",     O_JSON, "short", af_optional,
   "Publish in JSON format instead of strings, optionally in short (value directly below field key)"},
  {"mqttverbose",  O_VERB, nullptr,      0, "Publish all available attributes"},
  {"mqttlog",      O_LOGL, nullptr,      0, "Log library events"},
  {"mqttversion",  O_VERS, "VERSION",    0, "Use protocol VERSION [3.1]"},
  {"mqttignoreinvalid", O_IGIN, nullptr, 0,
   "Ignore invalid parameters during init (e.g. for DNS not resolvable yet)"},
  {"mqttchanges",  O_CHGS, nullptr,      0, "Whether to only publish changed messages instead of all received"},

  {"mqttca",       O_CAFI, "CA",         0, "Use CA file or dir (ending with '/') for MQTT TLS (no default)"},
  {"mqttcert",     O_CERT, "CERTFILE",   0, "Use CERTFILE for MQTT TLS client certificate (no default)"},
  {"mqttkey",      O_KEYF, "KEYFILE",    0, "Use KEYFILE for MQTT TLS client certificate (no default)"},
  {"mqttkeypass",  O_KEPA, "PASSWORD",   0, "Use PASSWORD for the encrypted KEYFILE (no default)"},
  {"mqttinsecure", O_INSE, nullptr,      0, "Allow insecure TLS connection (e.g. using a self signed certificate)"},

  {nullptr,        0,      nullptr,       0, nullptr},
};

// options for the MQTT client
static mqtt_client_config_t g_opt = {
  .host = "localhost",
  .port = 0,
  .clientId = nullptr,
  .username = nullptr,
  .password = nullptr,
  .logEvents = false,
  .version311 = false,
  .ignoreInvalidParams = false,
  .cafile = nullptr,
  .capath = nullptr,
  .certfile = nullptr,
  .keyfile = nullptr,
  .keypass = nullptr,
  .insecure = false,
  .lastWillTopic = nullptr,
  .lastWillData = nullptr,
};
static const char* g_topic = nullptr;     //!< optional topic template
static const char* g_globalTopic = nullptr;  //!< optional global topic
static const char* g_integrationFile = nullptr;  //!< the integration settings file
static vector<string>* g_integrationVars = nullptr;  //!< the integration settings variables
static bool g_retain = false;             //!< whether to retail all topics
static int g_qos = 0;                     //!< the qos value for all topics
static OutputFormat g_publishFormat = OF_NONE;  //!< the OutputFormat for publishing messages
static bool g_onlyChanges = false;        //!< whether to only publish changed messages instead of all received

/**
 * Replace all characters in the string with a space and return a copy of the original string.
 * @param arg the string to replace.
 * @return a copy of the original string.
 */
static char* replaceSecret(char *arg) {
  char* ret = strdup(arg);
  int cnt = 0;
  while (*arg && cnt++ < 256) {
    *arg++ = ' ';
  }
  return ret;
}

void splitFields(const string& str, vector<string>* row);

/**
 * The MQTT argument parsing function.
 * @param key the key from @a g_mqtt_argDefs.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
static int mqtt_parse_opt(int key, char *arg, const argParseOpt *parseOpt, void *userArg) {
  result_t result = RESULT_OK;
  unsigned int value;
  switch (key) {
  case O_HOST:  // --mqtthost=localhost
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid mqtthost");
      return EINVAL;
    }
    g_opt.host = arg;
    break;

  case O_PORT:  // --mqttport=1883
    value = parseInt(arg, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid mqttport");
      return EINVAL;
    }
    g_opt.port = (uint16_t)value;
    break;

  case O_CLID:  // --mqttclientid=clientid
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid mqttclientid");
      return EINVAL;
    }
    g_opt.clientId = arg;
    break;

  case O_USER:  // --mqttuser=username
    if (arg == nullptr) {
      argParseError(parseOpt, "invalid mqttuser");
      return EINVAL;
    }
    g_opt.username = arg;
    break;

  case O_PASS:  // --mqttpass=password
    if (arg == nullptr) {
      argParseError(parseOpt, "invalid mqttpass");
      return EINVAL;
    }
    g_opt.password = replaceSecret(arg);
    break;

  case O_TOPI:  // --mqtttopic=ebusd
  {
    if (arg == nullptr || arg[0] == 0 || arg[0] == '/' || strchr(arg, '+') || arg[strlen(arg)-1] == '/') {
      argParseError(parseOpt, "invalid mqtttopic");
      return EINVAL;
    }
    char *pos = strchr(arg, '#');
    if (pos && (pos == arg || pos[1])) {  // allow # only at very last position (to indicate not using any default)
      argParseError(parseOpt, "invalid mqtttopic");
      return EINVAL;
    }
    if (g_topic) {
      argParseError(parseOpt, "duplicate mqtttopic");
      return EINVAL;
    }
    StringReplacer replacer;
    if (!replacer.parse(arg, true)) {
      argParseError(parseOpt, "malformed mqtttopic");
      return ESRCH;  // abort in any case due to the above potentially being destructive
    }
    g_topic = arg;
    break;
  }

  case O_GTOP:  // --mqttglobal=global/
    if (arg == nullptr || strchr(arg, '+') || strchr(arg, '#')) {
      argParseError(parseOpt, "invalid mqttglobal");
      return EINVAL;
    }
    g_globalTopic = arg;
    break;

  case O_RETA:  // --mqttretain
    g_retain = true;
    break;

  case O_PQOS:  // --mqttqos=0
    value = parseInt(arg, 10, 0, 2, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid mqttqos value");
      return EINVAL;
    }
    g_qos = static_cast<signed>(value);
    break;

  case O_INTF:  // --mqttint=/etc/ebusd/mqttint.cfg
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid mqttint file");
      return EINVAL;
    }
    g_integrationFile = arg;
    break;

  case O_IVAR:  // --mqttvar=NAME=VALUE[,NAME=VALUE]*
    if (arg == nullptr || arg[0] == 0 || !strchr(arg, '=')) {
      argParseError(parseOpt, "invalid mqttvar");
      return EINVAL;
    }
    if (!g_integrationVars) {
      g_integrationVars = new vector<string>();
    }
    splitFields(arg, g_integrationVars);
    break;

  case O_JSON:  // --mqttjson[=short]
    g_publishFormat |= OF_JSON|OF_NAMES;
    if (arg && strcmp("short", arg) == 0) {
      g_publishFormat = (g_publishFormat & ~(OF_NAMES|OF_UNITS|OF_COMMENTS|OF_ALL_ATTRS)) | OF_SHORT;
    }
    break;

  case O_VERB:  // --mqttverbose
    g_publishFormat = (g_publishFormat & ~OF_SHORT) | OF_NAMES|OF_UNITS|OF_COMMENTS|OF_ALL_ATTRS;
    break;

  case O_LOGL:
    g_opt.logEvents = true;
    break;

  case O_VERS:  // --mqttversion=3.1.1
    if (arg == nullptr || arg[0] == 0 || (strcmp(arg, "3.1") != 0 && strcmp(arg, "3.1.1") != 0)) {
      argParseError(parseOpt, "invalid mqttversion");
      return EINVAL;
    }
    g_opt.version311 = strcmp(arg, "3.1.1") == 0;
    break;

  case O_IGIN:
    g_opt.ignoreInvalidParams = true;
    break;

  case O_CHGS:
    g_onlyChanges = true;
    break;

  case O_CAFI:  // --mqttca=file or --mqttca=dir/
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid mqttca");
      return EINVAL;
    }
    if (arg[strlen(arg)-1] == '/') {
      g_opt.cafile = nullptr;
      g_opt.capath = arg;
    } else {
      g_opt.cafile = arg;
      g_opt.capath = nullptr;
    }
    break;

  case O_CERT:  // --mqttcert=CERTFILE
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid mqttcert");
      return EINVAL;
    }
    g_opt.certfile = arg;
    break;

  case O_KEYF:  // --mqttkey=KEYFILE
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid mqttkey");
      return EINVAL;
    }
    g_opt.keyfile = arg;
    break;

  case O_KEPA:  // --mqttkeypass=PASSWORD
    if (arg == nullptr) {
      argParseError(parseOpt, "invalid mqttkeypass");
      return EINVAL;
    }
    g_opt.keypass = replaceSecret(arg);
    break;
  case O_INSE:  // --mqttinsecure
    g_opt.insecure = true;
    break;

  default:
    return EINVAL;
  }
  return 0;
}

static const argParseChildOpt g_mqtt_arg_child = {
  g_mqtt_argDefs, mqtt_parse_opt
};


const argParseChildOpt* mqtthandler_getargs() {
  return &g_mqtt_arg_child;
}

bool mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers) {
  if (g_opt.port > 0) {
    handlers->push_back(new MqttHandler(userInfo, busHandler, messages));
  }
  return true;
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
  if (pos == string::npos) {
    return str;
  }
  return str.substr(0, pos + 1);
}

MqttHandler::MqttHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages)
  : DataSink(userInfo, "mqtt", g_onlyChanges), DataSource(busHandler), WaitThread(),
    m_messages(messages), m_connected(false),
    m_lastUpdateCheckResult("."), m_lastScanStatus(SCAN_STATUS_NONE) {
  m_definitionsSince = 0;
  m_client = nullptr;
  bool hasIntegration = false;
  if (g_integrationFile != nullptr) {
    if (!m_replacers.parseFile(g_integrationFile)) {
      logOtherError("mqtt", "unable to open integration file %s", g_integrationFile);
    } else {
      hasIntegration = true;
      if (g_integrationVars) {
        for (auto& str : *g_integrationVars) {
          m_replacers.parseLine(str);
        }
      }
    }
  }
  if (g_integrationVars) {
    delete g_integrationVars;
    g_integrationVars = nullptr;
  }
  // determine topic and prefix
  StringReplacer& topic = m_replacers.get("topic");
  if (g_topic) {
    string str = g_topic;
    bool noDefault = str[str.size()-1] == '#';
    if (noDefault) {
      str.resize(str.size()-1);
    }
    bool parse = true;
    if (hasIntegration && !topic.empty()) {
      // topic defined in cmdline and integration file.
      if (str.find('%') == string::npos) {
        // cmdline topic is only the prefix, use it
        m_replacers.set("prefix", str);
        m_replacers.set("prefixn", removeTrailingNonTopicPart(str));
        parse = false;
      }  // else: cmdline topic is more than just a prefix => override integration topic completely
    }
    if (parse) {
      if (!topic.parse(str, true, true)) {
        logOtherNotice("mqtt", "unknown or duplicate topic parts potentially prevent matching incoming topics");
        topic.parse(str, true);
      } else if (!topic.checkMatchability()) {
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
      if (line.empty() || line == "/") {
        line = string(PACKAGE);  // ensure prefix if cmdline topic is absent
      }
      m_replacers.set("prefix", line);
      m_replacers.set("prefixn", removeTrailingNonTopicPart(line));
    }
    m_replacers.reduce(true);
    if (!m_replacers["type_switch-names"].empty() || m_replacers.uses("type_switch")) {
      ostringstream ostr;
      for (int i=-1; i<static_cast<signed>(sizeof(directionNames)/sizeof(char*)); i++) {
        for (auto typeName : typeNames) {
          ostr.str("");
          if (i >= 0) {
            ostr << directionNames[i] << '-';
          }
          ostr << typeName;
          const string suffix = ostr.str();
          string str = m_replacers.get("type_switch-" + suffix, false, false, "type_switch");
          if (str.empty()) {
            continue;
          }
          str += '\n';  // add trailing newline to ease the split
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
  if (!g_opt.clientId) {
    ostringstream clientId;
    clientId << PACKAGE_NAME << '_' << PACKAGE_VERSION << '_' << static_cast<unsigned>(getpid());
    g_opt.clientId = strdup(clientId.str().c_str());
  }
  if (g_opt.password && !g_opt.username) {
    g_opt.username = PACKAGE;
  }
  string willTopic = m_globalTopic.get("", "running");
  if (!willTopic.empty()) {
    g_opt.lastWillTopic = strdup(willTopic.c_str());
    g_opt.lastWillData = "false";
  }
  m_client = MqttClient::create(g_opt, this);
  m_isAsync = false;
  bool ret = m_client->connect(m_isAsync, m_connected);
  if (!ret) {
    logOtherError("mqtt", "unable to connect (invalid parameters)");
    delete m_client;
    m_client = nullptr;
  }
}

MqttHandler::~MqttHandler() {
  join();
  if (m_client) {
    delete m_client;
    m_client = nullptr;
  }
}

void MqttHandler::startHandler() {
  if (m_client) {
    WaitThread::start("MQTT");
  }
}

void MqttHandler::notifyMqttStatus(bool connected) {
  if (connected && m_client && isRunning()) {
    const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
    if (m_globalTopic.has("name")) {
      m_client->publishTopic(m_globalTopic.get("", "version"), sep + (PACKAGE_STRING "." REVISION) + sep, true);
    }
    publishTopic(m_globalTopic.get("", "running"), "true", true);
    if (!m_staticTopic) {
      m_client->subscribeTopic(m_subscribeTopic);
      if (!m_subscribeConfigRestartTopic.empty()) {
        m_client->subscribeTopic(m_subscribeConfigRestartTopic);
      }
    }
  }
}

void MqttHandler::notifyMqttTopic(const string& topic, const string& data) {
  size_t pos = topic.rfind('/');
  if (pos == string::npos) {
    return;
  }
  if (!m_subscribeConfigRestartTopic.empty() && topic == m_subscribeConfigRestartTopic) {
    logOtherDebug("mqtt", "received restart topic %s with data %s", topic.c_str(), data.c_str());
    if (m_subscribeConfigRestartPayload.empty() || data == m_subscribeConfigRestartPayload) {
      m_definitionsSince = 0;
    }
    return;
  }
  string direction = topic.substr(pos+1);
  if (direction.empty()) {
    return;
  }
  string matchTopic = topic.substr(0, pos);
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
  ssize_t match = m_replacers.get("topic").match(matchTopic, &circuit, &name, &field);
  if (match < 0 && !isList) {
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

void MqttHandler::notifyScanStatus(scanStatus_t scanStatus) {
  if (scanStatus != m_lastScanStatus) {
    m_lastScanStatus = scanStatus;
    if (m_globalTopic.has("name")) {
      const string sep = (g_publishFormat & OF_JSON) ? "\"" : "";
      string message;
      switch (scanStatus) {
        case SCAN_STATUS_RUNNING:
          message = "running";
          break;
        case SCAN_STATUS_FINISHED:
          message = "finished";
          break;
        default:
          message = "OK";
      }
      publishTopic(m_globalTopic.get("", "scan"), sep + message + sep, true);
    }
  }
}

bool parseBool(const string& str) {
  return !str.empty() && !(str == "0" || str == "no" || str == "false");
}

void splitFields(const string& str, vector<string>* row) {
  std::istringstream istr;
  istr.str(str);
  unsigned int lineNo = 0;
  FileReader::splitFields(&istr, row, &lineNo, nullptr, nullptr, false);
}

void MqttHandler::run() {
  time_t lastTaskRun, now, start, lastSignal = 0;
  bool signal = false;
  bool globalHasName = m_globalTopic.has("name");
  string signalTopic = m_globalTopic.get("", "signal");
  string uptimeTopic = m_globalTopic.get("", "uptime");
  ostringstream updates;
  unsigned int filterPriority = 0;
  unsigned int filterSeen = 0;
  string filterCircuit, filterNonCircuit, filterName, filterNonName, filterField, filterNonField,
      filterLevel, filterDirection;
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
    filterNonCircuit = m_replacers["filter-non-circuit"];
    FileReader::tolower(&filterNonCircuit);
    filterName = m_replacers["filter-name"];
    FileReader::tolower(&filterName);
    filterNonName = m_replacers["filter-non-name"];
    FileReader::tolower(&filterNonName);
    filterField = m_replacers["filter-field"];
    FileReader::tolower(&filterField);
    filterNonField = m_replacers["filter-non-field"];
    FileReader::tolower(&filterNonField);
    filterLevel = m_replacers["filter-level"];
    FileReader::tolower(&filterLevel);
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
    bool needsWait = m_isAsync || handleTraffic(allowReconnect);
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
        publishDefinition(m_replacers, "def_global_running-", m_globalTopic.get("", "running"), "global", "running",
                          "def_global-");
        if (globalHasName) {
          publishDefinition(m_replacers, "def_global_version-", m_globalTopic.get("", "version"), "global", "version",
                            "def_global-");
          publishDefinition(m_replacers, "def_global_signal-", signalTopic, "global", "signal", "def_global-");
          publishDefinition(m_replacers, "def_global_uptime-", uptimeTopic, "global", "uptime", "def_global-");
          publishDefinition(m_replacers, "def_global_updatecheck-", m_globalTopic.get("", "updatecheck"), "global",
                            "updatecheck", "def_global-");
          if (m_busHandler->getProtocol()->supportsUpdateCheck()) {
            publishDefinition(m_replacers, "def_global_updatecheck_device-", m_globalTopic.get("", "updatecheck"),
                              "global", "updatecheck_device", "");
          }
          publishDefinition(m_replacers, "def_global_scan-", m_globalTopic.get("", "scan"), "global", "scan",
                            "def_global-");
        }
        m_definitionsSince = 1;
      }
      if (m_connected && m_hasDefinitionTopic) {
        ostringstream ostr;
        deque<Message*> messages;
        m_messages->findAll("", "", m_levels, false, true, true, true, true, true, 0, 0, false, &messages);
        bool includeActiveWrite = FileReader::matches("w", filterDirection, true, true);
        for (const auto& message : messages) {
          bool checkPollAdjust = false;
          if (filterSeen > 0) {
            if (message->getLastUpdateTime() == 0) {
              if (message->isPassive()) {
                // only wait for data on passive messages
                continue;  // no data ever
              }
              if (!message->isWrite()) {
                // only wait for data on read messages or set their poll prio
                if (filterSeen > 1 && (!message->getPollPriority() || message->getPollPriority() > filterSeen)
                  && (filterPriority == 0 || filterSeen <= filterPriority)
                ) {
                  // check for poll prio adjustment after all other filters
                  checkPollAdjust = true;
                } else {
                  continue;  // no poll adjustment
                }
              }
            }
            if (message->getDataHandlerState()&1) {
              // already seen in the past, check for poll prio update
              if (m_definitionsSince > 1 && message->getCreateTime() <= m_definitionsSince) {
                continue;
              }
            }
            message->setDataHandlerState(1, true);
          } else if (message->getCreateTime() <= m_definitionsSince  // only newer defined
          && (!message->isConditional()  // unless conditional
            || message->getAvailableSinceTime() <= m_definitionsSince)) {
            continue;
          }
          if (!FileReader::matches(message->getCircuit(), filterCircuit, true, true)
          || (!filterNonCircuit.empty() && FileReader::matches(message->getCircuit(), filterNonCircuit, true, true))
          || !FileReader::matches(message->getName(), filterName, true, true)
          || (!filterNonName.empty() && FileReader::matches(message->getName(), filterNonName, true, true))
          || !FileReader::matches(message->getLevel(), filterLevel, true, true)) {
            continue;
          }
          const string direction = directionNames[(message->isWrite() ? 2 : 0) + (message->isPassive() ? 1 : 0)];
          if (!FileReader::matches(direction, filterDirection, true, true)) {
            continue;
          }
          if (checkPollAdjust) {
            if (!message->setPollPriority(filterSeen)) {
              continue;
            }
            m_messages->addPollMessage(false, message);
          }
          if (filterPriority > 0 && (message->getPollPriority() == 0 || message->getPollPriority() > filterPriority)) {
            continue;
          }
          if (includeActiveWrite) {
            if (message->isWrite()) {
              bool skipMultiFieldWrite = (!m_hasDefinitionFieldsPayload || m_publishByField)
              && !message->isPassive() && message->getFieldCount() > 1;
              if (skipMultiFieldWrite) {
                // multi-field message is not writable when publishing by field or combining
                // multiple fields in one definition, so skip it
                continue;
              }
            } else {
              // check for existance of write message with same name
              Message* write = m_messages->find(message->getCircuit(), message->getName(), "", true);
              if (write) {
                bool skipMultiFieldWrite = (!m_hasDefinitionFieldsPayload || m_publishByField)
                && write->getFieldCount() > 1;
                if (!skipMultiFieldWrite) {
                  continue;  // avoid sending definition of read AND write message with the same key
                }
                // else: multi-field write message is not writable when publishing by field or combining
                // multiple fields in one definition, so skip it
              }
            }
          }
          StringReplacers msgValues = m_replacers;  // need a copy here as the contents are manipulated
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
            if (!FileReader::matches(fieldName, filterField, true, true)
            || (!filterNonField.empty() && FileReader::matches(fieldName, filterNonField, true, true))) {
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
                typeStr = dt->hasTime() ? "datetime" : "date";
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
            StringReplacers values = msgValues;  // need a copy here as the contents are manipulated
            values.set("index", static_cast<signed>(index));
            values.set("field", fieldName);
            values.set("fieldname", field->getName(-1));
            values.set("type", typeStr);
            values.set("type_map", str);
            values.set("basetype", dataType->getId());
            values.set("comment", field->getAttribute("comment"));
            values.set("unit", field->getAttribute("unit"));
            if (dataType->isNumeric() && !dataType->hasFlag(EXP)) {
              auto dt = dynamic_cast<const NumberDataType*>(dataType);
              ostr.str("");
              if (dt->getMinMax(false, g_publishFormat, &ostr) == RESULT_OK) {
                values.set("min", ostr.str());
                ostr.str("");
              }
              if (dt->getMinMax(true, g_publishFormat, &ostr) == RESULT_OK) {
                values.set("max", ostr.str());
                ostr.str("");
              }
              if (dt->getStep(g_publishFormat, &ostr) != RESULT_OK) {
                // fallback method, when smallest number didn't work
                int divisor = dt->getDivisor();
                float step = 1.0f;
                if (divisor > 1) {
                  step /= static_cast<float>(divisor);
                } else if (divisor < 0) {
                  step *= static_cast<float>(-divisor);
                }
                ostr << static_cast<float>(step);
              }
              values.set("step", ostr.str());
            }
            if (dataType->isNumeric() && field->isList() && !values["field_values-entry"].empty()) {
              auto vl = (dynamic_cast<const ValueListDataField*>(field))->getList();
              string entryFormat = values["field_values-entry"];
              string::size_type pos = -1;
              while ((pos = entryFormat.find('$', pos+1)) != string::npos) {
                if (entryFormat.substr(pos+1, 4) == "text" || entryFormat.substr(pos+1, 5) == "value") {
                  entryFormat.replace(pos, 1, "%");
                }
              }
              entryFormat.replace(0, 0, "entry = ");
              string result = values["field_values-prefix"];
              bool first = true;
              for (const auto& it : vl) {
                StringReplacers entry;
                entry.parseLine(entryFormat);
                entry.set("value", it.first);
                entry.set("text", it.second);
                entry.reduce();
                if (first) {
                  first = false;
                } else {
                  result += values["field_values-separator"];
                }
                result += entry.get("entry", false, false);
              }
              result += values["field_values-suffix"];
              values.set("field_values", result);
            }
            if (!m_typeSwitches.empty()) {
              values.reduce(true);
              str = values.get("type_switch-by", false, false);
              string typeSwitch;
              for (int i = 0; i < 2; i++) {
                ostr.str("");
                if (i == 0) {
                  ostr << direction << '-';
                }
                ostr << typeStr;
                const string key = ostr.str();
                for (auto const &check : m_typeSwitches[key]) {
                  if (FileReader::matches(str, check.second, true, true)) {
                    typeSwitch = check.first;
                    i = 2;  // early exit
                    break;
                  }
                }
              }
              values.set("type_switch", typeSwitch);
              if (!typeSwitchNames.empty()) {
                vector<string> strs;
                splitFields(typeSwitch, &strs);
                for (size_t pos = 0; pos < typeSwitchNames.size(); pos++) {
                  values.set(typeSwitchNames[pos], pos < strs.size() ? strs[pos] : "");
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
                if (fields.tellp() > 0) {
                  fields << values["field-separator"];
                }
                fields << value;
              }
              continue;
            }
            publishDefinition(values);
          }
          if (fields.tellp() > 0) {
            msgValues.set("fields_payload", fields.str());
            publishDefinition(msgValues);
          }
          if (filterSeen && message->getLastUpdateTime() > message->getCreateTime()) {
            // ensure data is published as well
            m_updatedMessages[message->getKey()]++;
          } else if (filterSeen && direction == "w") {
            // publish data for read pendant of write message
            Message* read = m_messages->find(message->getCircuit(), message->getName(), "", false);
            if (read && read->getLastUpdateTime() > 0) {
              m_updatedMessages[read->getKey()]++;
            }
          }
        }
        m_definitionsSince = now+1;  // +1 to not do the same ones again
        needsWait = true;
      }
      time(&lastTaskRun);
    }
    if (sendSignal) {
      if (m_busHandler->getProtocol()->hasSignal()) {
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
              time_t changeTime = message->getLastChangeTime();
              if (changeTime > 0 && message->isAvailable()) {
                updates.str("");
                updates.clear();
                updates << dec;
                publishMessage(message, &updates);
              }
            }
          }
          it = m_updatedMessages.erase(it);
        }
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

void MqttHandler::publishDefinition(StringReplacers values, const string& prefix, const string& topic,
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

void MqttHandler::publishDefinition(const StringReplacers& values) {
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
  if (!m_client) {
    return false;
  }
  return m_client->run(allowReconnect, m_connected);
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
        *updates << "\"circuit\":\"" << message->getCircuit() << "\",\"name\":\"" << message->getName()
                 << "\",\"fields\":{";
      }
    } else if (m_staticTopic) {
      *updates << message->getCircuit() << UI_FIELD_SEPARATOR << message->getName() << UI_FIELD_SEPARATOR;
    }
    result_t result = message->decodeLastData(pt_any, false, nullptr, -1, outputFormat, updates);
    if (result == RESULT_EMPTY) {
      publishEmptyTopic(getTopic(message));  // alternatively: , json ? "null" : "");
      return;
    }
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
    result_t result = message->decodeLastData(pt_any, false, nullptr, index, outputFormat, updates);
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
  logOtherDebug("mqtt", "publish %s %s", topicStr, dataStr);
  m_client->publishTopic(topicStr, data, g_qos, g_retain || retain);
}

void MqttHandler::publishEmptyTopic(const string& topic) {
  const char* topicStr = topic.c_str();
  logOtherDebug("mqtt", "publish empty %s", topicStr);
  m_client->publishEmptyTopic(topic, 0, g_retain);
}

}  // namespace ebusd

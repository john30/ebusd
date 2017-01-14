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
#	include <config.h>
#endif

#include "mqtthandler.h"
#include "log.h"

namespace ebusd {

using std::dec;

/** the definition of the MQTT arguments. */
static const struct argp_option g_mqtt_argp_options[] = {
	{NULL,             0, NULL,    0, "MQTT options:", 1 },
	{"mqtthost",       1, "HOST",  0, "Connect to MQTT broker on HOST [localhost]", 0 },
	{"mqttport",       2, "PORT",  0, "Connect to MQTT broker on PORT (usually 1883), 0 to disable [0]", 0 },
	{"mqtttopic",      3, "TOPIC", 0, "Use MQTT TOPIC (prefix before /%circuit/%name or complete format) [ebusd]", 0 },

	{NULL,             0,        NULL,    0, NULL, 0 },
};

static const char* g_host = "localhost"; //!< MQTT Host to use [localhost]
static uint16_t g_port = 0; //!< optional port of MQTT broker, 0 to disable [0]
static const char* g_topic = PACKAGE; //!< MQTT topic to use (prefix if without wildcards) [ebusd]


/**
 * The MQTT argument parsing function.
 * @param key the key from @a mqtt_argp_options.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
static error_t mqtt_parse_opt(int key, char *arg, struct argp_state *state) {
	result_t result = RESULT_OK;

	switch (key) {
	case 1: // --mqtthost=localhost
		if (arg == NULL || arg[0] == 0) {
			argp_error(state, "invalid mqtthost");
			return EINVAL;
		}
		g_host = arg;
		break;

	case 2: // --mqttport=1883
		g_port = (uint16_t)parseInt(arg, 10, 1, 65535, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid mqttport");
			return EINVAL;
		}
		break;

	case 3: // --mqtttopic=ebusd
		if (arg == NULL || arg[0] == 0 || arg[0] == '/' || arg[strlen(arg)-1] == '/') {
			argp_error(state, "invalid mqtttopic");
			return EINVAL;
		}
		g_topic = arg;
		break;
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

DataHandler* mqtthandler_register(BusHandler* busHandler, MessageMap* messages) {
	return new MqttHandler(busHandler, messages);
}

/** the known topic column names. */
static const char* columnNames[] = {
	"circuit",
	"name",
	"field",
};

/** the known topic column IDs. */
static const size_t columnIds[] = {
	COLUMN_CIRCUIT,
	COLUMN_NAME,
	COLUMN_FIELDS,
};

/** the number of known column names. */
static const size_t columnCount = sizeof(columnNames) / sizeof(char*);


/**
 * Parse the topic template.
 * @param topic the topic template.
 * @param strs the @a vector to which the string parts shall be added.
 * @param cols the @a vector to which the column parts shall be added.
 * @return true on success, false on malformed topic template.
 */
bool parseTopic(const string topic, vector<string> &strs, vector<size_t> &cols) {
	size_t lastpos = 0;
	size_t end = topic.length();
	vector<string> columns;
	for (size_t pos=topic.find('%', lastpos); pos != string::npos; ) {
		size_t col = columnCount;
		size_t len = 0;
		for (size_t i = 0; i < columnCount; i++) {
			len = strlen(columnNames[i]);
			if (topic.substr(pos+1, len) == columnNames[i]) {
				col = columnIds[i];
				break;
			}
		}
		if (col == columnCount) {
			return false;
		}
		for (vector<size_t>::iterator it=cols.begin(); it != cols.end(); it++) {
			if (*it == col) {
				return false; // duplicate column
			}
		}
		strs.push_back(topic.substr(lastpos, pos-lastpos));
		cols.push_back(col);
		lastpos = pos+1+len;
		pos = topic.find('%', lastpos);
	}
	if (lastpos < end) {
		strs.push_back(topic.substr(lastpos, end-lastpos));
	}
	return true;
}


MqttHandler::MqttHandler(BusHandler* busHandler, MessageMap* messages)
	: DataSink(), DataSource(busHandler), Thread(), m_messages(messages) {
	bool enabled = g_port != 0;
	m_publishByField = false;
	m_mosquitto = NULL;
	if (enabled && !parseTopic(g_topic, m_topicStrs, m_topicCols)) {
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
	if (m_topicCols.empty()) {
		if (m_topicStrs.empty()) {
			m_topicStrs.push_back("");
		} else {
			string str = m_topicStrs[0];
			if (str.empty() || str[str.length()-1] != '/') {
				m_topicStrs[0] = str+"/";
			}
		}
		m_topicCols.push_back(COLUMN_CIRCUIT); // circuit
		m_topicStrs.push_back("/");
		m_topicCols.push_back(COLUMN_NAME); // name
	} else {
		for (size_t i = 0; i < m_topicCols.size(); i++) {
			if (m_topicCols[i] == COLUMN_FIELDS) { // fields
				m_publishByField = true;
				break;
			}
		}
	}
	m_globalTopic = getTopic(NULL)+"global/";
	m_mosquitto = NULL;
	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) {
		logOtherError("mqtt", "unable to initialize");
	} else {
		string clientId = PACKAGE_STRING;
		clientId += " "+static_cast<unsigned>(getpid());
		m_mosquitto = mosquitto_new(clientId.c_str(),
#if (LIBMOSQUITTO_MAJOR >= 1)
			true,
#endif
			this);
		if (!m_mosquitto) {
			logOtherError("mqtt", "unable to instantiate");
		}
	}
	if (m_mosquitto) {
		/*mosquitto_log_init(m_mosquitto, MOSQ_LOG_DEBUG | MOSQ_LOG_ERR | MOSQ_LOG_WARNING
							  | MOSQ_LOG_NOTICE | MOSQ_LOG_INFO, MOSQ_LOG_STDERR);*/
		string willTopic = m_globalTopic+"running";
		string willData = "false";
		size_t len = willData.length();
		mosquitto_will_set(m_mosquitto,
#if (LIBMOSQUITTO_MAJOR < 1)
			true,
#endif
			willTopic.c_str(), (uint32_t)len, reinterpret_cast<const uint8_t*>(willData.c_str()), 0, true);
		if (mosquitto_connect(m_mosquitto, g_host, g_port, 60
#if (LIBMOSQUITTO_MAJOR < 1)
				, true
#endif
				) != MOSQ_ERR_SUCCESS) {
			logOtherError("mqtt", "unable to connect");
			mosquitto_destroy(m_mosquitto);
			m_mosquitto = NULL;
		} else {
			logOtherNotice("mqtt", "connection established");
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

void MqttHandler::notifyTopic(string topic, string data) {
	size_t pos = topic.rfind('/');
	if (pos == string::npos) {
		return;
	}
	string suffix = topic.substr(pos+1);
	bool isWrite = false;
	if (suffix.empty()) {
		return;
	}
	string direction = suffix.substr(0, 3);
	isWrite = direction == "set";
	if (!isWrite && direction != "get") {
		return;
	}
	suffix = suffix.substr(3); // security level
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
		} else if (idx-1 < m_topicCols.size()) {
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
			switch (m_topicCols[idx-1]) {
			case COLUMN_CIRCUIT:
				circuit = field;
				break;
			case COLUMN_NAME:
				name = field;
				break;
			case COLUMN_FIELDS:
				//field = field; // TODO add support for writing a single field
				break;
			default:
				return;
			}
		}
	}
	if (circuit.empty() || name.empty()) {
		return;
	}
	logOtherInfo("mqtt", "received topic for %s %s", circuit.c_str(), name.c_str());
	if (suffix.length() > 0) {
		circuit += "#"+suffix;
	}
	Message* message = m_messages->find(circuit, name, isWrite);
	if (message == NULL) {
		message = m_messages->find(circuit, name, isWrite, true);
	}
	if (message == NULL) {
		logOtherError("mqtt", "%s message %s %s not found", isWrite?"write":"read", circuit.c_str(), name.c_str());
		return;
	}
	if (!message->isPassive()) {
		result_t result = m_busHandler->readFromBus(message, data);
		if (result != RESULT_OK) {
			logOtherError("mqtt", "%s %s %s: %s", isWrite?"write":"read", circuit.c_str(), name.c_str(), getResultCode(result));
			return;
		}
		logOtherNotice("mqtt", "%s %s %s: %s", isWrite?"write":"read", circuit.c_str(), name.c_str(), data.c_str());
	}
	ostringstream ostream;
	publishMessage(message, ostream);
}

void MqttHandler::run() {
	time_t lastTaskRun, now, start, lastSignal = 0;
	bool signal = false;
	string signalTopic = m_globalTopic+"signal";
	string uptimeTopic = m_globalTopic+"uptime";
	ostringstream updates;

	time(&now);
	start = lastTaskRun = now;
	publishTopic(m_globalTopic+"version", PACKAGE_STRING "." REVISION);
	publishTopic(m_globalTopic+"running", "true");
	publishTopic(signalTopic, "false");
	mosquitto_message_callback_set(m_mosquitto, on_message);
	string subTopic = getTopic(NULL)+"#";
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
		if (!m_updatedMessages.empty()) {
			for (map<Message*, int>::iterator it = m_updatedMessages.begin(); it != m_updatedMessages.end(); it++) {
				Message* message = it->first;
				updates.str("");
				updates.clear();
				updates << dec;
				publishMessage(message, updates);
			}
			m_updatedMessages.clear();
		}
	}
}

void MqttHandler::handleTraffic() {
	if (m_mosquitto) {
		mosquitto_loop(m_mosquitto, -1
#if (LIBMOSQUITTO_MAJOR >= 1)
			, 1
#endif
			);
	}
}

string MqttHandler::getTopic(Message* message, signed char fieldIndex) {
	ostringstream ret;
	for (size_t i = 0; i < m_topicStrs.size(); i++) {
		ret << m_topicStrs[i];
		if (!message) {
			break;
		}
		if (i < m_topicCols.size()) {
			if (m_topicCols[i] == COLUMN_FIELDS && fieldIndex >= 0) {
				ret << message->getFieldName(fieldIndex);
			} else {
				message->dumpColumn(ret, m_topicCols[i]);
			}
		}
	}
	return ret.str();
}

void MqttHandler::publishMessage(Message* message, ostringstream& updates) {
	result_t result = message->decodeLastData(updates);
	if (result != RESULT_OK) {
		logOtherError("mqtt", "decode %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(), getResultCode(result));
		return;
	}
	if (m_publishByField) {
		signed char index = 0;
		istringstream input(updates.str());
		string token;
		while (getline(input, token, UI_FIELD_SEPARATOR)) {
			string topic = getTopic(message, index);
			publishTopic(topic, token);
			index++;
		}
	} else {
		publishTopic(getTopic(message), updates.str());
	}
}

void MqttHandler::publishTopic(string topic, string data, bool retain) {
	logOtherDebug("mqtt", "publish %s %s", topic.c_str(), data.c_str());
	mosquitto_publish(m_mosquitto, NULL, topic.c_str(), (uint32_t)data.size(), reinterpret_cast<const uint8_t*>(data.c_str()), 0, retain);
}

} // namespace ebusd

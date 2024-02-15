/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023-2024 John Baier <ebusd@ebusd.eu>
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

#include "ebusd/mqttclient.h"
#include "ebusd/mqttclient_mosquitto.h"
#include <csignal>
#include <deque>
#include <algorithm>
#include <utility>
#include "lib/utils/log.h"
#include "lib/ebus/symbol.h"

namespace ebusd {


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



#if (LIBMOSQUITTO_MAJOR >= 1)
int on_keypassword(char *buf, int size, int rwflag, void *userdata) {
  MqttClientMosquitto* client = reinterpret_cast<MqttClientMosquitto*>(userdata);
  if (!client || !client->m_config.keypass) {
    return 0;
  }
  int len = static_cast<int>(strlen(client->m_config.keypass));
  if (len > size) {
    len = size;
  }
  memcpy(buf, client->m_config.keypass, len);
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
    MqttClientMosquitto* client = reinterpret_cast<MqttClientMosquitto*>(obj);
    if (client) {
      client->m_listener->notifyMqttStatus(true);
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
  MqttClientMosquitto* client = reinterpret_cast<MqttClientMosquitto*>(obj);
  if (!client || !message) {
    return;
  }
  string topic(message->topic);
  string data(message->payloadlen > 0 ? reinterpret_cast<char*>(message->payload) : "");
  client->m_listener->notifyMqttTopic(topic, data);
}

MqttClientMosquitto::MqttClientMosquitto(mqtt_client_config_t config, MqttClientListener *listener)
  : MqttClient(config, listener),
    m_mosquitto(nullptr),
    m_initialConnectFailed(false),
    m_lastErrorLogTime(0) {
  int major = -1;
  int minor = -1;
  int revision = -1;
  mosquitto_lib_version(&major, &minor, &revision);
  if (major < LIBMOSQUITTO_MAJOR) {
    logOtherError("mqtt", "invalid mosquitto version %d instead of %d, will try connecting anyway", major,
      LIBMOSQUITTO_MAJOR);
  }
  logOtherInfo("mqtt", "mosquitto version %d.%d.%d (compiled with %d.%d.%d)", major, minor, revision,
    LIBMOSQUITTO_MAJOR, LIBMOSQUITTO_MINOR, LIBMOSQUITTO_REVISION);
  if (check(mosquitto_lib_init(), "unable to initialize")) {
    signal(SIGPIPE, SIG_IGN);  // needed before libmosquitto v. 1.1.3
#if (LIBMOSQUITTO_MAJOR >= 1)
    m_mosquitto = mosquitto_new(config.clientId, true, this);
#else
    m_mosquitto = mosquitto_new(config.clientId, this);
#endif
    if (!m_mosquitto) {
      logOtherError("mqtt", "unable to instantiate");
    }
  }
  if (m_mosquitto) {
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1004001)
    check(mosquitto_threaded_set(m_mosquitto, true), "threaded_set");
    int version = config.version311 ? MQTT_PROTOCOL_V311 : MQTT_PROTOCOL_V31;
    check(mosquitto_opts_set(m_mosquitto, MOSQ_OPT_PROTOCOL_VERSION, reinterpret_cast<void*>(&version)),
       "opts_set protocol version");
#else
    if (config.version311) {
      logOtherError("mqtt", "version 3.1.1 not supported");
    }
#endif
    if (config.username || config.password) {
      if (mosquitto_username_pw_set(m_mosquitto, config.username, config.password) != MOSQ_ERR_SUCCESS) {
        logOtherError("mqtt", "unable to set username/password, trying without");
      }
    }
    if (config.lastWillTopic) {
      size_t len = config.lastWillData ? strlen(config.lastWillData) : 0;
#if (LIBMOSQUITTO_MAJOR >= 1)
      mosquitto_will_set(m_mosquitto, config.lastWillTopic, (uint32_t)len,
        reinterpret_cast<const uint8_t*>(config.lastWillData), 0, true);
#else
      mosquitto_will_set(m_mosquitto, true, config.lastWillTopic, (uint32_t)len,
        reinterpret_cast<const uint8_t*>(config.lastWillData), 0, true);
#endif
    }

    if (config.cafile || config.capath) {
#if (LIBMOSQUITTO_MAJOR >= 1)
      mosquitto_user_data_set(m_mosquitto, this);
      int ret;
      ret = mosquitto_tls_set(m_mosquitto, config.cafile, config.capath, config.certfile, config.keyfile,
        on_keypassword);
      if (ret != MOSQ_ERR_SUCCESS) {
        logOtherError("mqtt", "unable to set TLS: %d", ret);
      } else if (config.insecure) {
        ret = mosquitto_tls_insecure_set(m_mosquitto, true);
        if (ret != MOSQ_ERR_SUCCESS) {
          logOtherError("mqtt", "unable to set TLS insecure: %d", ret);
        }
      }
#else
      logOtherError("mqtt", "use of TLS not supported");
#endif
    }
    if (config.logEvents) {
#if (LIBMOSQUITTO_VERSION_NUMBER >= 1003001)
      mosquitto_log_callback_set(m_mosquitto, on_log);
#else
      logOtherError("mqtt", "logging of library events not supported");
#endif
    }
    mosquitto_connect_callback_set(m_mosquitto, on_connect);
    // mosquitto_disconnect_callback_set(m_mosquitto, on_disconnect);
    mosquitto_message_callback_set(m_mosquitto, on_message);
  }
}

bool MqttClientMosquitto::connect(bool &isAsync, bool &connected) {
  isAsync = false;
  if (!m_mosquitto) {
    connected = false;
    return false;
  }
  int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
  ret = mosquitto_connect(m_mosquitto, m_config.host, m_config.port, 60);
#else
  ret = mosquitto_connect(m_mosquitto, config.host, config.port, 60, true);
#endif
  if (ret == MOSQ_ERR_INVAL && !m_config.ignoreInvalidParams) {
    logOtherError("mqtt", "unable to connect (invalid parameters)");
    mosquitto_destroy(m_mosquitto);
    m_mosquitto = nullptr;
    connected = false;
    return false;  // never try again
  }
  if (!check(ret, "unable to connect, retrying")) {
    connected = false;
    m_initialConnectFailed = m_config.ignoreInvalidParams;
    return true;
  }
  connected = true;  // assume success until connect_callback says otherwise
  logOtherDebug("mqtt", "connection requested");
  return true;
}

MqttClientMosquitto::~MqttClientMosquitto() {
  if (m_mosquitto) {
    mosquitto_destroy(m_mosquitto);
    m_mosquitto = nullptr;
  }
  mosquitto_lib_cleanup();
}

bool MqttClientMosquitto::run(bool allowReconnect, bool &connected) {
  if (!m_mosquitto) {
    return false;
  }
  int ret;
#if (LIBMOSQUITTO_MAJOR >= 1)
  ret = mosquitto_loop(m_mosquitto, -1, 1);  // waits up to 1 second for network traffic
#else
  ret = mosquitto_loop(m_mosquitto, -1);  // waits up to 1 second for network traffic
#endif
  if (!connected && (ret == MOSQ_ERR_NO_CONN || ret == MOSQ_ERR_CONN_LOST) && allowReconnect) {
    if (m_initialConnectFailed) {
#if (LIBMOSQUITTO_MAJOR >= 1)
      ret = mosquitto_connect(m_mosquitto, m_config.host, m_config.port, 60);
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
  if (!connected && ret == MOSQ_ERR_SUCCESS) {
    connected = true;
    logOtherNotice("mqtt", "connection re-established");
  }
  if (!connected || ret == MOSQ_ERR_SUCCESS) {
    return false;
  }
  if (ret == MOSQ_ERR_NO_CONN || ret == MOSQ_ERR_CONN_LOST || ret == MOSQ_ERR_CONN_REFUSED) {
    logOtherError("mqtt", "communication error: %s", ret == MOSQ_ERR_NO_CONN ? "not connected"
                  : (ret == MOSQ_ERR_CONN_LOST ? "connection lost" : "connection refused"));
    connected = false;
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

void MqttClientMosquitto::publishTopic(const string& topic, const string& data, int qos, bool retain) {
  const char* topicStr = topic.c_str();
  const char* dataStr = data.c_str();
  const size_t len = strlen(dataStr);
  logOtherDebug("mqtt", "publish %s %s", topicStr, dataStr);
  check(mosquitto_publish(m_mosquitto, nullptr, topicStr, (uint32_t)len,
      reinterpret_cast<const uint8_t*>(dataStr), qos, retain), "publish");
}

void MqttClientMosquitto::publishEmptyTopic(const string& topic, int qos, bool retain) {
  const char* topicStr = topic.c_str();
  logOtherDebug("mqtt", "publish empty %s", topicStr);
  check(mosquitto_publish(m_mosquitto, nullptr, topicStr, 0, nullptr, qos, retain), "publish empty");
}

void MqttClientMosquitto::subscribeTopic(const string& topic) {
  check(mosquitto_subscribe(m_mosquitto, nullptr, topic.c_str(), 0), "subscribe");
}

}  // namespace ebusd

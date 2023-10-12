/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_MQTTCLIENT_H_
#define EBUSD_MQTTCLIENT_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ebusd {

/** \file ebusd/mqttclient.h
 * An abstraction for an MQTT client.
 */

using std::map;
using std::pair;
using std::string;
using std::vector;

/** settings for the connection to an MQTT broker. */
typedef struct mqtt_client_config {
  const char* host;      //!< host name or IP address of MQTT broker
  uint16_t port;         //!< optional port of MQTT broker
  const char* clientId;  //!< optional clientid override for MQTT broker
  const char* username;  //!< optional user name for MQTT broker
  const char* password;  //!< optional password for MQTT broker
  bool logEvents;        //!< whether to log library events
  bool version311;       //!< true to use protocol version 3.1.1
  bool ignoreInvalidParams;  //!< ignore invalid parameters during init
  const char* cafile;    //!< optional CA file for TLS
  const char* capath;    //!< optional CA path for TLS
  const char* certfile;  //!< optional client certificate file for TLS
  const char* keyfile;   //!< optional client key file for TLS
  const char* keypass;   //!< optional client key file password for TLS
  bool insecure;         //!< whether to allow insecure TLS connection
  const char* lastWillTopic;  //!< optional last will topic.
  const char* lastWillData;   //!< optional last will data.
} mqtt_client_config_t;


/**
 * Interface for listening to MQTT client events.
 */
class MqttClientListener {
 public:
  /**
   * Destructor.
   */
  virtual ~MqttClientListener() {}

  /**
   * Notification of status of connection to the broker.
   */
  virtual void notifyMqttStatus(bool connected) = 0;  // abstract

  /**
   * Notification of a received MQTT message.
   * @param topic the topic string.
   * @param data the data string.
   */
  virtual void notifyMqttTopic(const string& topic, const string& data) = 0;  // abstract
};


/**
 * An abstract MQTT client.
 */
class MqttClient {
 public:
  /**
   * Constructor.
   * @param config the client configuration to use.
   * @param listener the client listener to use.
   */
  MqttClient(const mqtt_client_config_t config, MqttClientListener *listener)
  : m_config(config), m_listener(listener) {}

  /**
   * Destructor.
   */
  virtual ~MqttClient() {}

  /**
   * Create a new instance.
   * @param config the client configuration to use.
   * @param listener the client listener to use.
   * @return the new MqttClient, or @a nullptr on error.
   */
  static MqttClient* create(mqtt_client_config_t config, MqttClientListener *listener);

  /**
   * Connect to the broker and start handling MQTT traffic.
   * @param isAsync set to true if the asynchronous client was started and @a run() does not
   * have to be called at all, false if the client is synchronous and does
   * it's work in @a run() only.
   * @param connected set to true if the connection was already established.
   * @return true on success, false if connection failed and the client is no longer usable (i.e. should be destroyed).
   */
  virtual bool connect(bool &isAsync, bool &connected) = 0;  // abstract

  /**
   * Called regularly to handle MQTT traffic.
   * @param allowReconnect true when reconnecting to the broker is allowed.
   * @return true on error for waiting a bit until next call, or false otherwise.
   */
  virtual bool run(bool allowReconnect, bool &connected) = 0;  // abstract

  /**
   * Publish a topic update.
   * @param topic the topic string.
   * @param data the data string.
   * @param retain whether the topic shall be retained.
   */
  virtual void publishTopic(const string& topic, const string& data, int qos, bool retain = false) = 0;  // abstract

  /**
   * Publish a topic update without any data.
   * @param topic the topic string.
   */
  virtual void publishEmptyTopic(const string& topic, int qos, bool retain = false) = 0;  // abstract

  /**
   * Subscribe to the specified topic pattern.
   * @param topic the topic pattern string to subscribe to.
   */
  virtual void subscribeTopic(const string& topic) = 0;  // abstract

 public:
  /** the client configuration to use. */
  const mqtt_client_config_t m_config;

  /** the @a MqttClientListener instance. */
  MqttClientListener* m_listener;
};

}  // namespace ebusd

#endif  // EBUSD_MQTTCLIENT_H_

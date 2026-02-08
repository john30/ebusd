/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023-2026 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_MQTTCLIENT_MOSQUITTO_H_
#define EBUSD_MQTTCLIENT_MOSQUITTO_H_

#include "ebusd/mqttclient.h"
#include <mosquitto.h>
#include <ctime>
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

class MqttClientMosquitto : public MqttClient {
 public:
  /**
   * Constructor.
   * @param config the client configuration to use.
   * @param listener the client listener to use.
   */
  MqttClientMosquitto(mqtt_client_config_t config, MqttClientListener *listener);

  virtual ~MqttClientMosquitto();

  // @copydoc
  bool connect(bool &isAsync, bool &connected) override;

  // @copydoc
  bool run(bool allowReconnect, bool &connected) override;

  // @copydoc
  void publishTopic(const string& topic, const string& data, int qos, bool retain = false) override;

  // @copydoc
  void publishEmptyTopic(const string& topic, int qos, bool retain = false) override;

  // @copydoc
  void subscribeTopic(const string& topic) override;

 private:
  /** the mosquitto structure if initialized, or nullptr. */
  struct mosquitto* m_mosquitto;

  /** whether the initial connect failed. */
  bool m_initialConnectFailed;

  /** the last system time when a communication error was logged. */
  time_t m_lastErrorLogTime;
};

}  // namespace ebusd

#endif  // EBUSD_MQTTCLIENT_MOSQUITTO_H_

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

#ifndef EBUSD_MQTTHANDLER_H_
#define EBUSD_MQTTHANDLER_H_

#include <mosquitto.h>
#include <map>
#include <string>
#include <vector>
#include "ebusd/datahandler.h"
#include "ebusd/bushandler.h"
#include "lib/ebus/message.h"

namespace ebusd {

/** @file ebusd/mqtthandler.h
 * A data handler enabling MQTT support via mosquitto.
 */

using std::map;
using std::string;
using std::vector;

/**
 * Helper function for getting the argp definition for MQTT.
 * @return a pointer to the argp_child structure.
 */
const struct argp_child* mqtthandler_getargs();

/**
 * Registration function that is called once during initialization.
 * @param userInfo the @a UserInfo instance.
 * @param busHandler the @a BusHandler instance.
 * @param messages the @a MessageMap instance.
 * @return the create @a DataHandler, or NULL on error.
 */
DataHandler* mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages);

/**
 * The main class supporting MQTT data handling.
 */
class MqttHandler : public DataSink, public DataSource, public Thread {
 public:
  /**
   * Constructor.
   * @param userInfo the @a UserInfo instance.
   * @param busHandler the @a BusHandler instance.
   * @param messages the @a MessageMap instance.
   */
  MqttHandler(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages);

  /**
   * Destructor.
   */
  virtual ~MqttHandler();

  // @copydoc
  void start() override;

  /**
   * Notify the handler of a received MQTT message.
   * @param topic the topic string.
   * @param data the data string.
   */
  void notifyTopic(string topic, string data);

  // @copydoc
  void notifyUpdateCheckResult(string checkResult) override;

 protected:
  // @copydoc
  void run() override;


 private:
  /**
   * Called regularly to handle MQTT traffic.
   */
  void handleTraffic();

  /**
   * Build the MQTT topic string for the @a Message.
   * @param message the @a Message to build the topic string for.
   * @param fieldIndex the optional field index for the field column, or -1.
   * @return the topic string.
   */
  string getTopic(Message* message, ssize_t fieldIndex = -1);

  /**
   * Prepare a @a Message and publish as topic.
   * @param message the @a Message to publish.
   * @param updates the @a ostringstream for preparation.
   */
  void publishMessage(Message* message, ostringstream& updates);

  /**
   * Publish a topic update to MQTT.
   * @param topic the topic string.
   * @param data the data string.
   * @param retain whether the topic shall be retained.
   */
  void publishTopic(string topic, string data, bool retain = true);

  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the MQTT topic string parts. */
  vector<string> m_topicStrs;

  /** the MQTT topic field parts. */
  vector<string> m_topicFields;

  /** the global topic prefix. */
  string m_globalTopic;

  /** whether to publish a separate topic for each message field. */
  bool m_publishByField;

  /** the mosquitto structure if initialized, or NULL. */
  struct mosquitto* m_mosquitto;

  /** whether the connection to the broker is established. */
  bool m_connected;

  /** the last update check result. */
  string m_lastUpdateCheckResult;
};

}  // namespace ebusd

#endif  // EBUSD_MQTTHANDLER_H_

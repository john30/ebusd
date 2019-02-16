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

#ifndef EBUSD_MQTTHANDLER_H_
#define EBUSD_MQTTHANDLER_H_

#include <mosquitto.h>
#include <map>
#include <string>
#include <list>
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
 * @param handlers the @a list to which new @a DataHandler instances shall be added.
 * @return true if registration was successful.
 */
bool mqtthandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers);

/**
 * The main class supporting MQTT data handling.
 */
class MqttHandler : public DataSink, public DataSource, public WaitThread {
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
   * Notify the handler of a (re-)established connection to the broker.
   */
  void notifyConnected();

  /**
   * Notify the handler of a received MQTT message.
   * @param topic the topic string.
   * @param data the data string.
   */
  void notifyTopic(const string& topic, const string& data);

  // @copydoc
  void notifyUpdateCheckResult(const string& checkResult) override;

 protected:
  // @copydoc
  void run() override;


 private:
  /**
   * Called regularly to handle MQTT traffic.
   * @param allowReconnect true when reconnecting to the broker is allowed.
   * @return true on error for waiting a bit until next call, or false otherwise.
   */
  bool handleTraffic(bool allowReconnect);

  /**
   * Build the MQTT topic string for the @a Message.
   * @param message the @a Message to build the topic string for.
   * @param suffix the optional suffix string to append.
   * @param fieldName the name of the singular field, or empty.
   * @return the topic string.
   */
  string getTopic(const Message* message, const string& suffix = "", const string& fieldName = "");

  /**
   * Prepare a @a Message and publish as topic.
   * @param message the @a Message to publish.
   * @param updates the @a ostringstream for preparation.
   */
  void publishMessage(const Message* message, ostringstream* updates);

  /**
   * Publish a topic update to MQTT.
   * @param topic the topic string.
   * @param data the data string.
   * @param retain whether the topic shall be retained.
   */
  void publishTopic(const string& topic, const string& data, bool retain = false);

  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the global topic prefix. */
  string m_globalTopic;

  /** the topic to subscribe to. */
  string m_subscribeTopic;

  /** whether to publish a separate topic for each message field. */
  bool m_publishByField;

  /** the mosquitto structure if initialized, or nullptr. */
  struct mosquitto* m_mosquitto;

  /** whether the connection to the broker is established. */
  bool m_connected;

  /** whether the initial connect failed. */
  bool m_initialConnectFailed;

  /** the last update check result. */
  string m_lastUpdateCheckResult;

  /** the last system time when a communication error was logged. */
  time_t m_lastErrorLogTime;
};

}  // namespace ebusd

#endif  // EBUSD_MQTTHANDLER_H_

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
 * Helper class for replacing a template string with real values.
 */
class MqttReplacer {
 public:
  /**
   * Normalize the string to contain only alpha numeric characters plus underscore by replacing other characters with
   * an underscore.
   * @param str the string to normalize.
   */
  static void normalize(string& str);

  /**
   * Parse the template string.
   * @param templateStr the template string.
   * @param onlyKnown true to allow only known field names from @a knownFieldNames.
   * @param noKnownDuplicates true to now allow duplicates from @a knownFieldNames.
   * @param emptyIfMissing true when the complete result is supposed to be empty when at least one referenced variable
   * is empty or not defined.
   * @return true on success, false on malformed template string.
   */
  bool parse(const string& templateStr, bool onlyKnown = true, bool noKnownDuplicates = true, bool emptyIfMissing = false);

  /**
   * Ensure the default topic parts are present (circuit and message).
   */
  void ensureDefault();

  /**
   * Return whether this replacer is completely empty.
   * @return true when empty.
   */
  bool empty() const;

  /**
   * Return whether the specified field is used.
   * @param field the field name to check.
   * @return true when the specified field is used.
   */
  bool has(const string& field) const;

  /**
   * Get the replaced template string.
   * @param values the named values for replacement.
   * @param untilFirstEmpty true to only return the prefix before the first empty field.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @return the replaced template string.
   */
  string get(const map<string, string>& values, bool untilFirstEmpty = true, bool onlyAlphanum = false) const;

  /**
   * Check if the fields can be reduced to a constant value.
   * @param values the named values for replacement.
   * @return true if the result is final.
   */
  bool isReducable(const map<string, string>& values) const;

  /**
   * Reduce the fields to a constant value if possible.
   * @param values the named values for replacement.
   * @param result the string to store the result in.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @return true if the result is final.
   */
  bool reduce(const map<string, string>& values, string& result, bool onlyAlphanum = false) const;

  /**
   * Match a topic string against the constant and variables parts.
   * @param topic the topic string to match.
   * @param circuit pointer to the string receiving the circuit name if present.
   * @param name pointer to the string receiving the message name if present.
   * @param field pointer to the string receiving the field name if present.
   * @return the index of the last unmatched part, or the negative index minus one for extra non-matched non-field parts.
   */
  ssize_t matchTopic(const string& topic, string* circuit, string* name, string* field) const;

 private:
  /**
   * the list of parts the template is composed of.
   * the string is either the plain string or the name of the field.
   * the number is negative for plain strings, the index to @a knownFieldNames for a known field, or the size of
   * @a knownFieldNames for an unknown field.
   */
  vector<std::pair<string, int>> m_parts;

  /** true when the complete result is supposed to be empty when at least one referenced variable
   * is empty or not defined. */
  bool m_emptyIfMissing;
};


/**
 * A set of constants and @a MqttReplacer variables.
 */
class MqttReplacers {
 public:
  /**
   * Get the value of the specified key from the constants only.
   * @param key the key for which to get the value.
   * @return the value string or empty.
   */
  const string& operator[](const string& key) const;

  /**
   * Check if the specified field is used by one of the replacers.
   * @param field the name of the field to check.
   * @return true if the specified field is used by one of the replacers.
   */
  bool uses(const string& field) const;

  /**
   * Get the variable value of the specified key.
   * @param key the key for which to get the value.
   * @return the value @a MqttReplacer.
   */
  MqttReplacer& get(const string& key);

  /**
   * Get the variable or constant value of the specified key.
   * @param key the key for which to get the value.
   * @param untilFirstEmpty true to only return the prefix before the first empty field.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @param fallbackKey optional fallback key to use when key value is undefined.
   * @return the value string or empty.
   */
  string get(const string& key, bool untilFirstEmpty, bool onlyAlphanum = false, const string& fallbackKey = "") const;

  /**
   * Set the constant value of the specified key and additionally normalized with uppercase key only (if the key does
   * not contain an underscore).
   * @param key the key to store.
   * @param value the value string.
   * @param removeReplacer true to remove a replacer with the same name.
   * @return true when an upper case key was stored/updates as well.
   */
  bool set(const string& key, const string& value, bool removeReplacer = true);

  /**
   * Set the constant value of the specified key.
   * @param key the key to store.
   * @param value the numeric value (converted to a string).
   */
  void set(const string& key, int value);

  /**
   * Reduce as many variables to constants as possible.
   */
  void reduce();

 private:
  /** constant values from the integration file. */
  map<string, string> m_constants;

  /** variable values from the integration file. */
  map<string, MqttReplacer> m_replacers;
};


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

 private:
  void parseIntegration(const string& line);

 public:
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

  // @copydoc
  void notifyScanStatus(const string& scanStatus) override;

 protected:
  // @copydoc
  void run() override;


 private:
  /**
   * Publish a definition topic as specified in the given values.
   * @param values the values with the message specification.
   * @param prefix the prefix for picking the message specification from the values (before "topic", "payload", and
   * "retain").
   * @param topic optional data topic (not the definition topic) to set before building the topic/payload, or empty.
   * @param circuit optional circuit to set before building the topic/payload, or empty.
   * @param name optional name to set before building the topic/payload, or empty.
   * @param fallbackPrefix optional fallback prefix to use when topic/payload/retain with prefix above is not defined.
   */
  void publishDefinition(MqttReplacers values, const string& prefix = "definition-", const string& topic = "",
                         const string& circuit = "", const string& name = "", const string& fallbackPrefix = "");

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
   * @param includeWithoutData whether to publish messages without data as well.
   */
  void publishMessage(const Message* message, ostringstream* updates, bool includeWithoutData = false);

  /**
   * Publish a topic update to MQTT.
   * @param topic the topic string.
   * @param data the data string.
   * @param retain whether the topic shall be retained.
   */
  void publishTopic(const string& topic, const string& data, bool retain = false);

  /**
   * Publish a topic update to MQTT without any data.
   * @param topic the topic string.
   */
  void publishEmptyTopic(const string& topic);

  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the global topic prefix. */
  string m_globalTopic;

  /** the topic to subscribe to. */
  string m_subscribeTopic;

  /** whether to publish a separate topic for each message field. */
  bool m_publishByField;

  /** the @a MqttReplacers from the integration file. */
  MqttReplacers m_replacers;

  /** whether the @a m_replacers includes the definition_topic. */
  bool m_hasDefinitionTopic;

  /** whether the @a m_replacers uses the fields_payload variable. */
  bool m_hasDefinitionFieldsPayload;

  /** map of type name to a list of pairs of wildcard string and mapped value. */
  map<string, vector<std::pair<string, string>>> m_typeSwitches;

  /** the subscribed configuration restart topic, or empty. */
  string m_subscribeConfigRestartTopic;

  /** the expected payload of the subscribed configuration restart topic, or empty for any. */
  string m_subscribeConfigRestartPayload;

  /** the last system time when the message definitions were published. */
  time_t m_definitionsSince;

  /** the mosquitto structure if initialized, or nullptr. */
  struct mosquitto* m_mosquitto;

  /** whether the connection to the broker is established. */
  bool m_connected;

  /** whether the initial connect failed. */
  bool m_initialConnectFailed;

  /** the last update check result. */
  string m_lastUpdateCheckResult;

  /** the last scan status. */
  string m_lastScanStatus;

  /** the last system time when a communication error was logged. */
  time_t m_lastErrorLogTime;
};

}  // namespace ebusd

#endif  // EBUSD_MQTTHANDLER_H_

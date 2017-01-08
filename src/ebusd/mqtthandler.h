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

#ifndef MQTTHANDLER_H_
#define MQTTHANDLER_H_

#include "datahandler.h"
#include "bushandler.h"
#include "message.h"
#include <mosquitto.h>

/** @file mqtthandler.h
 * A data handler enabling MQTT support via mosquitto.
 */

using namespace std;

/**
 * Helper function for getting the argp definition for MQTT.
 * @return a pointer to the argp_child structure.
 */
const struct argp_child* mqtthandler_getargs();

/**
 * Registration function that is called once during initialization.
 * @param busHandler the @a BusHandler instance.
 * @return the create @a DataHandler, or NULL on error.
 */
DataHandler* mqtthandler_register(BusHandler* busHandler);

/**
 * The main class supporting MQTT data handling.
 */
class MqttHandler : public DataSink, DataSource, Thread
{
public:

	/**
	 * Constructor.
	 * @param busHandler the @a BusHandler instance.
	 */
	MqttHandler(BusHandler* busHandler);

	/**
	 * Destructor.
	 */
	virtual ~MqttHandler();

	// @copydoc
	virtual void start();

protected:

	// @copydoc
	virtual void run();

private:

	/**
	 * Called regularly to handle MQTT traffic.
	 */
	void handleTraffic();

	/**
	 * Build the MQTT topic string for the @a Message.
	 * @param message the @a Message to build the topic string for.
	 * @return the topic string.
	 */
	string getTopic(Message* message);

	/**
	 * Publish a topic update to MQTT.
	 * @param topic the topic string.
	 * @param data the data string.
	 * @param retain whether the topic shall be retained.
	 */
	void publishTopic(string topic, string data, bool retain=true);

	/** the MQTT topic string parts. */
	vector<string> m_topicStrs;

	/** the MQTT topic column parts. */
	vector<size_t> m_topicCols;

	/** the global topic prefix. */
	string m_globalTopic;

	/** whether to publish a separate topic for each message field. */
	bool m_publishByField;

	/** the mosquitto structure if initialized, or NULL. */
	struct mosquitto* m_mosquitto;

};

#endif // DATAHANDLER_H_

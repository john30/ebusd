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

#ifndef EBUSD_DATAHANDLER_H_
#define EBUSD_DATAHANDLER_H_

#include <argp.h>
#include <map>
#include <list>
#include "bushandler.h"
#include "message.h"

/** @file datahandler.h
 * Classes and functions for implementing and registering generic data sinks
 * and sources that allow listening to received data updates and sending on
 * the bus.
 */

using namespace std;

class DataHandler;

/**
 * Helper function for getting the argp definition for all known @a DataHandler instances.
 * @return a pointer to the argp_child structure, or NULL.
 */
const struct argp_child* datahandler_getargs();

/**
 * Registration function that is called once during initialization.
 * @param busHandler the @a BusHandler instance.
 * @param messages the @a MessageMap instance.
 * @param handlers the @a list to which new @a DataHandler instances shall be added.
 * @return true if registration was successful.
 */
bool datahandler_register(BusHandler* busHandler, MessageMap* messages, list<DataHandler*>& handlers);


/**
 * Base class for all kinds of data handlers.
 */
class DataHandler
{
public:
	/**
	 * Constructor.
	 */
	DataHandler() {}

	/**
	 * Destructor.
	 */
	virtual ~DataHandler() {}

	/**
	 * Called to start the @a DataHandler.
	 */
	virtual void start() = 0;

	/**
	 * Return whether this is a @a DataSink instance.
	 * @return whether this is a @a DataSink instance.
	 */
	virtual bool isDataSink() { return false; }

	/**
	 * Return whether this is a @a DataSource instance.
	 * @return whether this is a @a DataSource instance.
	 */
	virtual bool isDataSource() { return false; }

};


/**
 * Base class for listening to data updates.
 */
class DataSink : virtual public DataHandler
{
public:

	/**
	 * Constructor.
	 */
	DataSink() {}

	/**
	 * Destructor.
	 */
	virtual ~DataSink() {}

	/**
	 * Notify the sink of an updated @a Message.
	 * @param message the updated @a Message.
	 */
	virtual void notifyUpdate(Message* message);

	// @copydoc
	virtual bool isDataSink() { return true; }

protected:

	/** a map of updated @p Message instances. */
	map<Message*, int> m_updatedMessages;

};


/**
 * Base class providing data to be sent on the bus.
 */
class DataSource : virtual public DataHandler
{
public:

	/**
	 * Constructor.
	 * @param busHandler the @a BusHandler instance.
	 */
	DataSource(BusHandler* busHandler)
		: m_busHandler(busHandler) {}

	/**
	 * Destructor.
	 */
	virtual ~DataSource() {}

	// @copydoc
	virtual bool isDataSource() { return true; }

protected:

	/** the @a BusHandler instance. */
	BusHandler* m_busHandler;

};

#endif // EBUSD_DATAHANDLER_H_

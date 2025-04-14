/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2025 John Baier <ebusd@ebusd.eu>
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

#include <map>
#include <list>
#include <string>
#include "ebusd/bushandler.h"
#include "lib/ebus/message.h"
#include "lib/utils/arg.h"

namespace ebusd {

/** @file ebusd/datahandler.h
 * Classes and functions for implementing and registering generic data sinks
 * and sources that allow listening to received data updates and sending on
 * the bus.
 */

using std::list;
using std::map;

class UserInfo;
class DataHandler;

/** type for scan status. */
enum scanStatus_t {
  SCAN_STATUS_NONE = 0,      //!< no scan status (never started before)
  SCAN_STATUS_RUNNING = 1,   //!< scan is currently running
  SCAN_STATUS_FINISHED = 2,  //!< scan is finished
};


/**
 * Helper function for getting the arg definition for all known @a DataHandler instances.
 * @return a pointer to the child argument options, or nullptr.
 */
const argParseChildOpt* datahandler_getargs();

/**
 * Registration function that is called once during initialization.
 * @param userInfo the @a UserInfo instance.
 * @param busHandler the @a BusHandler instance.
 * @param messages the @a MessageMap instance.
 * @param handlers the @a list to which new @a DataHandler instances shall be added.
 * @return true if registration was successful.
 */
bool datahandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers);


/**
 * Helper interface for user authentication.
 */
class UserInfo {
 public:
  /**
   * Destructor.
   */
  virtual ~UserInfo() {}

  /**
   * Check whether the specified user exists.
   * @param user the user name.
   * @return whether the user exists.
   */
  virtual bool hasUser(const string& user) const = 0;  // abstract

  /**
   * Check whether the secret string matches the one of the specified user.
   * @param user the user name.
   * @param secret the secret to check.
   * @return whether the secret string is valid.
   */
  virtual bool checkSecret(const string& user, const string& secret) const = 0;  // abstract

  /**
   * Get the access levels associated with the specified user.
   * @param user the user name, or empty for default levels.
   * @return the access levels separated by semicolon.
   */
  virtual string getLevels(const string& user) const = 0;  // abstract
};


/**
 * Base class for all kinds of data handlers.
 */
class DataHandler {
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
  virtual void startHandler() = 0;

  /**
   * Return whether this is a @a DataSink instance.
   * @return whether this is a @a DataSink instance.
   */
  virtual bool isDataSink() const { return false; }

  /**
   * Return whether this is a @a DataSource instance.
   * @return whether this is a @a DataSource instance.
   */
  virtual bool isDataSource() const { return false; }
};


/**
 * Base class for listening to data updates.
 */
class DataSink : virtual public DataHandler {
 public:
  /**
   * Constructor.
   * @param userInfo the @a UserInfo instance.
   * @param user the user name for determining the allowed access levels (fall back to default levels).
   * @param changedOnly whether to handle changed messages only in the updates.
   */
  DataSink(const UserInfo* userInfo, const string& user, bool changedOnly) : m_changedOnly(changedOnly) {
    m_levels = userInfo->getLevels(userInfo->hasUser(user) ? user : "");
  }

  /**
   * Destructor.
   */
  virtual ~DataSink() {}

  // @copydoc
  bool isDataSink() const override { return true; }

  /**
   * Notify the sink of an updated @a Message (not necessarily changed though).
   * @param message the updated @a Message.
   * @param changed whether the message data changed since the last notification.
   */
  virtual void notifyUpdate(Message* message, bool changed);

  /**
   * Notify the sink of the latest update check result.
   * @param checkResult a string describing available updates, or empty if no update is available.
   */
  virtual void notifyUpdateCheckResult(const string& checkResult) {}

  /**
   * Notify the sink of the latest scan status.
   * @param scanStatus the scan status.
   */
  virtual void notifyScanStatus(scanStatus_t scanStatus) {}

 protected:
  /** the allowed access levels. */
  string m_levels;

  /** whether to handle changed messages only in the updates. */
  bool m_changedOnly;

  /** a map of updated @p Message keys. */
  map<uint64_t, int> m_updatedMessages;
};


/**
 * Base class providing data to be sent on the bus.
 */
class DataSource : virtual public DataHandler {
 public:
  /**
   * Constructor.
   * @param busHandler the @a BusHandler instance.
   */
  explicit DataSource(BusHandler* busHandler)
    : m_busHandler(busHandler) {}

  /**
   * Destructor.
   */
  virtual ~DataSource() {}

  // @copydoc
  bool isDataSource() const override { return true; }


 protected:
  /** the @a BusHandler instance. */
  BusHandler* m_busHandler;
};

}  // namespace ebusd

#endif  // EBUSD_DATAHANDLER_H_

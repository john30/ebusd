/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023-2025 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_REQUEST_H_
#define EBUSD_REQUEST_H_

#include <string>
#include <cstdio>
#include <list>
#include <vector>
#include "lib/ebus/datatype.h"
#include "lib/utils/queue.h"
#include "lib/utils/notify.h"
#include "lib/utils/thread.h"
#include "lib/utils/log.h"

namespace ebusd {

/** \file ebusd/request.h
 * Abstraction of ebusd client requests.
 */

/** the request listen mode. */
enum ListenMode {
  lm_none,    //!< normal mode (no listening)
  lm_listen,  //!< listening mode
  lm_direct,  //!< direct mode
};

/**
 * Request mode info.
 */
struct RequestMode {
  ListenMode listenMode;   //!< whether in listening or direct mode
  OutputFormat format;     //!< the output format settings for listen mode
  bool listenWithUnknown;  //!< include unknown messages in listen/direct mode
  bool listenOnlyUnknown;  //!< only print unknown messages in listen/direct mode
};

/**
 * Abstract class for request/response.
 */
class Request {
 public:
  /**
   * Destructor.
   */
  virtual ~Request() { }

  /**
   * Add request data from the client.
   * @param request the request data from the client.
   * @return true when the request is complete and the response shall be prepared.
   */
  virtual bool add(const char* request) = 0;

  /**
   * @return whether the request is still empty.
   */
  virtual bool empty() const = 0;

  /**
   * Split the request into arguments.
   * @param args the @a vector to push the arguments to.
   */
  virtual void split(vector<string>* args) = 0;

  /**
   * Return whether this is a HTTP request.
   * @return whether this is a HTTP request.
   */
  virtual bool isHttp() const = 0;

  /**
   * Log the request or the given response in debug level.
   */
  virtual void log(const string* response = nullptr) const = 0;

  /**
   * Return the current user name.
   * @return the current user name.
   */
  virtual const string& getUser() const = 0;

  /**
   * Wait for the response being set and return the result string.
   * @param result the variable in which to store the result string.
   * @return true when the client shall be disconnected.
   */
  virtual bool waitResponse(string* result) = 0;

  /**
   * Set the result string and notify a waiting thread.
   * @param result the result string.
   * @param user the new user name.
   * @param newMode the new @a RequestMode.
   * @param listenUntil the end time to which to updates were added (exclusive).
   * @param disconnect true when the client shall be disconnected.
   */
  virtual void setResult(const string& result, const string& user, RequestMode* newMode, time_t listenUntil,
      bool disconnect) = 0;

  /**
   * Return the @a RequestMode.
   * @param listenSince set listening to the specified start time from which to add updates (inclusive).
   * @return the @a RequestMode.
   */
  virtual RequestMode getMode(time_t* listenSince = nullptr) = 0;
};

/**
 * Default @a Request implementation.
 */
class RequestImpl : public Request {
 public:
  /**
   * Constructor.
   * @param isHttp whether this is a HTTP request.
   */
  explicit RequestImpl(bool isHttp);

  /**
   * Destructor.
   */
  virtual ~RequestImpl();


 private:
  /**
   * Hidden copy constructor.
   * @param src the object to copy from.
   */
  RequestImpl(const RequestImpl& src);


 public:
  // @copydoc
  bool add(const char* request) override;

  // @copydoc
  bool empty() const override { return m_request.empty(); }

  // @copydoc
  void split(vector<string>* args) override;

  // @copydoc
  bool isHttp() const override { return m_isHttp; }

  // @copydoc
  void log(const string* response = nullptr) const override {
    if (response) {
      if (response->length() > 100) {
        logDebug(lf_main, "<<< %s ...", response->substr(0, 100).c_str());
      } else {
        logDebug(lf_main, "<<< %s", response->c_str());
      }
    } else {
      logDebug(lf_main, ">>> %s", m_request.c_str());
    }
  }

  // @copydoc
  const string& getUser() const override { return m_user; }

  // @copydoc
  bool waitResponse(string* result) override;

  // @copydoc
  void setResult(const string& result, const string& user, RequestMode* mode, time_t listenUntil,
      bool disconnect) override;

  // @copydoc
  RequestMode getMode(time_t* listenSince = nullptr) override {
    if (listenSince) {
      *listenSince = m_listenSince;
    }
    return m_mode;
  }


 private:
  /** whether this is a HTTP message. */
  const bool m_isHttp;

  /** the request string. */
  string m_request;

  /** the current user name. */
  string m_user;

  /** whether the result was already set. */
  bool m_resultSet;

  /** the result string. */
  string m_result;

  /** set to true when the client shall be disconnected. */
  bool m_disconnect;

  /** mutex variable for exclusive lock. */
  pthread_mutex_t m_mutex;

  /** condition variable for exclusive lock. */
  pthread_cond_t m_cond;

  /** the @a RequestMode. */
  RequestMode m_mode;

  /** start timestamp of listening update. */
  time_t m_listenSince;
};

}  // namespace ebusd

#endif  // EBUSD_REQUEST_H_

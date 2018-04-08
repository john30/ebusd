/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2018 John Baier <ebusd@ebusd.eu>
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

#ifndef EBUSD_MAINLOOP_H_
#define EBUSD_MAINLOOP_H_

#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include "ebusd/bushandler.h"
#include "ebusd/datahandler.h"
#include "ebusd/network.h"
#include "lib/ebus/filereader.h"
#include "lib/ebus/message.h"
#include "lib/utils/rotatefile.h"

namespace ebusd {

/** \file ebusd/mainloop.h
 * The main loop for the TCP client interface and regular tasks such as resolving scanned data.
 */

/**
 * Helper class for user authentication.
 */
class UserList : public UserInfo, public MappedFileReader {
 public:
  /**
   * Constructor.
   * @param defaultLevels the default access levels.
   */
  explicit UserList(const string& defaultLevels) : MappedFileReader::MappedFileReader(false) {
    if (!defaultLevels.empty()) {
      string levels = defaultLevels;
      transform(levels.begin(), levels.end(), levels.begin(), [](unsigned char c) {
        return c == ',' ? VALUE_SEPARATOR : c;
      });
      m_userLevels[""] = levels;
    }
  }

  /**
   * Destructor.
   */
  virtual ~UserList() {}

  // @copydoc
  result_t getFieldMap(const string& preferLanguage, vector<string>* row, string* errorDescription) const override;

  // @copydoc
  result_t addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
      vector< map<string, string> >* subRows, string* errorDescription, bool replace) override;

  // @copydoc
  bool hasUser(const string& user) const override {
    return m_userLevels.find(user) != m_userLevels.end();
  }

  // @copydoc
  bool checkSecret(const string& user, const string& secret) const override {
    auto it = m_userSecrets.find(user);
    return it != m_userSecrets.end() && it->second == secret;
  }

  // @copydoc
  string getLevels(const string& user) const override {
    auto it = m_userLevels.find(user);
    return it == m_userLevels.end() ? "" : it->second;
  }

 private:
  /** the secret string by user name. */
  map<string, string> m_userSecrets;

  /** the access levels by user name (separated by semicolon, empty name for default levels). */
  map<string, string> m_userLevels;
};


/**
 * The main loop handling requests from connected clients.
 */
class MainLoop : public Thread, DeviceListener {
 public:
  /**
   * Construct the main loop and create network and bus handling components.
   * @param opt the program options.
   * @param device the @a Device instance.
   * @param messages the @a MessageMap instance.
   */
  MainLoop(const struct options& opt, Device *device, MessageMap* messages);

  /**
   * Destructor.
   */
  ~MainLoop();

  /**
   * Shutdown the main loop.
   */
  void shutdown() { m_shutdown = true; }

  /**
   * Get the @a BusHandler instance.
   * @return the created @a BusHandler instance.
   */
  BusHandler* getBusHandler() { return m_busHandler; }

  /**
   * Add a client @a NetMessage to the queue.
   * @param message the client @a NetMessage to handle.
   */
  void addMessage(NetMessage* message) { m_netQueue.push(message); }

  // @copydoc
  void notifyDeviceData(symbol_t symbol, bool received) override;


 protected:
  // @copydoc
  void run() override;


 private:
  /**
   * Decode and execute client message.
   * @param data the data string to decode (may be empty).
   * @param connected set to false when the client connection shall be closed.
   * @param isHttp true for HTTP message.
   * @param listening set to true when the client is in listening mode.
   * @param user set to the new user name when changed by authentication.
   * @param reload set to true when the configuration files were reloaded.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t decodeMessage(const string& data, bool isHttp, bool* connected, bool* listening,
      string* user, bool* reload, ostringstream* ostream);

  /**
   * Parse the hex master message from the remaining arguments.
   * @param args the arguments passed to the command.
   * @param argPos the index of the first argument to parse.
   * @param srcAddress the source address to set, or @a SYN for the own master address.
   * @param master the @a MasterSymbolString to write the data to.
   * @return the result from parsing the arguments.
   */
  result_t parseHexMaster(const vector<string>& args, size_t argPos, symbol_t srcAddress,
      MasterSymbolString* master);

  /**
   * Get the access levels associated with the specified user name.
   * @param user the user name, or empty for default levels.
   * @return the access levels separated by semicolon.
   */
  string getUserLevels(const string& user) { return m_userList.getLevels(user); }

  /**
   * Execute the auth command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param user the current user name to set to the new user name on success.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeAuth(const vector<string>& args, string* user, ostringstream* ostream);

  /**
   * Execute the read command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param levels the current user's access levels.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeRead(const vector<string>& args, const string& levels, ostringstream* ostream);

  /**
   * Execute the write command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param levels the current user's access levels.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeWrite(const vector<string>& args, const string levels, ostringstream* ostream);

  /**
   * Execute the hex command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeHex(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the find command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param levels the current user's access levels.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeFind(const vector<string>& args, const string& levels, ostringstream* ostream);

  /**
   * Execute the listen command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param listening set to true when the client is in listening mode.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeListen(const vector<string>& args, bool* listening, ostringstream* ostream);

  /**
   * Execute the state command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeState(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the grab command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeGrab(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the define command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeDefine(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the decode command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  //result_t executeDecode(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the encode command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  //result_t executeEncode(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the scan command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param levels the current user's access levels.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeScan(const vector<string>& args, const string& levels, ostringstream* ostream);

  /**
   * Execute the log command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeLog(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the raw command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeRaw(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the dump command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeDump(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the reload command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeReload(const vector<string>& args, ostringstream* ostream);

  /**
   * Execute the info command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param user the current user name.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeInfo(const vector<string>& args, const string& user, ostringstream* ostream);

  /**
   * Execute the quit command.
   * @param args the arguments passed to the command (starting with the command itself), or empty for help.
   * @param connected set to false when the client connection shall be closed.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeQuit(const vector<string>& args, bool *connected, ostringstream* ostream);

  /**
   * Execute the help command.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeHelp(ostringstream* ostream);

  /**
   * Execute the HTTP GET command.
   * @param args the arguments passed to the command (starting with the command itself).
   * @param connected set to false when the client connection shall be closed.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t executeGet(const vector<string>& args, bool* connected, ostringstream* ostream);

  /**
   * Format the HTTP answer to the result string.
   * @param ret the result code of handling the request.
   * @param type the content type.
   * @param ostream the @a ostringstream to format the result string to.
   * @return the result code.
   */
  result_t formatHttpResult(result_t ret, int type, ostringstream* ostream);

  /** the @a Device instance. */
  Device* m_device;

  /** the number of reconnects requested from the @a Device. */
  unsigned int m_reconnectCount;

  /** the @a RotateFile for writing sent/received bytes in log format, or NULL. */
  RotateFile* m_logRawFile;

  /** whether raw logging to @p logNotice is enabled (only relevant if m_logRawFile is NULL). */
  bool m_logRawEnabled;

  /** whether to log raw bytes instead of messages with @a m_logRawEnabled. */
  bool m_logRawBytes;

  /** the buffer for building log raw message. */
  ostringstream m_logRawBuffer;

  /** true when the last byte in @a m_logRawBuffer was receive, false if it was sent. */
  bool m_logRawLastReceived;

  /** the last sent/received symbol.*/
  symbol_t m_logRawLastSymbol;

  /** the @a RotateFile for dumping received data, or NULL. */
  RotateFile* m_dumpFile;

  /** the @a UserList instance. */
  UserList m_userList;

  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the own master address for sending on the bus. */
  const symbol_t m_address;

  /** whether to pick configuration files matching initial scan. */
  const bool m_scanConfig;

  /** the initial address to scan for @a m_scanConfig
   * (@a ESC=none, 0xfe=broadcast ident, @a SYN=full scan, else: single slave address). */
  const symbol_t m_initialScan;

  /** true when the poll interval is non zero. */
  const bool m_polling;

  /** whether to enable the hex command. */
  const bool m_enableHex;

  /** the MessageMap for handling newly defined messages for testing (if enabled), or NULL. */
  MessageMap* m_newlyDefinedMessages;

  /** set to true to shutdown. */
  bool m_shutdown;

  /** perform automatic update check. */
  bool m_runUpdateCheck;

  /** the created @a BusHandler instance. */
  BusHandler* m_busHandler;

  /** the created @a Network instance. */
  Network* m_network;

  /** the @a NetMessage @a Queue. */
  Queue<NetMessage*> m_netQueue;

  /** the path for HTML files served by the HTTP port. */
  string m_htmlPath;

  /** the registered @a DataHandler instances. */
  list<DataHandler*> m_dataHandlers;

  /** the result of the last update check, or empty. */
  string m_updateCheck;
};

}  // namespace ebusd

#endif  // EBUSD_MAINLOOP_H_

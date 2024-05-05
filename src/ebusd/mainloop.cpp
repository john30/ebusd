/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ebusd/mainloop.h"
#include <iomanip>
#include <deque>
#include <algorithm>
#include "ebusd/main.h"
#include "ebusd/scan.h"
#include "lib/utils/log.h"
#include "lib/ebus/data.h"

namespace ebusd {

using std::dec;
using std::setw;
using std::endl;
using std::ifstream;

/** the number of seconds of permanent missing signal after which to reconnect the device. */
#define RECONNECT_MISSING_SIGNAL 60


result_t UserList::getFieldMap(const string& preferLanguage, vector<string>* row, string* errorDescription) const {
  // name,secret,level[,level]*
  if (row->empty()) {
    row->push_back("name");
    row->push_back("secret");
    row->push_back("*level");
    return RESULT_OK;
  }
  map<string, string> seen;
  for (auto& name : *row) {
    tolower(&name);
    if (name == "name" || name == "secret") {
      if (seen.find(name) != seen.end()) {
        *errorDescription = "duplicate field " + name;
        return RESULT_ERR_INVALID_ARG;
      }
    } else if (name == "level") {
      name = "*level";
    } else {
      *errorDescription = "unknown field " + name;
      return RESULT_ERR_INVALID_ARG;
    }
    seen[name] = name;
  }
  if (seen.find("name") == seen.end() || seen.find("secret") == seen.end()) {
    return RESULT_ERR_EOF;  // require at least name and secret
  }
  return RESULT_OK;
}

result_t UserList::addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
    vector< map<string, string> >* subRows, string* errorDescription, bool replace) {
  string name = (*row)["name"];
  string secret = (*row)["secret"];
  if (name.empty()) {
    return RESULT_ERR_INVALID_ARG;
  }
  if (name == "*") {
    name = "";  // default levels
  }
  string levels;
  for (const auto& entry : *subRows) {
    const auto it = entry.find("level");
    if (it != entry.end() && !it->second.empty()) {
      if (!levels.empty()) {
        levels += VALUE_SEPARATOR;
      }
      levels += it->second;
    }
  }
  m_userSecrets[name] = secret;
  m_userLevels[name] = levels;
  return RESULT_OK;
}


#define VERBOSITY_0 OF_NONE
#define VERBOSITY_1 OF_NAMES
#define VERBOSITY_2 (VERBOSITY_1 | OF_UNITS)
#define VERBOSITY_3 (VERBOSITY_2 | OF_COMMENTS)
#define VERBOSITY_4 (VERBOSITY_3 | OF_ALL_ATTRS)


MainLoop::MainLoop(const struct options& opt, BusHandler* busHandler,
    MessageMap* messages, ScanHelper* scanHelper, Queue<Request*>* requestQueue)
  : Thread(), m_busHandler(busHandler), m_protocol(busHandler->getProtocol()), m_reconnectCount(0),
    m_userList(opt.accessLevel), m_messages(messages),
    m_scanHelper(scanHelper), m_address(opt.address), m_scanConfig(opt.scanConfig),
    m_initialScan(opt.readOnly ? (symbol_t)ESC : opt.initialScan), m_scanRetries(opt.scanRetries),
    m_scanStatus(SCAN_STATUS_NONE), m_polling(opt.pollInterval > 0), m_enableHex(opt.enableHex),
    m_shutdown(false), m_runUpdateCheck(opt.updateCheck), m_httpClient(), m_requestQueue(requestQueue) {
  if (opt.aclFile[0]) {
    string errorDescription;
    time_t mtime = 0;
    istream* stream = FileReader::openFile(opt.aclFile, &errorDescription, &mtime);
    result_t result;
    if (stream) {
      result = m_userList.readFromStream(stream, opt.aclFile, mtime, false, nullptr, &errorDescription);
      delete(stream);
    } else {
      result = RESULT_ERR_NOTFOUND;
    }
    if (result != RESULT_OK) {
      logError(lf_main, "error reading ACL file \"%s\": %s", opt.aclFile, getResultCode(result));
    }
  }

  m_htmlPath = opt.htmlPath;
  logInfo(lf_main, "registering data handlers");
  if (datahandler_register(&m_userList, m_busHandler, messages, &m_dataHandlers)) {
    logInfo(lf_main, "registered data handlers");
  } else {
    logError(lf_main, "error registering data handlers");
  }
  if (opt.enableDefine) {
    m_newlyDefinedMessages = new MessageMap(true, "", false);
    m_newlyDefinedMessages->setResolver(scanHelper);
  } else {
    m_newlyDefinedMessages = nullptr;
  }
}

MainLoop::~MainLoop() {
  m_shutdown = true;
  join();

  for (const auto dataHandler : m_dataHandlers) {
    delete dataHandler;
  }
  m_dataHandlers.clear();
  if (m_newlyDefinedMessages) {
    delete m_newlyDefinedMessages;
    m_newlyDefinedMessages = nullptr;
  }
}

void MainLoop::shutdown() {
  m_shutdown = true;
  if (m_requestQueue != nullptr) {
    m_requestQueue->push(nullptr);  // just to notify potentially waiting thread
  }
}

/** the delay for running the update check. */
#define CHECK_DELAY (24*3600)

/** the initial delay for running the update check. */
#define CHECK_INITIAL_DELAY (2*60)

/** the number of completed scan runs after which to try again failed ones. */
#define SCAN_REPEAT_COUNT 6

void MainLoop::run() {
  bool reload = true;
  time_t lastTaskRun, now, start, lastSignal = 0, since, sinkSince = 1, nextCheckRun;
  int taskDelay = 5;
  symbol_t lastScanAddress = 0;  // 0 is known to be a master
  scanStatus_t lastScanStatus = m_scanStatus;
  int scanCompleted = 0;
  int scanRetry = 0;
  time(&now);
  start = now;
  lastTaskRun = now;
  nextCheckRun = now + CHECK_INITIAL_DELAY;
  ostringstream updates;
  list<DataSink*> dataSinks;
  deque<Message*> messages;

  for (const auto dataHandler : m_dataHandlers) {
    if (dataHandler->isDataSink()) {
      dataSinks.push_back(dynamic_cast<DataSink*>(dataHandler));
    }
    dataHandler->startHandler();
  }
  while (!m_shutdown) {
    // pick the next request to handle
    Request* req = m_requestQueue->pop(taskDelay);
    time(&now);
    if (now < lastTaskRun) {
      // clock skew
      if (now < lastSignal) {
        lastSignal -= lastTaskRun-now;
      }
      lastTaskRun = now;
    } else if (!m_shutdown && now > lastTaskRun+taskDelay) {
      logDebug(lf_main, "performing regular tasks");
      if (m_protocol->hasSignal()) {
        lastSignal = now;
      } else if (lastSignal && now > lastSignal+RECONNECT_MISSING_SIGNAL) {
        lastSignal = 0;
        m_protocol->reconnect();
        m_reconnectCount++;
      }
      if (m_scanConfig && scanRetry <= m_scanRetries) {
        bool loadDelay = false;
        if (m_initialScan != ESC && reload && m_protocol->hasSignal()) {
          loadDelay = true;
          result_t result;
          if (m_initialScan == SYN) {
            logNotice(lf_main, "starting initial full scan");
            result = m_busHandler->startScan(true, "*");
            if (result == RESULT_OK) {
              m_scanStatus = SCAN_STATUS_RUNNING;
            }
          } else if (m_initialScan == BROADCAST) {
            logNotice(lf_main, "starting initial broadcast scan");
            MasterSymbolString master;
            SlaveSymbolString slave;
            master.push_back(m_address);
            master.push_back(BROADCAST);
            master.push_back(0x07);
            master.push_back(0xfe);  // query existance message
            master.adjustHeader();
            result = m_protocol->sendAndWait(master, &slave);
          } else {
            logNotice(lf_main, "starting initial scan for %2.2x", m_initialScan);
            result = m_busHandler->scanAndWait(m_initialScan, true);
            if (result == RESULT_OK) {
              ostringstream ret;
              if (m_busHandler->formatScanResult(m_initialScan, false, &ret)) {
                logNotice(lf_main, "initial scan result: %s", ret.str().c_str());
              }
              m_scanStatus = SCAN_STATUS_RUNNING;
            }
          }
          if (result != RESULT_OK) {
            logError(lf_main, "initial scan failed: %s", getResultCode(result));
          } else {
            reload = false;
          }
        }
        if (!loadDelay && m_protocol->hasSignal()) {
          lastScanAddress = m_busHandler->getNextScanAddress(lastScanAddress, scanCompleted >= SCAN_REPEAT_COUNT);
          if (lastScanAddress == SYN) {
            taskDelay = 5;
            lastScanAddress = 0;
            m_scanStatus = SCAN_STATUS_FINISHED;
            scanCompleted++;
            if (scanCompleted > SCAN_REPEAT_COUNT) {  // repeat failed scan only every Nth time
              scanCompleted = 0;
              scanRetry++;
              logNotice(lf_main, "scan completed %d time(s), %s", scanRetry,
                        scanRetry <= m_scanRetries ? "check again" : "end");
            }
          } else {
            m_scanStatus = SCAN_STATUS_RUNNING;
            result_t result = m_busHandler->scanAndWait(lastScanAddress, true);
            taskDelay = (result == RESULT_ERR_NO_SIGNAL) ? 10 : 1;
            if (result != RESULT_OK) {
              logError(lf_main, "scan config %2.2x: %s", lastScanAddress, getResultCode(result));
            } else {
              logInfo(lf_main, "scan config %2.2x message received", lastScanAddress);
              nextCheckRun = now + CHECK_INITIAL_DELAY;  // delay update check due to new scan data
            }
          }
        }
        if (lastScanStatus != m_scanStatus && !dataSinks.empty()) {
          lastScanStatus = m_scanStatus;
          for (const auto dataSink : dataSinks) {
            dataSink->notifyScanStatus(lastScanStatus);
          }
        }
      } else if (reload && m_protocol->hasSignal()) {
        reload = false;
        // execute initial instructions
        m_scanHelper->executeInstructions(m_busHandler);
        if (m_messages->sizeConditions() > 0 && !m_polling) {
          logError(lf_main, "conditions require a poll interval > 0");
        }
        // notify data sinks to make them update the messages
        for (const auto dataSink : dataSinks) {
          dataSink->notifyScanStatus(SCAN_STATUS_FINISHED);
        }
      }
      if (m_runUpdateCheck && !m_shutdown && now > nextCheckRun) {
        if (!m_httpClient.connect("upd.ebusd.eu",
#ifdef HAVE_SSL
                            443, true,
#else  // HAVE_SSL
                            80, false,
#endif  // HAVE_SSL
                            PACKAGE_NAME "/" PACKAGE_VERSION)) {
          logError(lf_main, "update check connect error");
          nextCheckRun = now + CHECK_INITIAL_DELAY;
        } else {
          ostringstream ostr;
          ostr << "{\"v\":\"" PACKAGE_VERSION "\",\"r\":\"" REVISION << "\""
#if defined(__amd64__) || defined(__x86_64__) || defined(__ia64__) || defined(__IA64__)
               << ",\"a\":\"amd64\""
#elif defined(__aarch64__)
               << ",\"a\":\"aarch64\""
#elif defined(__arm__)
               << ",\"a\":\"arm\""
#elif defined(__i386__) || defined(__i686__)
               << ",\"a\":\"i386\""
#elif defined(__mips__)
               << ",\"a\":\"mips\""
#else
               << ",\"a\":\"other\""
#endif
               << ",\"u\":" << (now-start);
          m_protocol->formatInfoJson(&ostr);
          if (m_reconnectCount) {
            ostr << ",\"rc\":" << m_reconnectCount;
          }
          m_busHandler->formatUpdateInfo(&ostr);
          ostr << "}";
          string response;
          bool repeat = false;
          if (!m_httpClient.post("/", ostr.str(), &response, &repeat)) {
            logError(lf_main, "update check error: %s", response.c_str());
            nextCheckRun = now + (repeat ? CHECK_INITIAL_DELAY : CHECK_DELAY);
          } else {
            m_updateCheck = response.empty() ? "unknown" : response;
            logNotice(lf_main, "update check: %s", response.c_str());
            if (!dataSinks.empty()) {
              for (const auto dataSink : dataSinks) {
                dataSink->notifyUpdateCheckResult(response == "OK" ? "" : m_updateCheck);
              }
            }
            nextCheckRun = now + CHECK_DELAY;
          }
        }
      }
      time(&lastTaskRun);
    }
    time(&now);
    if (!dataSinks.empty()) {
      messages.clear();
      m_messages->lock();
      m_messages->findAll("", "", "*", false, true, true, true, true, true, sinkSince, now, false, &messages);
      for (const auto message : messages) {
        bool changed = message->getLastChangeTime() >= sinkSince;
        for (const auto dataSink : dataSinks) {
          dataSink->notifyUpdate(message, changed);
        }
      }
      m_messages->unlock();
      sinkSince = now;
    }
    if (req == nullptr) {
      continue;
    }
    if (m_shutdown) {
      req->setResult("ERR: shutdown", "", nullptr, now, true);
      break;
    }
    string user = req->getUser();
    RequestMode reqMode = req->getMode(&since);
    if (reqMode.listenMode == lm_none) {
      since = now;
    }
    ostringstream ostream;
    bool connected = true;
    if (!req->empty()) {
      req->log();
      bool currentReload = reload;
      result_t result = decodeRequest(req, &connected, &reqMode, &user, &reload, &ostream);
      if (reload && !currentReload) {
        scanRetry = 0;  // restart scan counting
      }
      if (!req->isHttp() && (ostream.tellp() == 0 || result != RESULT_OK)) {
        if (reqMode.listenMode != lm_direct) {
          ostream.str("");
        }
        ostream << getResultCode(result);
      }
      const auto resp = ostream.str();
      req->log(&resp);
      if (ostream.tellp() == 0) {
        ostream << "\n";  // only for HTTP
      } else if (!req->isHttp()) {
        ostream << (reqMode.listenMode == lm_direct ? "\n" : "\n\n");
      }
    }
    if (reqMode.listenMode == lm_listen) {
      if (!reqMode.listenOnlyUnknown) {
        string levels = getUserLevels(user);
        messages.clear();
        m_messages->findAll("", "", levels, false, true, true, true, true, true, since, now, true, &messages);
        for (const auto message : messages) {
          ostream << message->getCircuit() << " " << message->getName() << " = " << dec;
          message->decodeLastData(false, nullptr, -1, reqMode.format, &ostream);
          ostream << endl;
        }
      }
      if (reqMode.listenWithUnknown || reqMode.listenOnlyUnknown) {
        if (m_busHandler->isGrabEnabled()) {
          m_busHandler->formatGrabResult(true, OF_NONE, &ostream, true, since, now);
        } else {
          m_busHandler->enableGrab(true);  // needed for listening to all messages
        }
      }
    } else if (reqMode.listenMode == lm_direct) {
      if (m_busHandler->isGrabEnabled()) {
        m_busHandler->formatGrabResult(false, OF_NONE, &ostream, true, since, now);
      }
    }
    // send result to client
    req->setResult(ostream.str(), user, &reqMode, now, !connected);
  }
}

result_t MainLoop::decodeRequest(Request* req, bool* connected, RequestMode* reqMode,
    string* user, bool* reload, ostringstream* ostream) {
  vector<string> args;
  req->split(&args);
  string cmd = args.size() > 0 ? args[0] : "";
  if (req->isHttp()) {
    if (args.size() < 2) {
      *connected = false;
      *ostream << "HTTP/1.0 400 Bad Request\r\n\r\n";
      return RESULT_OK;
    }
    if (cmd == "GET") {
      return executeGet(args, connected, ostream);
    }
    *connected = false;
    *ostream << "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
    return RESULT_OK;
  }

  transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
  if (cmd == "?" || cmd == "H" || cmd == "HELP") {
    // found "HELP CMD"
    cmd = args.size() > 1 ? args[1] : "";
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    args.clear();  // empty args is used as command help indicator
  }
  if (reqMode->listenMode == lm_direct) {
    return executeDirect(args, reqMode, ostream);
  }
  if (cmd.empty() && args.size() == 0) {
    return executeHelp(ostream);
  }
  if (args.size() == 2) {
    string arg = args[1];
    if (arg == "?" || arg == "-?" || arg == "--help") {
      // found "CMD HELP"
      args.clear();  // empty args is used as command help indicator
    }
  }
  if (cmd == "AUTH" || cmd == "A") {
    return executeAuth(args, user, ostream);
  }
  if (cmd == "R" || cmd == "READ") {
    return executeRead(args, getUserLevels(*user), ostream);
  }
  if (cmd == "W" || cmd == "WRITE") {
    return executeWrite(args, getUserLevels(*user), ostream);
  }
  if (cmd == "HEX") {
    if (m_enableHex) {
      return executeHex(args, ostream);
    }
    *ostream << "ERR: command not enabled";
    return RESULT_OK;
  }
  if (cmd == "INJECT") {
    if (m_enableHex) {
      return executeInject(args, ostream);
    }
    *ostream << "ERR: command not enabled";
    return RESULT_OK;
  }
  if (cmd == "ANSWER") {
    if (m_enableHex && m_protocol->isAnswering()) {
      return executeAnswer(args, ostream);
    }
    *ostream << "ERR: command not enabled";
    return RESULT_OK;
  }
  if (cmd == "F" || cmd == "FIND") {
    return executeFind(args, getUserLevels(*user), ostream);
  }
  if (cmd == "L" || cmd == "LISTEN") {
    return executeListen(args, reqMode, ostream);
  }
  if (cmd == "DIRECT") {
    return executeDirect(args, reqMode, ostream);
  }
  if (cmd == "S" || cmd == "STATE") {
    return executeState(args, ostream);
  }
  if (cmd == "G" || cmd == "GRAB") {
    return executeGrab(args, ostream);
  }
  if (cmd == "DEF" || cmd == "DEFINE") {
    if (m_newlyDefinedMessages) {
      return executeDefine(args, ostream);
    }
    *ostream << "ERR: command not enabled";
    return RESULT_OK;
  }
  if (cmd == "D" || cmd == "DECODE") {
    return executeDecode(args, ostream);
  }
  if (cmd == "E" || cmd == "ENCODE") {
    return executeEncode(args, ostream);
  }
  if (cmd == "SCAN") {
    return executeScan(args, getUserLevels(*user), ostream);
  }
  if (cmd == "LOG") {
    return executeLog(args, ostream);
  }
  if (cmd == "RAW") {
    return executeRaw(args, ostream);
  }
  if (cmd == "DUMP") {
    return executeDump(args, ostream);
  }
  if (cmd == "RELOAD") {
    *reload = true;
    return executeReload(args, ostream);
  }
  if (cmd == "Q" || cmd == "QUIT") {
    return executeQuit(args, connected, ostream);
  }
  if (cmd == "I" || cmd == "INFO") {
    return executeInfo(args, *user, ostream);
  }
  if (cmd == "?" || cmd == "H" || cmd == "HELP") {
    return executeHelp(ostream);
  }
  *ostream << "ERR: command not found";
  return RESULT_OK;
}

result_t MainLoop::parseHexMaster(const vector<string>& args, size_t argPos, symbol_t srcAddress, bool autoLength,
    MasterSymbolString* master) {
  ostringstream msg;
  while (argPos < args.size()) {
    if ((args[argPos].length() % 2) != 0) {
      return RESULT_ERR_INVALID_NUM;
    }
    msg << args[argPos++];
  }
  string str = msg.str();
  unsigned int length = static_cast<unsigned int>(str.size());
  if (length < (autoLength ? 3 : 4)*2) {  // at least ZZ, PB, SB[, NN]
    return RESULT_ERR_INVALID_ARG;
  }
  result_t ret;
  if (autoLength) {
    length = length/2 - 3;
  } else {
    length = parseInt(str.substr(3*2, 2).c_str(), 16, 0, MAX_POS, &ret);
    if (ret != RESULT_OK) {
      return ret;
    }
  }
  if (((autoLength ? 3 : 4)+length)*2 != str.size()) {
    return RESULT_ERR_INVALID_ARG;
  }
  master->push_back(srcAddress == SYN ? m_address : srcAddress);
  ret = master->parseHex(str.substr(0, 3*2));
  if (ret == RESULT_OK && !isValidAddress((*master)[1])) {
    ret = RESULT_ERR_INVALID_ADDR;
  }
  if (ret != RESULT_OK) {
    return ret;
  }
  if (autoLength) {
    master->push_back(static_cast<symbol_t>(length));
  }
  ret = master->parseHex(str.substr(3*2));
  return ret;
}

result_t MainLoop::executeAuth(const vector<string>& args, string* user, ostringstream* ostream) {
  if (args.size() != 3) {
    *ostream << "usage: auth USER SECRET\n"
                " Authenticate with USER name and SECRET.\n"
                "  USER    the user name\n"
                "  SECRET  the secret string of the user";
    return RESULT_OK;
  }
  if (m_userList.checkSecret(args[1], args[2])) {
    *user = args[1];
    return RESULT_OK;
  }
  *ostream << "ERR: invalid user name or secret";
  return RESULT_OK;
}

result_t MainLoop::executeRead(const vector<string>& args, const string& levels, ostringstream* ostream) {
  size_t argPos = 1;
  bool hex = false, newDefinition = false;
  OutputFormat verbosity = OF_NONE;
  time_t maxAge = 5*60;
  string circuit, params;
  symbol_t srcAddress = SYN, dstAddress = SYN;
  size_t pollPriority = 0;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-h") {
      hex = true;
    } else if (args[argPos] == "-def") {
      if (!m_newlyDefinedMessages) {
        *ostream << "ERR: option not enabled";
        return RESULT_OK;
      }
      newDefinition = true;
    } else if (args[argPos] == "-f") {
      maxAge = 0;
    } else if (args[argPos] == "-m") {
      argPos++;
      if (args.size() > argPos) {
        result_t result;
        maxAge = parseInt(args[argPos].c_str(), 10, 0, 24*60*60, &result);
        if (result != RESULT_OK) {
          argPos = 0;  // print usage
          break;
        }
      } else {
        argPos = 0;  // print usage
        break;
      }
    } else if (args[argPos] == "-v") {
      if ((verbosity & VERBOSITY_3) == VERBOSITY_0) {
        verbosity |= VERBOSITY_1;
      } else if ((verbosity & VERBOSITY_3) == VERBOSITY_1) {
        verbosity |= VERBOSITY_2;
      } else {
        verbosity |= VERBOSITY_3;
      }
    } else if (args[argPos] == "-vv") {
      verbosity |= VERBOSITY_2;
    } else if (args[argPos] == "-vvv" || args[argPos] == "-V") {
      verbosity |= VERBOSITY_3;
    } else if (args[argPos] == "-n") {
      verbosity = (verbosity & ~OF_VALUENAME) | OF_NUMERIC;
    } else if (args[argPos] == "-N") {
      verbosity = (verbosity & ~OF_NUMERIC) | OF_VALUENAME;
    } else if (args[argPos] == "-c") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      circuit = args[argPos];
    } else if (args[argPos] == "-s" || args[argPos] == "-d") {
      bool dest = args[argPos] == "-d";
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      result_t ret;
      symbol_t address = (symbol_t)parseInt(args[argPos].c_str(), 16, 0, 0xff, &ret);
      if (ret != RESULT_OK || !isValidAddress(address, dest) || (dest == isMaster(address))) {
        return RESULT_ERR_INVALID_ADDR;  // deny send from slave address and to master addresses
      }
      if (dest) {
        dstAddress = address;
      } else {
        srcAddress = address == m_address ? (symbol_t)SYN : address;
      }
    } else if (args[argPos] == "-p") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      result_t ret;
      pollPriority = (size_t)parseInt(args[argPos].c_str(), 10, 1, 9, &ret);
      if (ret != RESULT_OK) {
        return RESULT_ERR_INVALID_NUM;
      }
    } else if (args[argPos] == "-i") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      params = args[argPos];
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }
  if ((hex && (newDefinition || verbosity != OF_NONE || !circuit.empty() || !params.empty() || dstAddress != SYN
      || pollPriority > 0 || args.size() < argPos + 1))
  || (newDefinition && (hex || !circuit.empty() || pollPriority > 0 || args.size() != argPos + 1))) {
    argPos = 0;  // print usage
  }

  if (argPos == 0 || args.size() < argPos + 1 || args.size() > argPos + 2) {
    *ostream <<
        "usage: read [-f] [-m SECONDS] [-s QQ] [-d ZZ] [-c CIRCUIT] [-p PRIO] [-v|-V] [-n|-N] [-i VALUE[;VALUE]*]"
        " NAME [FIELD[.N]]\n"
        "  or:  read [-f] [-m SECONDS] [-s QQ] [-d ZZ] [-v|-V] [-n|-N] [-i VALUE[;VALUE]*] -def DEFINITION "
        "(only if enabled)\n"
        "  or:  read [-f] [-m SECONDS] [-s QQ] [-c CIRCUIT] -h ZZPBSBNN[DD]*\n"
        " Read value(s) or hex message.\n"
        "  -f           force reading from the bus (same as '-m 0')\n"
        "  -m SECONDS   only return cached value if age is less than SECONDS [300]\n"
        "  -c CIRCUIT   limit to messages of CIRCUIT\n"
        "  -s QQ        override source address QQ\n"
        "  -d ZZ        override destination address ZZ\n"
        "  -p PRIO      set the message poll priority (1-9)\n"
        "  -v           increase verbosity (include names/units/comments)\n"
        "  -V           be very verbose (include names, units, and comments)\n"
        "  -n           use numeric value of value=name pairs\n"
        "  -N           use numeric and named value of value=name pairs\n"
        "  -i VALUE     read additional message parameters from VALUE\n"
        "  NAME         NAME of the message to send\n"
        "  FIELD        only retrieve the field named FIELD\n"
        "  N            only retrieve the N'th field named FIELD (0-based)\n"
        "  -def         read with explicit message definition (only if enabled):\n"
        "    DEFINITION message definition to use instead of known definition\n"
        "  -h           send hex read message (or answer from cache):\n"
        "    ZZ         destination address\n"
        "    PB SB      primary/secondary command byte\n"
        "    NN         number of following data bytes\n"
        "    DD         data byte(s) to send";
    return RESULT_OK;
  }
  time_t now;
  time(&now);

  if (hex) {
    MasterSymbolString master;
    result_t ret = parseHexMaster(args, argPos, srcAddress, false, &master);
    if (ret != RESULT_OK) {
      return ret;
    }
    if (master[1] == BROADCAST || isMaster(master[1])) {
      return RESULT_ERR_INVALID_ARG;
    }
    logNotice(lf_main, "read hex cmd: %s", master.getStr().c_str());

    // find message
    Message* message = m_messages->find(master, false, true, false, false);

    if (message == nullptr) {
      return RESULT_ERR_NOTFOUND;
    }
    if (!message->hasLevel(levels)) {
      return RESULT_ERR_NOTAUTHORIZED;
    }
    if (message->isWrite()) {
      return RESULT_ERR_INVALID_ARG;
    }
    if (circuit.length() > 0 && circuit != message->getCircuit()) {
      return RESULT_ERR_INVALID_ARG;  // non-matching circuit
    }
    if (srcAddress == SYN
        && (message->getLastUpdateTime() + maxAge > now
            || (message->isPassive() && message->getLastUpdateTime() != 0))) {
      const SlaveSymbolString& slave = message->getLastSlaveData();
      logNotice(lf_main, "hex read %s %s from cache", message->getCircuit().c_str(), message->getName().c_str());
      *ostream << slave.getStr();
      return RESULT_OK;
    }

    // send message
    SlaveSymbolString slave;
    ret = m_protocol->sendAndWait(master, &slave);

    if (ret == RESULT_OK) {
      ret = message->storeLastData(master, slave);
      ostringstream result;
      if (ret == RESULT_OK) {
        ret = message->decodeLastData(false, nullptr, -1, OF_NONE, &result);
      }
      if (ret >= RESULT_OK) {
        logInfo(lf_main, "read hex %s %s cache update: %s", message->getCircuit().c_str(), message->getName().c_str(),
                result.str().c_str());
      } else {
        logError(lf_main, "read hex %s %s cache update: %s", message->getCircuit().c_str(), message->getName().c_str(),
                 getResultCode(ret));
      }
      *ostream << slave.getStr();
      return RESULT_OK;
    }
    logError(lf_main, "read hex %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
             getResultCode(ret));
    return ret;
  }

  string fieldName;
  ssize_t fieldIndex = -2;
  if (!newDefinition && args.size() == argPos + 2) {
    fieldName = args[argPos + 1];
    fieldIndex = -1;
    size_t pos = fieldName.find_last_of('.');
    if (pos != string::npos) {
      result_t result = RESULT_OK;
      fieldIndex = static_cast<ssize_t>(parseInt(fieldName.substr(pos+1).c_str(), 10, 0, MAX_POS, &result));
      if (result == RESULT_OK) {
        fieldName = fieldName.substr(0, pos);
      }
    }
  }

  string name;
  Message* message;
  result_t ret;
  if (newDefinition) {
    string errorDescription;
    istringstream defstr("#\n" + args[argPos]);  // ensure first line is not used for determining col names
    m_newlyDefinedMessages->clear();
    ret = m_newlyDefinedMessages->readFromStream(&defstr, "temporary", now, true, nullptr, &errorDescription);
    if (ret != RESULT_OK) {
      *ostream << "ERR: bad definition: " << errorDescription;
      return RESULT_OK;
    }
    deque<Message*> messages;
    m_newlyDefinedMessages->findAll("", "", levels, false, true, false, false, true, false, 0, 0, false, &messages);
    if (messages.empty()) {
      *ostream << "ERR: bad definition: no read message";
      return RESULT_OK;
    }
    message = *messages.begin();
  } else {
    name = args[argPos];
    message = m_messages->find(circuit, name, levels, false);
  }
  // adjust poll priority
  if (!newDefinition && message != nullptr && pollPriority > 0 && message->setPollPriority(pollPriority)) {
    m_messages->addPollMessage(false, message);
  }
  bool allowCache = !newDefinition && srcAddress == SYN && dstAddress == SYN && maxAge > 0 && params.length() == 0;
  Message* cacheMessage = allowCache ? m_messages->find(circuit, name, levels, false, true) : nullptr;
  bool hasCache = cacheMessage != nullptr;
  if (!hasCache || (allowCache && message && message->getLastUpdateTime() > cacheMessage->getLastUpdateTime())) {
    cacheMessage = message;  // message is newer/better
  }
  if (cacheMessage && (cacheMessage->getLastUpdateTime() + maxAge > now
                       || (cacheMessage->isPassive() && cacheMessage->getLastUpdateTime() != 0))) {
    if (verbosity & OF_NAMES) {
      *ostream << cacheMessage->getCircuit() << " " << cacheMessage->getName() << " ";
    }
    ret = cacheMessage->decodeLastData(false, fieldIndex == -2 ? nullptr : fieldName.c_str(), fieldIndex, verbosity,
        ostream);
    if (ret != RESULT_OK) {
      if (ret < RESULT_OK) {
        logError(lf_main, "read %s %s cached: %s", cacheMessage->getCircuit().c_str(),
            cacheMessage->getName().c_str(), getResultCode(ret));
      }
      return ret;
    }
    logInfo(lf_main, "read %s %s cached: %s", cacheMessage->getCircuit().c_str(), cacheMessage->getName().c_str(),
        ostream->str().c_str());
    return RESULT_OK;
  }

  if (!message && hasCache) {
    *ostream << "ERR: no data stored";
    return RESULT_OK;
  }  // else: read directly from bus

  if (message == nullptr) {
    return RESULT_ERR_NOTFOUND;
  }
  if (message->getDstAddress() == SYN && dstAddress == SYN) {
    return RESULT_ERR_INVALID_ADDR;
  }
  // read directly from bus
  ret = m_busHandler->readFromBus(message, params, dstAddress, srcAddress);
  if (ret != RESULT_OK) {
    return ret;
  }
  if (verbosity & OF_NAMES) {
    *ostream << message->getCircuit() << " " << message->getName() << " ";
  }
  ret = message->decodeLastData(false, false, fieldIndex == -2 ? nullptr : fieldName.c_str(), fieldIndex, verbosity,
      ostream);
  if (ret < RESULT_OK) {
    logError(lf_main, "read %s %s: decode %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    ostream->str("");
    *ostream << getResultCode(ret) << " in decode";
    return RESULT_OK;
  }
  if (ret > RESULT_OK) {
    return ret;
  }
  logInfo(lf_main, "read %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(), ostream->str().c_str());
  return ret;
}

result_t MainLoop::executeWrite(const vector<string>& args, const string levels, ostringstream* ostream) {
  size_t argPos = 1;
  bool hex = false, newDefinition = false;
  string circuit;
  symbol_t srcAddress = SYN, dstAddress = SYN;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-h") {
      hex = true;
    } else if (args[argPos] == "-def") {
      if (!m_newlyDefinedMessages) {
        *ostream << "ERR: option not enabled";
        return RESULT_OK;
      }
      newDefinition = true;
    } else if (args[argPos] == "-s" || args[argPos] == "-d") {
      bool dest = args[argPos] == "-d";
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      result_t ret;
      symbol_t address = (symbol_t)parseInt(args[argPos].c_str(), 16, 0, 0xff, &ret);
      if (ret != RESULT_OK || !isValidAddress(address, dest) || (!dest && !isMaster(address))) {
        return RESULT_ERR_INVALID_ADDR;  // deny send from slave address
      }
      if (dest) {
        dstAddress = address;
      } else {
        srcAddress = address == m_address ? (symbol_t)SYN : address;
      }
    } else if (args[argPos] == "-c") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      circuit = args[argPos];
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }

  if ((args.size() < argPos + 1)
  || (hex && (newDefinition || dstAddress != SYN))
  || (newDefinition && (hex || !circuit.empty() || args.size() > argPos + 2))
  || (!newDefinition && !hex && (circuit.empty() || args.size() > argPos + 2))) {
    argPos = 0;  // print usage
  }

  if (argPos == 0) {
    *ostream << "usage: write [-s QQ] [-d ZZ] -c CIRCUIT NAME [VALUE[;VALUE]*]\n"
        "  or:  write [-s QQ] [-d ZZ] -def DEFINITION [VALUE[;VALUE]*] (only if enabled)\n"
        "  or:  write [-s QQ] [-c CIRCUIT] -h ZZPBSBNN[DD]*\n"
        " Write value(s) or hex message.\n"
        "  -s QQ        override source address QQ\n"
        "  -d ZZ        override destination address ZZ\n"
        "  -c CIRCUIT   CIRCUIT of the message to send\n"
        "  NAME         NAME of the message to send\n"
        "  VALUE        a single field VALUE\n"
        "  -def         write with explicit message definition (only if enabled):\n"
        "    DEFINITION message definition to use instead of known definition\n"
        "  -h           send hex write message:\n"
        "    ZZ         destination address\n"
        "    PB SB      primary/secondary command byte\n"
        "    NN         number of following data bytes\n"
        "    DD         data byte(s) to send";
    return RESULT_OK;
  }

  if (hex && argPos > 0) {
    MasterSymbolString master;
    result_t ret = parseHexMaster(args, argPos, srcAddress, false, &master);
    if (ret != RESULT_OK) {
      return ret;
    }
    logNotice(lf_main, "write hex cmd: %s", master.getStr().c_str());

    // find message
    Message* message = m_messages->find(master, false, false, true, false);

    if (message == nullptr) {
      return RESULT_ERR_NOTFOUND;
    }
    if (!message->hasLevel(levels)) {
      return RESULT_ERR_NOTAUTHORIZED;
    }
    if (!message->isWrite()) {
      return RESULT_ERR_INVALID_ARG;
    }
    if (!circuit.empty() && circuit != message->getCircuit()) {
      return RESULT_ERR_INVALID_ARG;  // non-matching circuit
    }

    // send message
    SlaveSymbolString slave;
    ret = m_protocol->sendAndWait(master, &slave);

    if (ret == RESULT_OK) {
      // also update read messages
      ret = message->storeLastData(master, slave);
      ostringstream result;
      if (ret == RESULT_OK) {
        ret = message->decodeLastData(false, nullptr, -1, OF_NONE, &result);
      }
      if (ret >= RESULT_OK) {
        logInfo(lf_main, "write hex %s %s cache update: %s", message->getCircuit().c_str(),
            message->getName().c_str(), result.str().c_str());
      } else {
        logError(lf_main, "write hex %s %s cache update: %s", message->getCircuit().c_str(),
            message->getName().c_str(), getResultCode(ret));
      }
      if (master[1] == BROADCAST) {
        *ostream << "done broadcast";
        return RESULT_OK;
      }
      if (isMaster(master[1])) {
        return RESULT_OK;
      }
      *ostream << slave.getStr();
      return RESULT_OK;
    }
    logError(lf_main, "write hex %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    return ret;
  }

  Message* message;
  result_t ret;
  if (newDefinition) {
    time_t now;
    time(&now);
    string errorDescription;
    istringstream defstr("#\n" + args[argPos]);  // ensure first line is not used for determining col names
    m_newlyDefinedMessages->clear();
    ret = m_newlyDefinedMessages->readFromStream(&defstr, "temporary", now, true, nullptr, &errorDescription);
    if (ret != RESULT_OK) {
      *ostream << "ERR: bad definition: " << errorDescription;
      return RESULT_OK;
    }
    deque<Message*> messages;
    m_newlyDefinedMessages->findAll("", "", levels, false, false, true, false, true, false, 0, 0, false, &messages);
    if (messages.empty()) {
      *ostream << "ERR: bad definition: no write message";
      return RESULT_OK;
    }
    message = *messages.begin();
  } else {
    message = m_messages->find(circuit, args[argPos], levels, true);
  }

  if (message == nullptr) {
    return RESULT_ERR_NOTFOUND;
  }
  if (message->getDstAddress() == SYN && dstAddress == SYN) {
    return RESULT_ERR_INVALID_ADDR;
  }
  // allow missing values
  ret = m_busHandler->readFromBus(message, args.size() == argPos + 1 ? "" : args[argPos + 1], dstAddress, srcAddress);
  if (ret != RESULT_OK) {
    logError(lf_main, "write %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    return ret;
  }
  dstAddress = message->getLastMasterData().dataAt(1);
  if (dstAddress == BROADCAST || isMaster(dstAddress)) {
    logNotice(lf_main, "write %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    if (dstAddress == BROADCAST) {
      *ostream << "done broadcast";
    }
    return RESULT_OK;
  }

  ret = message->decodeLastData(false, false, nullptr, -1, OF_NONE, ostream);  // decode data
  if (ret >= RESULT_OK && ostream->str().empty()) {
    logNotice(lf_main, "write %s %s: decode %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    return RESULT_OK;
  }
  if (ret != RESULT_OK) {
    logError(lf_main, "write %s %s: decode %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(ret));
    ostream->str("");
    *ostream << getResultCode(ret) << " in decode";
    return RESULT_OK;
  }
  logNotice(lf_main, "write %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
      ostream->str().c_str());
  return RESULT_OK;
}

result_t MainLoop::parseHexAndSend(const vector<string>& args, size_t& argPos, bool isDirectMode,
    ostringstream* ostream) {
  symbol_t srcAddress = SYN;
  bool autoLength = false;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-s" && argPos + 1 < args.size()) {
      result_t ret;
      argPos++;
      symbol_t address = (symbol_t)parseInt(args[argPos].c_str(), 16, 0, 0xff, &ret);
      if (ret != RESULT_OK || !isValidAddress(address, false) || !isMaster(address)) {
        return RESULT_ERR_INVALID_ADDR;
      }
      srcAddress = address == m_address ? (symbol_t)SYN : address;
    } else if (args[argPos] == "-n") {
      autoLength = true;
    } else {
      argPos = 0;  // print usage
      return RESULT_OK;
    }
    argPos++;
  }
  if (argPos >= args.size()) {
    argPos = 0;  // print usage
    return RESULT_OK;
  }

  MasterSymbolString master;
  result_t ret = parseHexMaster(args, argPos, srcAddress, autoLength, &master);
  argPos = args.size();  // mark as successfully parsed
  if (ret != RESULT_OK) {
    return ret;
  }
  logNotice(lf_main, isDirectMode ? "direct cmd: %s" : "hex cmd: %s", master.getStr().c_str());

  // send message
  SlaveSymbolString slave;
  ret = m_protocol->sendAndWait(master, &slave);

  if (ret == RESULT_OK) {
    if (master[1] == BROADCAST) {
      *ostream << "done broadcast";
      return RESULT_OK;
    }
    if (isMaster(master[1])) {
      *ostream << "done";
      return RESULT_OK;
    }
    *ostream << slave.getStr();
    return RESULT_OK;
  }
  logError(lf_main, isDirectMode ? "direct: %s" : "hex: %s", getResultCode(ret));
  return ret;
}

result_t MainLoop::executeHex(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  result_t ret = parseHexAndSend(args, argPos, false, ostream);
  if (argPos != 0 && argPos == args.size()) {
    return ret;
  }
  *ostream << "usage: hex [-s QQ] ZZPBSBNN[DD]*\n"
              "  or:  hex [-s QQ] -n ZZPBSB[DD]*\n"
              " Send arbitrary data in hex.\n"
              "  -s QQ  override source address QQ\n"
              "  -n     automatically determine the number of data bytes\n"
              "  ZZ     destination address\n"
              "  PB SB  primary/secondary command byte\n"
              "  NN     number of following data bytes\n"
              "  DD     data byte(s) to send";
  return RESULT_OK;
}

result_t MainLoop::executeInject(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  if (argPos < args.size()) {
    MasterSymbolString master;
    SlaveSymbolString slave;
    if (!m_scanHelper->parseMessage(args[argPos++], false, &master, &slave)) {
      return RESULT_ERR_INVALID_ARG;
    }
    m_busHandler->notifyProtocolMessage(md_recv, master, slave);
    return RESULT_OK;
  }
  *ostream << "usage: inject QQZZPBSBNN[DD]*/[NN[DD]*]\n"
              " Inject hex data (without sending to bus).\n"
              "  QQ     source address\n"
              "  ZZ     destination address\n"
              "  PB SB  primary/secondary command byte\n"
              "  NN     number of following data bytes\n"
              "  DD     data byte(s)";
  return RESULT_OK;
}

result_t MainLoop::executeAnswer(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  symbol_t srcAddress = SYN;
  symbol_t dstAddress = SYN;
  bool master = false;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-s" && argPos + 1 < args.size()) {
      result_t ret;
      argPos++;
      symbol_t address = (symbol_t)parseInt(args[argPos].c_str(), 16, 0, 0xff, &ret);
      if (ret != RESULT_OK || !isValidAddress(address, false) || !isMaster(address)) {
        return RESULT_ERR_INVALID_ADDR;
      }
      srcAddress = address;
    } else if (args[argPos] == "-d" && argPos + 1 < args.size()) {
      result_t ret;
      argPos++;
      symbol_t address = (symbol_t)parseInt(args[argPos].c_str(), 16, 0, 0xff, &ret);
      if (ret != RESULT_OK || !isValidAddress(address)) {
        return RESULT_ERR_INVALID_ADDR;
      }
      dstAddress = address;
    } else if (args[argPos] == "-m") {
      master = true;
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }
  MasterSymbolString id;
  if (argPos > 0 && argPos < args.size()) {
    result_t ret = id.parseHex(args[argPos++]);
    if (ret != RESULT_OK) {
      return ret;
    }
    if (id.size() < 2 || id.size() > 6) {
      return RESULT_ERR_INVALID_POS;
    }
  }
  SlaveSymbolString answer;
  answer.push_back(0);  // room for length byte
  if (argPos > 0 && argPos < args.size()) {
    result_t ret = answer.parseHex(args[argPos++]);
    if (ret != RESULT_OK) {
      return ret;
    }
    if (answer.size() > 16) {
      return RESULT_ERR_INVALID_POS;
    }
  }
  answer.adjustHeader();
  if (argPos < args.size()) {
    argPos = 0;  // print usage
  }
  if (argPos <= 1) {
    *ostream << "usage: answer [-m] [-s QQ] [-d ZZ] PBSB[ID]* [DD]*\n"
                " Answer to a message from the bus.\n"
                "  -m     destination is a master\n"
                "  -s QQ  source address to limit to\n"
                "  -d ZZ  override destination address (instead of own address)\n"
                "  PB SB  primary/secondary command byte\n"
                "  ID     further ID bytes\n"
                "  DD     data bytes (only length used with -m)";
    return RESULT_OK;
  }
  if (isMaster(dstAddress)) {
    master = true;
  } else if (dstAddress == SYN) {
    dstAddress = master ? m_address : getSlaveAddress(m_address);
  }
  if (!m_protocol->setAnswer(srcAddress, dstAddress, id[0], id[1], id.data()+2, id.size()-2, answer)) {
    return RESULT_ERR_INVALID_ARG;
  }
  return RESULT_OK;
}

result_t MainLoop::executeDirect(const vector<string>& args, RequestMode* reqMode, ostringstream* ostream) {
  if (reqMode->listenMode != lm_direct) {
    if (args.size() == 1) {
      reqMode->listenMode = lm_direct;
      m_busHandler->enableGrab(true);  // needed for listening to all messages
      *ostream << "direct mode started";
      return RESULT_OK;
    }
    *ostream << "usage: direct\n"
                " Enter direct mode.";
    return RESULT_OK;
  }
  if (args.size() > 0) {
    string firstArg = args[0];
    if (firstArg == "stop") {
      reqMode->listenMode = lm_none;
      *ostream << "direct mode stopped";
      return RESULT_OK;
    }
    if (firstArg != "") {
      for (size_t argPos = 0; argPos < args.size(); argPos++) {
        if (argPos > 0) {
          *ostream << " ";
        }
        *ostream << args[argPos];
      }
      *ostream << ":";
      if (!m_enableHex) {
        *ostream << "ERR: hex command not enabled";
        return RESULT_OK;
      }
      size_t argPos = 0;
      result_t ret = parseHexAndSend(args, argPos, true, ostream);
      if (ret == RESULT_OK && argPos != args.size()) {
        ret = RESULT_ERR_INVALID_ARG;
      }
      return ret;
    }
  }
  *ostream << "usage: [-s QQ] ZZPBSBNN[DD]*\n"
              "  or: [-s QQ] -n ZZPBSB[DD]*\n"
              "  or: stop\n"
              " Send arbitrary data in hex (only if enabled) or stop direct mode.\n"
              "  -s QQ  override source address QQ\n"
              "  -n     automatically determine the number of data bytes\n"
              "  ZZ     destination address\n"
              "  PB SB  primary/secondary command byte\n"
              "  NN     number of following data bytes\n"
              "  DD     data byte(s) to send";
  return RESULT_OK;
}

result_t MainLoop::executeFind(const vector<string>& args, const string& levels, ostringstream* ostream) {
  size_t argPos = 1;
  bool configFormat = false, exact = false, withRead = true, withWrite = false, withPassive = true, first = true,
      onlyWithData = false, hexFormat = false, userLevel = true, withConditions = false;
  string useLevels = levels;
  OutputFormat verbosity = OF_NONE;
  vector<string> fieldNames;
  string circuit;
  vector<symbol_t> id;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-v") {
      if ((verbosity & VERBOSITY_4) == VERBOSITY_0) {
        verbosity |= VERBOSITY_1;
      } else if ((verbosity & VERBOSITY_4) == VERBOSITY_1) {
        verbosity |= VERBOSITY_2;
      } else if ((verbosity & VERBOSITY_4) == VERBOSITY_2) {
        verbosity |= VERBOSITY_3;
      } else {
        verbosity |= VERBOSITY_4;
      }
    } else if (args[argPos] == "-vv") {
      verbosity |= VERBOSITY_2;
    } else if (args[argPos] == "-vvv") {
      verbosity |= VERBOSITY_3;
    } else if (args[argPos] == "-vvvv" || args[argPos] == "-V") {
      verbosity |= VERBOSITY_4;
    } else if (args[argPos] == "-f") {
      configFormat = true;
      if (hexFormat) {
        argPos = 0;  // print usage
        break;
      }
    } else if (args[argPos] == "-F") {
      argPos++;
      if (hexFormat || (argPos >= args.size())) {
        argPos = 0;  // print usage
        break;
      }
      if (!Message::extractFieldNames(args[argPos], true, &fieldNames)) {
        argPos = 0;  // print usage
        break;
      }
    } else if (args[argPos] == "-e") {
      exact = true;
    } else if (args[argPos] == "-r") {
      if (first) {
        first = false;
        withWrite = withPassive = false;
      }
      withRead = true;
    } else if (args[argPos] == "-w") {
      if (first) {
        first = false;
        withRead = withPassive = false;
      }
      withWrite = true;
    } else if (args[argPos] == "-p") {
      if (first) {
        first = false;
        withRead = withWrite = false;
      }
      withPassive = true;
    } else if (args[argPos] == "-a") {
      withRead = withWrite = withPassive = withConditions = true;
    } else if (args[argPos] == "-d") {
      onlyWithData = true;
    } else if (args[argPos] == "-h") {
      hexFormat = true;
      if (configFormat) {
        argPos = 0;  // print usage
        break;
      }
    } else if (args[argPos] == "-i") {
      argPos++;
      if (argPos >= args.size() || !id.empty()) {
        argPos = 0;  // print usage
        break;
      }
      result_t result = Message::parseId(args[argPos], &id);
      if (result != RESULT_OK) {
        return result;
      }
      if (id.empty()) {
        argPos = 0;  // print usage
        break;
      }
    } else if (args[argPos] == "-c") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      circuit = args[argPos];
    } else if (args[argPos] == "-l") {
      argPos++;
      if (argPos >= args.size()) {
        argPos = 0;  // print usage
        break;
      }
      useLevels = args[argPos];
      userLevel = false;
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }
  if (argPos == 0 || args.size() < argPos || args.size() > argPos + 1) {
    *ostream <<
        "usage: find [-v|-V] [-r] [-w] [-p] [-a] [-d] [-h] [-i ID] [-f] [-F COL[,COL]*] [-e] [-c CIRCUIT]"
        " [-l LEVEL] [NAME]\n"
        " Find message(s).\n"
        "  -v             increase verbosity (include names/units/comments+destination address+update time)\n"
        "  -V             be very verbose (include everything)\n"
        "  -r             limit to active read messages (default: read + passive)\n"
        "  -w             limit to active write messages (default: read + passive)\n"
        "  -p             limit to passive messages (default: read + passive)\n"
        "  -a             include all message types (read, passive, and write) and all conditional\n"
        "  -d             only include messages with actual data\n"
        "  -h             show hex data instead of decoded values\n"
        "  -i ID          limit to messages with ID (in hex, PB, SB and further ID bytes)\n"
        "  -f             list messages in CSV configuration file format (including conditions with '-a')\n"
        "  -F COL[,COL]*  list messages in the specified format (including conditions with '-a')\n"
        "                 (COL: type|circuit|level|name|comment|qq|zz|pbsb|id|fields or custom fields)\n"
        "  -e             match NAME and optional CIRCUIT exactly (ignoring case)\n"
        "  -c CIRCUIT     limit to messages of CIRCUIT (or a part thereof without '-e')\n"
        "  -l LEVEL       limit to messages with access LEVEL (\"*\" for any, default: current level)\n"
        "  NAME           NAME of the messages to find (or a part thereof without '-e')";
    return RESULT_OK;
  }
  deque<Message*> messages;
  m_messages->findAll(circuit, args.size() == argPos ? "" : args[argPos], useLevels,
      exact, withRead, withWrite, withPassive, userLevel, !withConditions, 0, 0, false, &messages);

  bool found = false;
  char str[32];
  for (const auto message : messages) {
    if (!id.empty() && !message->checkIdPrefix(id)) {
      continue;
    }
    time_t lastup = message->getLastUpdateTime();
    if (onlyWithData && lastup == 0) {
      continue;
    }
    if (configFormat) {
      if (found) {
        *ostream << endl;
      }
      message->dump(nullptr, withConditions, verbosity, ostream);
    } else if (!fieldNames.empty()) {
      if (found) {
        *ostream << endl;
      }
      message->dump(&fieldNames, withConditions, verbosity, ostream);
    } else {
      if (found) {
        *ostream << endl;
      }
      *ostream << message->getCircuit() << " " << message->getName() << " = ";
      if (lastup == 0) {
        *ostream << "no data stored";
        if (!message->isAvailable()) {
          *ostream << " (message not available due to condition)";
        }
      } else if (hexFormat) {
        *ostream << message->getLastMasterData().getStr() << " / " << message->getLastSlaveData().getStr();
      } else {
        result_t ret = message->decodeLastData(false, nullptr, -1, verbosity, ostream);
        if (ret != RESULT_OK) {
          *ostream << " (" << getResultCode(ret)
                   << " for " << message->getLastMasterData().getStr()
                   << " / " << message->getLastSlaveData().getStr() << ")";
        }
      }
      if ((verbosity & VERBOSITY_3) == VERBOSITY_3) {
        symbol_t dstAddress = message->getDstAddress();
        if (dstAddress != SYN) {
          snprintf(str, sizeof(str), "%02x", dstAddress);
        } else if (lastup != 0 && message->getLastMasterData().size() > 1) {
          snprintf(str, sizeof(str), "%02x", message->getLastMasterData().dataAt(1));
        } else {
          snprintf(str, sizeof(str), "any");
        }
        if (lastup != 0) {
          struct tm td;
          localtime_r(&lastup, &td);
          size_t len = strlen(str);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
          snprintf(str+len, sizeof(str)-len, ", lastup=%04d-%02d-%02d %02d:%02d:%02d",
            td.tm_year+1900, td.tm_mon+1, td.tm_mday,
            td.tm_hour, td.tm_min, td.tm_sec);
#pragma GCC diagnostic pop
        }
        *ostream << " [ZZ=" << str;
        if (message->isPassive()) {
          *ostream << ", passive";
        } else {
          *ostream << ", active";
        }
        if (message->isWrite()) {
          *ostream << " write]";
        } else {
          *ostream << " read]";
        }
      }
    }
    found = true;
  }
  if (!found) {
    return RESULT_ERR_NOTFOUND;
  }
  return RESULT_OK;
}

result_t MainLoop::executeListen(const vector<string>& args, RequestMode* reqMode, ostringstream* ostream) {
  size_t argPos = 1;
  OutputFormat verbosity = OF_NONE;
  bool listenWithUnknown = false;
  bool listenOnlyUnknown = false;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-v") {
      if ((verbosity & VERBOSITY_3) == VERBOSITY_0) {
        verbosity |= VERBOSITY_1;
      } else if ((verbosity & VERBOSITY_3) == VERBOSITY_1) {
        verbosity |= VERBOSITY_2;
      } else {
        verbosity |= VERBOSITY_3;
      }
    } else if (args[argPos] == "-vv") {
      verbosity |= VERBOSITY_2;
    } else if (args[argPos] == "-vvv" || args[argPos] == "-V") {
      verbosity |= VERBOSITY_3;
    } else if (args[argPos] == "-n") {
      verbosity = (verbosity & ~OF_VALUENAME) | OF_NUMERIC;
    } else if (args[argPos] == "-N") {
      verbosity = (verbosity & ~OF_NUMERIC) | OF_VALUENAME;
    } else if (args[argPos] == "-u") {
      listenWithUnknown = true;
      listenOnlyUnknown = false;
    } else if (args[argPos] == "-U") {
      listenOnlyUnknown = true;
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }
  if (argPos > 0 && args.size() == argPos) {
    reqMode->format = verbosity;
    reqMode->listenWithUnknown = listenWithUnknown;
    reqMode->listenOnlyUnknown = listenOnlyUnknown;
    if (listenWithUnknown || listenOnlyUnknown) {
      m_busHandler->enableGrab(true);  // needed for listening to all messages
    }
    if (reqMode->listenMode == lm_listen) {
      *ostream << "listen continued";
      return RESULT_OK;
    }
    reqMode->listenMode = lm_listen;
    *ostream << "listen started";
    return RESULT_OK;
  }

  if (argPos == 0 || args.size() != argPos + 1 || args[argPos] != "stop") {
    *ostream << "usage: listen [-v|-V] [-n|-N] [-u|-U] [stop]\n"
                " Listen for updates or stop it.\n"
                "  -v  increase verbosity (include names/units/comments)\n"
                "  -V  be very verbose (include names, units, and comments)\n"
                "  -n  use numeric value of value=name pairs\n"
                "  -N  use numeric and named value of value=name pairs\n"
                "  -u  include unknown messages\n"
                "  -U  only show unknown messages";
    return RESULT_OK;
  }
  reqMode->listenMode = lm_none;
  *ostream << "listen stopped";
  return RESULT_OK;
}

result_t MainLoop::executeState(const vector<string>& args, ostringstream* ostream) {
  if (args.size() == 0) {
    *ostream << "usage: state\n"
                " Report bus state.";
    return RESULT_OK;
  }
  if (m_protocol->hasSignal()) {
    *ostream << "signal acquired, "
             << m_protocol->getSymbolRate() << " symbols/sec ("
             << m_protocol->getMaxSymbolRate() << " max), "
             << m_protocol->getMasterCount() << " masters";
    return RESULT_OK;
  }
  return RESULT_ERR_NO_SIGNAL;
}

result_t MainLoop::executeGrab(const vector<string>& args, ostringstream* ostream) {
  if (args.size() == 1) {
    *ostream << (m_busHandler->enableGrab(true) ? "grab started" : "grab continued");
    return RESULT_OK;
  }
  if (args.size() == 2 && args[1] == "stop") {
    *ostream << (m_busHandler->enableGrab(false) ? "grab stopped" : "grab not running");
    return RESULT_OK;
  }
  if (args.size() >= 2 && args[1] == "result") {
    bool onlyUnknown = true;
    bool decode = false;
    bool invalid = false;
    for (size_t i = args.size() - 1; i >= 2; i--) {
      string arg = args[i];
      if (arg == "all") {
        onlyUnknown = false;
      } else if (arg == "decode") {
        decode = true;
      } else {
        invalid = true;
        break;
      }
    }
    if (!invalid) {
      m_busHandler->formatGrabResult(onlyUnknown, decode ? OF_DEFINITION : OF_NONE, ostream);
      return RESULT_OK;
    }
  }
  *ostream << "usage: grab [stop]\n"
              "  or:  grab result [all|decode]*\n"
              " Start or stop grabbing, or report/decode unknown or all grabbed messages.";
  return RESULT_OK;
}

result_t MainLoop::executeDefine(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  bool replace = false;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-r") {
      replace = true;
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }

  if (argPos == 0 || args.size() != argPos + 1) {
    *ostream <<
         "usage: define [-r] DEFINITION\n"
         " Define a new message.\n"
         "  -r          replace an already existing definition\n"
         "  DEFINITION  message definition to add";
    return RESULT_OK;
  }

  time_t now;
  time(&now);
  string errorDescription;
  istringstream defstr("#\n" + args[argPos]);  // ensure first line is not used for determining col names
  return m_messages->readFromStream(&defstr, "tcp", now, true, nullptr, &errorDescription, replace);
}


result_t MainLoop::executeDecode(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  OutputFormat verbosity = OF_NONE;
  while (args.size() > argPos && args[argPos][0] == '-') {
    if (args[argPos] == "-v") {
      if ((verbosity & VERBOSITY_3) == VERBOSITY_0) {
        verbosity |= VERBOSITY_1;
      } else if ((verbosity & VERBOSITY_3) == VERBOSITY_1) {
        verbosity |= VERBOSITY_2;
      } else {
        verbosity |= VERBOSITY_3;
      }
    } else if (args[argPos] == "-vv") {
      verbosity |= VERBOSITY_2;
    } else if (args[argPos] == "-vvv" || args[argPos] == "-V") {
      verbosity |= VERBOSITY_3;
    } else if (args[argPos] == "-n") {
      verbosity = (verbosity & ~OF_VALUENAME) | OF_NUMERIC;
    } else if (args[argPos] == "-N") {
      verbosity = (verbosity & ~OF_NUMERIC) | OF_VALUENAME;
    } else {
      argPos = 0;  // print usage
      break;
    }
    argPos++;
  }
  if (args.size() < argPos + 2) {
    argPos = 0;  // print usage
  }

  if (argPos == 0 || args.size() != argPos + 2) {
    *ostream <<
        "usage: decode [-v|-V] [-n|-N] DEFINITION DD[DD]*\n"
        " Decode field(s) by definition and hex data.\n"
        "  -v          increase verbosity (include names/units/comments)\n"
        "  -V          be very verbose (include names, units, and comments)\n"
        "  -n          use numeric value of value=name pairs\n"
        "  -N          use numeric and named value of value=name pairs\n"
        "  DEFINITION  field definition (type,divisor/values,unit,comment,...)\n"
        "  DD          data byte(s) to decode";
    return RESULT_OK;
  }

  time_t now;
  time(&now);
  istringstream defstr("#\n" + args[argPos]);  // ensure first line is not used for determining col names
  string errorDescription;
  DataFieldTemplates* templates = m_scanHelper->getTemplates("*");
  LoadableDataFieldSet fields("", templates);
  result_t ret = fields.readFromStream(&defstr, "temporary", now, true, nullptr, &errorDescription);
  if (ret != RESULT_OK) {
    return ret;
  }
  SlaveSymbolString slave;
  slave.push_back(0);  // dummy length
  ret = slave.parseHex(args[argPos+1]);
  if (ret != RESULT_OK) {
    return ret;
  }
  slave.adjustHeader();
  return fields.read(slave, 0, false, nullptr, -1, verbosity, -1, ostream);
}


result_t MainLoop::executeEncode(const vector<string>& args, ostringstream* ostream) {
  size_t argPos = 1;
  if (argPos == 0 || args.size() != argPos + 2) {
    *ostream <<
        "usage: encode DEFINITION VALUE[;VALUE]*\n"
        " Encode field(s) by definition and decoded value(s).\n"
        "  DEFINITION  field definition (type,divisor/values,unit,comment,...)\n"
        "  VALUE       single field VALUE to encode";
    return RESULT_OK;
  }

  time_t now;
  time(&now);
  istringstream defstr("#\n" + args[argPos]);  // ensure first line is not used for determining col names
  string errorDescription;
  DataFieldTemplates* templates = m_scanHelper->getTemplates("*");
  LoadableDataFieldSet fields("", templates);
  result_t ret = fields.readFromStream(&defstr, "temporary", now, true, nullptr, &errorDescription);
  if (ret != RESULT_OK) {
    return ret;
  }
  istringstream datastr(args[argPos+1]);
  SlaveSymbolString slave;
  ret = fields.write(UI_FIELD_SEPARATOR, 0, &datastr, &slave, nullptr);
  if (ret != RESULT_OK) {
    return ret;
  }
  *ostream << slave.getStr(1);
  return ret;
}


result_t MainLoop::executeScan(const vector<string>& args, const string& levels, ostringstream* ostream) {
  if (args.size() == 1) {
    result_t result = m_busHandler->startScan(false, levels);
    if (result == RESULT_ERR_DUPLICATE) {
      *ostream << "ERR: scan already running";
      return RESULT_OK;
    }
    if (result != RESULT_OK) {
      logError(lf_main, "scan: %s", getResultCode(result));
    }
    return result;
  }

  if (args.size() == 2) {
    if (args[1] == "full") {
      result_t result = m_busHandler->startScan(true, levels);
      if (result != RESULT_OK) {
        logError(lf_main, "full scan: %s", getResultCode(result));
      }
      return result;
    }

    if (args[1] == "result") {
      m_busHandler->formatScanResult(ostream);
      return RESULT_OK;
    }

    if (args[1] == "status") {
      switch (m_scanStatus) {
        case SCAN_STATUS_RUNNING:
          *ostream << "running";
          break;
        case SCAN_STATUS_FINISHED:
          *ostream << "finished";
          break;
        default:
          *ostream << "unused";
          break;
      }
      unsigned int running = m_busHandler->getRunningScans();
      if (running > 0) {
        *ostream << ", some messages pending";
      }
      return RESULT_OK;
    }

    if (!m_protocol->hasSignal()) {
      return RESULT_ERR_NO_SIGNAL;
    }
    result_t result;
    symbol_t dstAddress = (symbol_t)parseInt(args[1].c_str(), 16, 0, 0xff, &result);
    if (result == RESULT_OK && !isValidAddress(dstAddress, false)) {
      result = RESULT_ERR_INVALID_ADDR;
    }
    if (result != RESULT_OK) {
      return result;
    }
    result = m_busHandler->scanAndWait(dstAddress);
    if (result != RESULT_OK) {
      return result;
    }
    if (!m_busHandler->formatScanResult(dstAddress, false, ostream)) {
      return RESULT_EMPTY;
    }
    return RESULT_OK;
  }

  *ostream << "usage: scan [full|ZZ]\n"
              "  or:  scan result\n"
              "  or:  scan status\n"
              " Scan seen slaves, all slaves (full), a single slave (address ZZ), or report scan result or status.";
  return RESULT_OK;
}

result_t MainLoop::executeLog(const vector<string>& args, ostringstream* ostream) {
  if (args.size() == 1) {
    for (int val = 0; val < lf_COUNT; val++) {
      LogFacility facility = (LogFacility)val;
      *ostream << getLogFacilityStr(facility) << ": " << getLogLevelStr(getFacilityLogLevel(facility)) << "\n";
    }
    return RESULT_OK;
  }
  if (args.size() != 3) {
    *ostream << "usage: log [AREA[,AREA]* LEVEL]\n"
        " Set log level for the specified area(s) or get current settings.\n"
        "  AREA   the area to set the level for (main|network|bus|update|other|all)\n"
        "  LEVEL  the log level to set (error|notice|info|debug)";
    return RESULT_OK;
  }
  int facilities = parseLogFacilities(args[1].c_str());
  LogLevel level = parseLogLevel(args[2].c_str());
  if (facilities != -1 && level != ll_COUNT) {
    if (setFacilitiesLogLevel(facilities, level)) {
      return RESULT_OK;
    }
    *ostream << "same";
    return RESULT_OK;
  }
  return RESULT_ERR_INVALID_ARG;
}

result_t MainLoop::executeRaw(const vector<string>& args, ostringstream* ostream) {
  bool bytes = args.size() == 2 && args[1] == "bytes";
  if (args.size() != 1 && !bytes) {
    *ostream << "usage: raw [bytes]\n"
                " Toggle logging of messages or each byte.";
    return RESULT_OK;
  }
  bool enabled = m_protocol->toggleLogRaw(bytes);
  *ostream << (enabled ? "raw logging enabled" : "raw logging disabled");
  return RESULT_OK;
}

result_t MainLoop::executeDump(const vector<string>& args, ostringstream* ostream) {
  if (args.size() != 1) {
    *ostream << "usage: dump\n"
                " Toggle binary dump of received bytes.";
    return RESULT_OK;
  }
  if (!m_protocol->hasDumpFile()) {
    *ostream << "dump not configured";
    return RESULT_OK;
  }
  bool enabled = m_protocol->toggleDump();
  *ostream << (enabled ? "dump enabled" : "dump disabled");
  return RESULT_OK;
}

result_t MainLoop::executeReload(const vector<string>& args, ostringstream* ostream) {
  if (args.size() != 1) {
    *ostream << "usage: reload\n"
                " Reload CSV config files.";
    return RESULT_OK;
  }
  m_busHandler->clear();
  m_scanHelper->loadConfigFiles(!m_scanConfig);
  return RESULT_OK;
}

result_t MainLoop::executeInfo(const vector<string>& args, const string& user, ostringstream* ostream) {
  bool verbose = args.size() == 2 && args[1] == "verbose";
  if (args.size() != 1 && !verbose) {
    *ostream << "usage: info [verbose]\n"
                " Report information about the daemon, configuration, seen participants, and the device.";
    return RESULT_OK;
  }
  *ostream << "version: " << PACKAGE_STRING "." REVISION "\n";
  if (!m_updateCheck.empty()) {
    *ostream << "update check: " << m_updateCheck << "\n";
  }
  *ostream << "device: ";
  m_protocol->formatInfo(ostream, verbose, false);
  *ostream << "\n";
  if (!user.empty()) {
    *ostream << "user: " << user << "\n";
  }
  string levels = getUserLevels(user);
  if (!user.empty() || !levels.empty()) {
    *ostream << "access: " << levels << "\n";
  }
  if (m_protocol->hasSignal()) {
    *ostream << "signal: acquired\n"
             << "symbol rate: " << m_protocol->getSymbolRate() << "\n"
             << "max symbol rate: " << m_protocol->getMaxSymbolRate() << "\n";
    if (m_protocol->getMinArbitrationDelay() >= 0) {
      *ostream << "min arbitration micros: " << m_protocol->getMinArbitrationDelay() << "\n"
               << "max arbitration micros: " << m_protocol->getMaxArbitrationDelay() << "\n";
    }
    if (m_protocol->getMinSymbolLatency() >= 0) {
      *ostream << "min symbol latency: " << m_protocol->getMinSymbolLatency() << "\n"
               << "max symbol latency: " << m_protocol->getMaxSymbolLatency() << "\n";
    }
    if (m_scanStatus != SCAN_STATUS_NONE) {
      *ostream << "scan: " << (m_scanStatus == SCAN_STATUS_FINISHED ? "finished" : "running");
      unsigned int running = m_busHandler->getRunningScans();
      if (running > 0) {
        *ostream << ", some messages pending";
      }
      *ostream << "\n";
    }
  } else {
    *ostream << "signal: no signal\n";
  }
  *ostream << "reconnects: " << m_reconnectCount << "\n"
           << "masters: " << m_protocol->getMasterCount() << "\n"
           << "messages: " << m_messages->size() << "\n"
           << "conditional: " << m_messages->sizeConditional() << "\n"
           << "poll: " << m_messages->sizePoll() << "\n"
           << "update: " << m_messages->sizePassive();
  if (verbose) {
    *ostream << "\nconfig path: " << m_scanHelper->getConfigPath();
  }
  m_busHandler->formatSeenInfo(ostream);
  return RESULT_OK;
}

result_t MainLoop::executeQuit(const vector<string>& args, bool *connected, ostringstream* ostream) {
  if (args.size() == 1) {
    *connected = false;
    *ostream << "connection closed";
    return RESULT_OK;
  }
  *ostream << "usage: quit\n"
              " Close client connection.";
  return RESULT_OK;
}

result_t MainLoop::executeHelp(ostringstream* ostream) {
  *ostream << "usage:\n"
      " read|r    Read value(s):         read [-f] [-m SECONDS] [-s QQ] [-d ZZ] [-c CIRCUIT] [-p PRIO] [-v|-V] [-n|-N]"
      " [-i VALUE[;VALUE]*] NAME [FIELD[.N]]\n"
      "           Read by new defintion: read [-f] [-m SECONDS] [-s QQ] [-d ZZ] [-v|-V] [-n|-N]"
      " [-i VALUE[;VALUE]*] -def DEFINITION (if enabled)\n"
      "           Read hex message:      read [-f] [-m SECONDS] [-s QQ] [-c CIRCUIT] -h ZZPBSBNN[DD]*\n"
      " write|w   Write value(s):        write [-s QQ] [-d ZZ] -c CIRCUIT NAME [VALUE[;VALUE]*]\n"
      "           Write by new def.:     write [-s QQ] [-d ZZ] -def DEFINITION [VALUE[;VALUE]*] (if enabled)\n"
      "           Write hex message:     write [-s QQ] [-c CIRCUIT] -h ZZPBSBNN[DD]*\n"
      " auth|a    Authenticate user:     auth USER SECRET\n"
      " find|f    Find message(s):       find [-v|-V] [-r] [-w] [-p] [-a] [-d] [-h] [-i ID] [-f] [-F COL[,COL]*] [-e]"
      " [-c CIRCUIT] [-l LEVEL] [NAME]\n"
      " listen|l  Listen for updates:    listen [-v|-V] [-n|-N] [-u|-U] [stop]\n"
      " hex       Send hex data:         hex [-s QQ] [-n] ZZPBSB[NN][DD]* (if enabled)\n"
      " inject    Inject hex data:       inject QQZZPBSBNN[DD]*/[NN[DD]*] (if enabled)\n"
      " answer    Answer a message:      answer [-m] [-s QQ] [-d ZZ] PBSB[ID]* [DD]* (if enabled)\n"
      " direct    Enter direct mode\n"
      " state|s   Report bus state\n"
      " info|i    Report information about the daemon, configuration, seen participants, and the device.\n"
      " grab|g    Grab messages:         grab [stop]\n"
      "           Report the messages:   grab result [all|decode]\n"
      " define    Define new message:    define [-r] DEFINITION (if enabled)\n"
      " decode|d  Decode field(s):       decode [-v|-V] [-n|-N] DEFINITION DD[DD]*\n"
      " encode|e  Encode field(s):       encode DEFINITION VALUE[;VALUE]*\n"
      " scan      Scan slaves:           scan [full|ZZ]\n"
      "           Report scan result:    scan result\n"
      "           Report scan status:    scan status\n"
      " log       Set log area level:    log [AREA[,AREA]* LEVEL]\n"
      " raw       Toggle logging of messages or each byte.\n"
      " dump      Toggle binary dump of received bytes\n"
      " reload    Reload CSV config files\n"
      " quit|q    Close connection\n"
      " help|?    Print help             help [COMMAND], COMMMAND ?";
  return RESULT_OK;
}

/**
 * Parse a boolean query value.
 * @param value the query value.
 * @return the parsed boolean.
 */
bool parseBoolQuery(const string& value) {
  return value.length() == 0 || value == "1" || value == "true";
}

result_t MainLoop::executeGet(const vector<string>& args, bool* connected, ostringstream* ostream) {
  time_t maxAge = -1;
  size_t argPos = 1;
  string uri = args[argPos++];
  int type = -1;
  result_t ret = RESULT_OK;
  if (uri.substr(0, 5) == "/data" && (uri.length() == 5 || uri[5] == '/')) {
    string circuit, name;
    size_t pos = uri.find('/', 6);
    if (pos == string::npos) {
      circuit = uri.length() == 5 ? "" : uri.substr(6);
    } else {
      circuit = uri.substr(6, pos - 6);
      name = uri.substr(pos + 1);
    }
    bool required = false, full = false, withWrite = false, raw = false;
    bool withDefinition = false;
    string newDefinition;
    OutputFormat verbosity = OF_NAMES;
    time_t since = 0;
    size_t pollPriority = 0;
    bool exact = false;
    string user;
    if (args.size() > argPos) {
      string secret;
      string query = args[argPos];
      istringstream stream(query);
      string token;
      while (getline(stream, token, '&')) {
        pos = token.find('=');
        string qname, value;
        if (pos != string::npos) {
          qname = token.substr(0, pos);
          value = token.substr(pos + 1);
        } else {
          qname = token;
        }
        if (qname == "since") {
          since = parseInt(value.c_str(), 10, 0, 0xffffffff, &ret);
        } else if (qname == "poll") {
          pollPriority = (size_t)parseInt(value.c_str(), 10, 1, 9, &ret);
        } else if (qname == "exact") {
          exact = parseBoolQuery(value);
        } else if (qname == "verbose") {
          if (parseBoolQuery(value)) {
            verbosity |= OF_UNITS | OF_COMMENTS;
          }
        } else if (qname == "indexed") {
          if (parseBoolQuery(value)) {
            verbosity &= ~OF_NAMES;
          }
        } else if (qname == "numeric") {
          if (parseBoolQuery(value)) {
            verbosity = (verbosity & ~OF_VALUENAME) | OF_NUMERIC;
          }
        } else if (qname == "valuename") {
          if (parseBoolQuery(value)) {
            verbosity = (verbosity & ~OF_NUMERIC) | OF_VALUENAME;
          }
        } else if (qname == "full") {
          full = parseBoolQuery(value);
        } else if (qname == "required") {
          required = parseBoolQuery(value);
        } else if (qname == "maxage") {
          maxAge = parseInt(value.c_str(), 10, 0, 24*60*60, &ret);
          required = true;
        } else if (qname == "write") {
          withWrite = parseBoolQuery(value);
        } else if (qname == "raw") {
          raw = parseBoolQuery(value);
        } else if (qname == "def") {
          withDefinition = parseBoolQuery(value);
        } else if (qname == "define") {
          if (!m_newlyDefinedMessages || circuit.empty() || name.empty() || value.empty()) {
            ret = RESULT_ERR_INVALID_ARG;
            break;
          }
          size_t comma = value.find(',');
          if (comma == string::npos || comma == 0
          || value.find(circuit+","+name+",") != comma+1) {  // ensure same circuit+name
            ret = RESULT_ERR_INVALID_ARG;
            break;
          }
          newDefinition = value;
        } else if (qname == "user") {
          user = value;
        } else if (qname == "secret") {
          secret = value;
        }
        if (ret != RESULT_OK) {
          break;
        }
      }
      if ((!user.empty() || !secret.empty()) && !m_userList.checkSecret(user, secret)) {
        ret = RESULT_ERR_NOTAUTHORIZED;
      }
    }

    *ostream << "{";
    string lastCircuit;
    time_t now;
    time(&now);
    time_t maxLastUp = 0;
    if (ret == RESULT_OK && !newDefinition.empty()) {
      string errorDescription;
      istringstream defstr("#\n" + newDefinition);  // ensure first line is not used for determining col names
      ret = m_messages->readFromStream(&defstr, "http", now, true, nullptr, &errorDescription, true);
    }
    if (ret == RESULT_OK) {
      bool first = true;
      verbosity |= OF_JSON | (full ? OF_ALL_ATTRS : OF_NONE) | (withDefinition ? OF_DEFINITION : OF_NONE);
      deque<Message*> messages;
      m_messages->findAll(circuit, name, getUserLevels(user), exact, true, withWrite, true, true, true, 0, 0, false,
                          &messages);
      string lastName;
      for (deque<Message*>::iterator it = messages.begin(); it != messages.end(); it++) {
        Message* message = *it;
        symbol_t dstAddress = message->getDstAddress();
        if (dstAddress == SYN) {
          continue;
        }
        if (pollPriority > 0 && message->setPollPriority(pollPriority)) {
          m_messages->addPollMessage(false, message);
        }
        time_t lastup = message->getLastUpdateTime();
        if (required && (lastup == 0 || (maxAge >=0 && lastup + maxAge <= now))) {
          // read directly from bus
          if (message->isPassive()) {
            continue;  // not possible to actively read this message
          }
          if (m_busHandler->readFromBus(message, "") != RESULT_OK) {
            continue;
          }
        } else {
          if (since > 0 && lastup <= since) {
            continue;
          }
          if (lastup > maxLastUp) {
            maxLastUp = lastup;
          }
        }
        bool sameCircuit = message->getCircuit() == lastCircuit;
        if (!sameCircuit) {
          if (lastCircuit.length() > 0) {
            *ostream << "\n  }\n },";
          }
          lastCircuit = message->getCircuit();
          *ostream << "\n \"" << lastCircuit << "\": {";
          if (full && m_messages->decodeCircuit(lastCircuit, verbosity, ostream)) {  // add circuit specific values
            *ostream << ",";
          }
          *ostream << "\n  \"messages\": {";
          lastName = "";
          first = true;
        }
        name = message->getName();
        bool same = sameCircuit && name == lastName;
        if (!same && it+1 != messages.end()) {
          Message* next = *(it+1);
          same = next->getCircuit() == lastCircuit && next->getName() == name;
        }
        message->decodeJson(!first, same, true, raw, verbosity, ostream);
        lastName = name;
        first = false;
      }

      if (lastCircuit.length() > 0) {
        *ostream << "\n  }\n },";
      }
      *ostream << "\n \"global\": {"
               << "\n  \"version\": \"" << PACKAGE_VERSION "." REVISION "\"" << setw(0) << dec;
      if (!m_updateCheck.empty()) {
        *ostream << ",\n  \"updatecheck\": \"" << m_updateCheck << "\"";
      }
      if (!user.empty()) {
        *ostream << ",\n  \"user\": \"" << user << "\"";
      }
      string levels = getUserLevels(user);
      if (!user.empty() || !levels.empty()) {
        *ostream << ",\n  \"access\": \"" << levels << "\"";
      }
      *ostream << ",\n  \"signal\": " << (m_protocol->hasSignal() ? "true" : "false");
      if (m_protocol->hasSignal()) {
        *ostream << ",\n  \"symbolrate\": " << m_protocol->getSymbolRate()
                 << ",\n  \"maxsymbolrate\": " << m_protocol->getMaxSymbolRate();
        if (m_protocol->getMinArbitrationDelay() >= 0) {
          *ostream << ",\n  \"minarbitrationmicros\": " << m_protocol->getMinArbitrationDelay()
                   << ",\n  \"maxarbitrationmicros\": " << m_protocol->getMaxArbitrationDelay();
        }
        if (m_protocol->getMinSymbolLatency() >= 0) {
          *ostream << ",\n  \"minsymbollatency\": " << m_protocol->getMinSymbolLatency()
                   << ",\n  \"maxsymbollatency\": " << m_protocol->getMaxSymbolLatency();
        }
      }
      if (!m_protocol->isReadOnly()) {
        *ostream << ",\n  \"qq\": " << static_cast<unsigned>(m_address);
      }
      *ostream << ",\n  \"reconnects\": " << m_reconnectCount
               << ",\n  \"masters\": " << m_protocol->getMasterCount()
               << ",\n  \"messages\": " << m_messages->size()
               << ",\n  \"lastup\": " << static_cast<unsigned>(maxLastUp)
               << "\n }"
               << "\n}";
      type = 6;
    }
    *connected = false;
    return formatHttpResult(ret, type, ostream);
  }  // request for "/data..."

  if (uri == "/datatypes") {
    *ostream << "[";
    OutputFormat verbosity = OF_NAMES|OF_JSON|OF_ALL_ATTRS;
    DataTypeList::getInstance()->dump(verbosity, true, ostream);
    *ostream << "\n]";
    type = 6;
    *connected = false;
    return formatHttpResult(ret, type, ostream);
  }

  if (uri == "/templates" || uri.substr(0, 11) == "/templates/") {
    *ostream << "[";
    OutputFormat verbosity = OF_NAMES|OF_JSON|OF_ALL_ATTRS;
    string name = uri == "/templates" ? "" : uri.substr(11) + "/";
    const auto tmpl = m_scanHelper->getTemplates(name);
    tmpl->dump(verbosity, ostream);
    *ostream << "\n]";
    type = 6;
    *connected = false;
    return formatHttpResult(ret, type, ostream);
  }

  if (uri == "/raw") {
    time_t since = 0, until = 0;
    bool onlyUnknown = false;
    if (args.size() > argPos) {
      string query = args[argPos];
      istringstream stream(query);
      string token;
      while (getline(stream, token, '&')) {
        size_t pos = token.find('=');
        string qname, value;
        if (pos != string::npos) {
          qname = token.substr(0, pos);
          value = token.substr(pos + 1);
        } else {
          qname = token;
        }
        if (qname == "since") {
          since = parseInt(value.c_str(), 10, 0, 0xffffffff, &ret);
        } else if (qname == "unknown") {
          onlyUnknown = parseBoolQuery(value);
        }
        if (ret != RESULT_OK) {
          break;
        }
      }
    }
    if (ret == RESULT_OK) {
      *ostream << "[";
      if (since > 0) {
        time_t now;
        time(&now);
        until = now-1;
      }
      m_busHandler->formatGrabResult(onlyUnknown, OF_JSON, ostream, false, since, until);
      *ostream << "\n]";
      type = 6;
    }
    *connected = false;
    return formatHttpResult(ret, type, ostream);
  }

  if (uri == "/decode") {
    string def;
    string raw;
    if (args.size() > argPos) {
      string query = args[argPos];
      istringstream stream(query);
      string token;
      while (getline(stream, token, '&')) {
        size_t pos = token.find('=');
        string qname, value;
        if (pos != string::npos) {
          qname = token.substr(0, pos);
          value = token.substr(pos + 1);
        } else {
          qname = token;
        }
        if (qname == "def") {
          def = value;
        } else if (qname == "raw") {
          raw = value;
        } else {
          ret = RESULT_ERR_INVALID_ARG;
        }
        if (ret != RESULT_OK) {
          break;
        }
      }
    }
    if (def.empty() || raw.empty()) {
      ret = RESULT_ERR_INVALID_ARG;
    } else if (ret == RESULT_OK) {
      time_t now;
      time(&now);
      istringstream defstr("#\n" + def);  // ensure first line is not used for determining col names
      string errorDescription;
      DataFieldTemplates* templates = m_scanHelper->getTemplates("*");
      LoadableDataFieldSet fields("", templates);
      ret = fields.readFromStream(&defstr, "temporary", now, true, nullptr, &errorDescription);
      if (ret == RESULT_OK && fields.size()) {
        SlaveSymbolString slave;
        slave.push_back(0);  // dummy length
        ret = slave.parseHex(raw);
        const SingleDataField* field = fields[0];
        if (ret == RESULT_OK && field) {
          slave.adjustHeader();
          ret = field->read(slave, 0, false, nullptr, 0, OF_JSON|OF_SHORT, 0, ostream);
        }
      } else {
        ret = RESULT_ERR_INVALID_ARG;
      }
      type = 6;
    }
    *connected = false;
    return formatHttpResult(ret, type, ostream);
  }

  if (uri.length() < 1 || uri[0] != '/' || uri.find("//") != string::npos || uri.find("..") != string::npos) {
    ret = RESULT_ERR_INVALID_ARG;
  } else {
    string filename = m_htmlPath + uri;
    if (uri[uri.length() - 1] == '/') {
      filename += "index.html";
    }
    size_t pos = filename.find_last_of('.');
    if (pos != string::npos && pos != filename.length() - 1 && pos >= filename.length() - 5) {
      string ext = filename.substr(pos + 1);
      if (ext == "html") {
        type = 0;
      } else if (ext == "css") {
        type = 1;
      } else if (ext == "js") {
        type = 2;
      } else if (ext == "png") {
        type = 3;
      } else if (ext == "jpg" || ext == "jpeg") {
        type = 4;
      } else if (ext == "svg") {
        type = 5;
      } else if (ext == "json") {
        type = 6;
      } else if (ext == "yaml") {
        type = 7;
      } else if (ext == "csv") {
        type = 8;
      }
    }
    if (type < 0) {
      ret = RESULT_ERR_NOTFOUND;
    } else {
      ifstream ifs;
      ifs.open(filename.c_str(), ifstream::in | ifstream::binary);
      if (!ifs.is_open()) {
        ret = RESULT_ERR_NOTFOUND;
      } else {
        ifs >> ostream->rdbuf();
        ifs.close();
      }
    }
  }
  *connected = false;
  return formatHttpResult(ret, type, ostream);
}

result_t MainLoop::formatHttpResult(result_t ret, int type, ostringstream* ostream) {
  string data = ret == RESULT_OK ? ostream->str() : "";
  ostream->str("");
  ostream->clear();
  *ostream << "HTTP/1.0 ";
  switch (ret) {
  case RESULT_OK:
    *ostream << "200 OK\r\nContent-Type: ";
    switch (type) {
    case 1:
      *ostream << "text/css";
      break;
    case 2:
      *ostream << "application/javascript";
      break;
    case 3:
      *ostream << "image/png";
      break;
    case 4:
      *ostream << "image/jpeg";
      break;
    case 5:
      *ostream << "image/svg+xml";
      break;
    case 6:
      *ostream << "application/json;charset=utf-8";
      break;
    case 7:
      *ostream << "application/yaml;charset=utf-8";
      break;
    case 9:
      *ostream << "text/comma-separated-values";
      break;
    default:
      *ostream << "text/html";
      break;
    }
    *ostream << "\r\nContent-Length: " << setw(0) << dec << static_cast<unsigned>(data.length());
    break;
  case RESULT_ERR_NOTFOUND:
    *ostream << "404 Not Found";
    break;
  case RESULT_ERR_INVALID_ARG:
  case RESULT_ERR_INVALID_NUM:
  case RESULT_ERR_INVALID_POS:
  case RESULT_ERR_OUT_OF_RANGE:
  case RESULT_ERR_INVALID_PART:
  case RESULT_ERR_MISSING_ARG:
  case RESULT_ERR_INVALID_LIST:
    *ostream << "400 Bad Request";
    break;
  case RESULT_ERR_NOTAUTHORIZED:
    *ostream << "403 Forbidden";
    break;
  default:
    *ostream << "500 Internal Server Error";
    break;
  }
  *ostream << "\r\nServer: " PACKAGE_NAME "/" PACKAGE_VERSION "\r\n\r\n" << data;
  return RESULT_OK;
}

}  // namespace ebusd

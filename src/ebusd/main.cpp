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

#include "ebusd/main.h"
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <csignal>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <map>
#include <vector>
#include "ebusd/bushandler.h"
#include "ebusd/mainloop.h"
#include "ebusd/network.h"
#include "lib/utils/log.h"
#include "lib/utils/httpclient.h"
#include "lib/utils/tcpsocket.h"
#include "ebusd/scan.h"

namespace ebusd {

using std::cout;

/** the previous config path suffixes to rewrite to the current one. */
#define PREVIOUS_CONFIG_PATH_SUFFIXES {"ebusd.eu/config/", "cfg.ebusd.eu/"}

/** the opened PID file, or nullptr. */
static FILE* s_pidFile = nullptr;

/** the program options. */
static struct options s_opt;

/** the @a MessageMap instance, or nullptr. */
static MessageMap* s_messageMap = nullptr;

/** the @a ScanHelper instance, or nullptr. */
static ScanHelper* s_scanHelper = nullptr;

/** the @a ProtocolHandler instance, or nullptr. */
static ProtocolHandler* s_protocol = nullptr;

/** the @a BusHandler instance, or nullptr. */
static BusHandler* s_busHandler = nullptr;

/** the @a Request @a Queue instance, or nullptr. */
static Queue<Request*>* s_requestQueue = nullptr;

/** the @a MainLoop instance, or nullptr. */
static MainLoop* s_mainLoop = nullptr;

/** the @a Network instance, or nullptr. */
static Network* s_network = nullptr;

void shutdown(bool error = false);

void daemonize() {
  // fork off the parent process
  pid_t pid = fork();

  if (pid < 0) {
    logError(lf_main, "fork() failed, exiting");
    exit(EXIT_FAILURE);
  }

  // If we got a good PID, then we can exit the parent process
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  // At this point we are executing as the child process

  // Create a new SID for the child process and
  // detach the process from the parent (normally a shell)
  if (setsid() < 0) {
    logError(lf_main, "setsid() failed, exiting");
    exit(EXIT_FAILURE);
  }

  // Change the current working directory. This prevents the current
  // directory from being locked; hence not being able to remove it.
  if (chdir("/tmp") < 0) {
    logError(lf_main, "daemon chdir() failed, exiting");
    exit(EXIT_FAILURE);
  }

  // Close stdin, stdout and stderr
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // create pid file and try to lock it
  s_pidFile = fopen(s_opt.pidFile, "w+");

  umask(S_IWGRP | S_IRWXO);  // set permissions of newly created files to 750

  if (s_pidFile != nullptr) {
    setbuf(s_pidFile, nullptr);  // disable buffering
    if (lockf(fileno(s_pidFile), F_TLOCK, 0) < 0
      || fprintf(s_pidFile, "%d\n", getpid())  <= 0) {
      fclose(s_pidFile);
      s_pidFile = nullptr;
    }
  }
  if (s_pidFile == nullptr) {
    logError(lf_main, "can't open pidfile: %s, exiting", s_opt.pidFile);
    shutdown(true);
  }
}

/**
 * Clean up all dynamically allocated and stop main loop and all dependent components.
 */
void cleanup() {
  if (s_network != nullptr) {
    delete s_network;
    s_network = nullptr;
  }
  if (s_mainLoop != nullptr) {
    delete s_mainLoop;
    s_mainLoop = nullptr;
  }
  if (s_requestQueue != nullptr) {
    Request* msg;
    while ((msg = s_requestQueue->pop()) != nullptr) {
      delete msg;
    }
    delete s_requestQueue;
    s_requestQueue = nullptr;
  }
  if (s_protocol != nullptr) {
    delete s_protocol;
    s_protocol = nullptr;
  }
  if (s_busHandler != nullptr) {
    delete s_busHandler;
    s_busHandler = nullptr;
  }
  if (s_messageMap != nullptr) {
    delete s_messageMap;
    s_messageMap = nullptr;
  }
  if (s_scanHelper != nullptr) {
    delete s_scanHelper;
    s_scanHelper = nullptr;
  }
}

/**
 * Clean up resources and shutdown.
 */
void shutdown(bool error) {
  cleanup();

  // reset all signal handlers to default
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  // close and delete pid file if necessary
  if (s_pidFile != nullptr) {
    if (fclose(s_pidFile) == 0) {
      remove(s_opt.pidFile);
    }
    s_pidFile = nullptr;
  }

  logNotice(lf_main, "ebusd stopped");
  closeLogFile();

  exit(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

/**
 * The signal handling function.
 * @param sig the received signal.
 */
void signalHandler(int sig) {
  switch (sig) {
  case SIGHUP:
    logNotice(lf_main, "SIGHUP received");
    if (!s_opt.foreground && s_opt.logFile && s_opt.logFile[0] != 0) {  // for log file rotation
      closeLogFile();
      setLogFile(s_opt.logFile);
    }
    break;
  case SIGINT:
    logNotice(lf_main, "SIGINT received");
    if (s_mainLoop) {
      s_mainLoop->shutdown();
    } else {
      shutdown();
    }
    break;
  case SIGTERM:
    logNotice(lf_main, "SIGTERM received");
    if (s_mainLoop) {
      s_mainLoop->shutdown();
    } else {
      shutdown();
    }
    break;
  default:
    logNotice(lf_main, "undefined signal %s", strsignal(sig));
    break;
  }
}


/**
 * Main function.
 * @param argc the number of command line arguments.
 * @param argv the command line arguments.
 * @param envp the environment variables.
 * @return the exit code.
 */
int main(int argc, char* argv[], char* envp[]) {
  switch (parse_main_args(argc, argv, envp, &s_opt)) {
    case 0:  // OK
      break;
    case '?':  // help printed
      return 0;
    case 'V':
      printf("" PACKAGE_STRING "." REVISION "\n");
      return 0;
    default:
      logWrite(lf_main, ll_error, "invalid arguments");  // force logging on exit
      return EINVAL;
  }

  if (s_opt.logAreas != -1 || s_opt.logLevel != ll_COUNT) {
    setFacilitiesLogLevel(1 << lf_COUNT, ll_none);
    setFacilitiesLogLevel(s_opt.logAreas, s_opt.logLevel);
  }

  const string lang = MappedFileReader::normalizeLanguage(
    s_opt.preferLanguage == nullptr || !s_opt.preferLanguage[0] ? "" : s_opt.preferLanguage);
  string configLocalPrefix, configUriPrefix, configLangQuery;
#ifdef HAVE_SSL
  HttpClient::initialize(s_opt.caFile, s_opt.caPath);
#else  // HAVE_SSL
  HttpClient::initialize(nullptr, nullptr);
#endif  // HAVE_SSL
  HttpClient* configHttpClient = nullptr;
  string configPath = s_opt.configPath;
  if (configPath.find("://") == string::npos) {
    configLocalPrefix = s_opt.configPath;
  } else {
    if (!s_opt.scanConfig && s_opt.dumpConfig == OF_NONE) {
      logWrite(lf_main, ll_error, "invalid configpath without scanconfig");  // force logging on exit
      return EINVAL;
    }
    for (auto prevSuffix : PREVIOUS_CONFIG_PATH_SUFFIXES) {
      size_t pos = configPath.find(prevSuffix);
      if (pos == string::npos || (pos >= 3 && configPath.substr(pos-3, 3) != "://")) {
        continue;
      }
      configPath = CONFIG_PATH;
      logNotice(lf_main, "replaced old configPath %s with new one: %s", s_opt.configPath, configPath.c_str());
      break;
    }
    uint16_t configPort = 443;
    string proto, configHost;
    if (!HttpClient::parseUrl(configPath, &proto, &configHost, &configPort, &configUriPrefix)) {
#ifndef HAVE_SSL
      if (proto == "https") {
        logWrite(lf_main, ll_error, "invalid configPath URL (HTTPS not supported)");  // force logging on exit
        return EINVAL;
      }
#endif  // HAVE_SSL
      logWrite(lf_main, ll_error, "invalid configPath URL");  // force logging on exit
      return EINVAL;
    }
    bool isCdn = configHost == CONFIG_HOST;
    string suffix;
    if (isCdn) {
      suffix = (lang != "en" && lang != "tt" ? "de" : lang) + "/";
      configUriPrefix += suffix;
      configPath += suffix;  // only for informational purposes
    } else {
      configLangQuery = lang.empty() ? lang : "?l=" + lang;
    }
    configHttpClient = new HttpClient();
    if (
      !configHttpClient->connect(configHost, configPort, proto == "https", PACKAGE_NAME "/" PACKAGE_VERSION)
      // if that did not work, issue a single retry with higher timeout:
      && !configHttpClient->connect(configHost, configPort, proto == "https", PACKAGE_NAME "/" PACKAGE_VERSION, 8)
    ) {
      logWrite(lf_main, ll_error, "invalid configPath URL (connect)");  // force logging on exit
      delete configHttpClient;
      cleanup();
      return EINVAL;
    }
    if (isCdn) {
      // check load balancing redirection
      string redirPath;
      uint16_t redirPort = 443;
      string redirProto, redirHost, redirUriPrefix;
      bool json = true;
      if (configHttpClient->get("/redirect.json", "", &redirPath, nullptr, nullptr, &json) && !json
      && HttpClient::parseUrl(redirPath, &redirProto, &redirHost, &redirPort, &redirUriPrefix)
      && redirHost != configHost) {
        configHttpClient->disconnect();
        ebusd::HttpClient* redirClient = new HttpClient();
        if (redirClient->connect(redirHost, redirPort, redirProto == "https", PACKAGE_NAME "/" PACKAGE_VERSION)) {
          configUriPrefix = redirUriPrefix + suffix;
          configPath = redirPath + suffix;  // only for informational purposes
          delete configHttpClient;
          configHttpClient = redirClient;
          logNotice(lf_main, "configPath URL redirected to %s", configPath.c_str());
        }
      }
    }
    logInfo(lf_main, "configPath URL is valid");
    configHttpClient->disconnect();
  }

  s_messageMap = new MessageMap(s_opt.checkConfig, lang);
  s_scanHelper = new ScanHelper(s_messageMap, configPath, configLocalPrefix, configUriPrefix,
    configLangQuery, configHttpClient, s_opt.checkConfig);
  s_messageMap->setResolver(s_scanHelper);
  if (s_opt.checkConfig) {
    logNotice(lf_main, PACKAGE_STRING "." REVISION " performing configuration check...");

    result_t result = s_scanHelper->loadConfigFiles(!s_opt.scanConfig || s_opt.injectCount <= 0);
    result_t overallResult = s_scanHelper->executeInstructions(nullptr);
    MasterSymbolString master;
    SlaveSymbolString slave;
    int arg_index = argc - s_opt.injectCount;
    while (result == RESULT_OK && s_opt.scanConfig && arg_index < argc) {
      // check scan config for each passed ident message
      if (!s_scanHelper->parseMessage(argv[arg_index++], true, &master, &slave)) {
        continue;
      }
      symbol_t address = master[1];
      Message* message = s_messageMap->getScanMessage(address);
      if (!message) {
        logError(lf_main, "invalid scan address %2.2x", address);
        if (overallResult == RESULT_OK) {
          overallResult = RESULT_ERR_INVALID_ADDR;
        }
      } else {
        message->storeLastData(master, slave);
        string file;
        result_t res = s_scanHelper->loadScanConfigFile(address, &file);
        result_t instrRes = s_scanHelper->executeInstructions(nullptr);
        if (res == RESULT_OK) {
          logInfo(lf_main, "scan config %2.2x: file %s loaded", address, file.c_str());
        } else if (overallResult == RESULT_OK) {
          overallResult = res;
        }
        if (overallResult == RESULT_OK && instrRes != RESULT_OK) {
          overallResult = instrRes;
        }
      }
    }
    if (result != RESULT_OK) {
      overallResult = result;
    }
    if (result == RESULT_OK && s_opt.dumpConfig) {
      ostream* out = &cout;
      std::ofstream fout;
      if (s_opt.dumpConfigTo) {
        fout.open(s_opt.dumpConfigTo);
        if (!fout.is_open()) {
          logError(lf_main, "error dumping config to %s", s_opt.dumpConfigTo);
        } else {
          out = &fout;
        }
      }
      if (fout.is_open()) {
        logNotice(lf_main, "saving configuration dump to %s", s_opt.dumpConfigTo);
      } else {
        logNotice(lf_main, "configuration dump:");
      }
      if (s_opt.dumpConfig & OF_JSON) {
        *out << "{\"datatypes\":[";
        DataTypeList::getInstance()->dump(s_opt.dumpConfig, out);
        *out << "],\"templates\":[";
        const auto tmpl = s_scanHelper->getTemplates("");
        tmpl->dump(s_opt.dumpConfig, out);
        *out << "],\"messages\":";
        s_messageMap->dump(true, s_opt.dumpConfig, out);
        *out << "}";
      } else {
        s_messageMap->dump(true, s_opt.dumpConfig, out);
      }
      if (fout.is_open()) {
        fout.close();
      }
    }
    if (overallResult == RESULT_OK && s_messageMap->getMaxIdLength() > 7) {
      logNotice(lf_main, "max message ID length exceeded");
      overallResult = RESULT_CONTINUE;
    }
    cleanup();
    return overallResult == RESULT_OK ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  const char* device = s_opt.device;
  if (!s_opt.checkConfig && strncmp(device, "mdns:", 5) == 0) {
    // auto discovery
    mdns_oneshot_t address;
    #define MAX_ADDRESSES 10
    mdns_oneshot_t addresses[MAX_ADDRESSES];
    size_t otherCount = MAX_ADDRESSES;
    int ret = 0;
    #define MAX_MDNS_RUNS 3
    for (int i=0; i < MAX_MDNS_RUNS && ret == 0; i++) {  // 3*up to 5 seconds = 15 seconds max
      logWrite(lf_main, ll_notice, "discovering device from \"%s\", try %d/%d", device, i+1, MAX_MDNS_RUNS);
      ret = resolveMdnsOneShot(device+5, &address, addresses, &otherCount);
    }
    if (ret < 0) {
      logWrite(lf_main, ll_error, "unable to discover device \"%s\", error %d", device, ret);
      cleanup();
      return EINVAL;
    }
    const char *ip;
    for (size_t pos=0; pos < otherCount; pos++) {
      mdns_oneshot_t *addr = addresses+pos;
      ip = inet_ntoa(addr->address);
      logWrite(lf_main, ll_info, "discovered another device with ID %s and device string %s:%s",
        addr->id, addr->proto, ip);
    }
    if (ret == 0) {
      logWrite(lf_main, ll_error, "unable to discover device \"%s\", %s found", device, otherCount ? "ID not" : "none");
      cleanup();
      return EINVAL;
    }
    if (ret > 1) {
      ip = inet_ntoa(address.address);
      logWrite(lf_main, ll_info, "discovered another device with ID %s and device string %s:%s",
        address.id, address.proto, ip);
      logWrite(lf_main, ll_error,
        "found several devices from \"%s\". use e.g. \"--device=mdns:%s\" to limit it to the desired one", device,
        address.id);
      cleanup();
      return EINVAL;
    }
    ip = inet_ntoa(address.address);
    char *mdnsDevice = reinterpret_cast<char*>(malloc(4*4+3+1));  // ens:xxx.xxx.xxx.xxx
    snprintf(mdnsDevice, 4*4+3+1, "%s:%s", address.proto, ip);
    device = mdnsDevice;
    logWrite(lf_main, ll_notice, "using discovered device with ID %s and device string %s", address.id, device);
  }

  s_busHandler = new BusHandler(s_messageMap, s_scanHelper, s_opt.pollInterval);

  // create the protocol and open the device
  ebus_protocol_config_t config = {
    .device = device,
    .noDeviceCheck = s_opt.noDeviceCheck,
    .readOnly = s_opt.readOnly,
    .extraLatency = s_opt.extraLatency,
    .ownAddress = s_opt.address,
    .answer = s_opt.answer,
    .busLostRetries = s_opt.acquireRetries,
    .failedSendRetries = s_opt.sendRetries,
    .busAcquireTimeout = s_opt.acquireTimeout,
    .slaveRecvTimeout = s_opt.receiveTimeout,
    .lockCount = s_opt.masterCount,
    .generateSyn = s_opt.generateSyn,
    .initialSend = s_opt.initialSend,
  };
  s_protocol = ProtocolHandler::create(config, s_busHandler);
  if (s_protocol == nullptr) {
    logWrite(lf_main, ll_error, "unable to create protocol/device %s", config.device);  // force logging on exit
    cleanup();
    return EINVAL;
  }
  s_busHandler->setProtocol(s_protocol);
  if (s_opt.answer) {
    istringstream input;
    input.str("ebusd.eu;" PACKAGE_NAME ";" SCAN_VERSION ";100");
    Message* message = s_messageMap->getScanMessage();
    SlaveSymbolString response;
    if (message && message->prepareSlave(&input, &response) == RESULT_OK) {
      s_protocol->setAnswer(SYN, s_protocol->getOwnSlaveAddress(), message->getPrimaryCommand(),
      message->getSecondaryCommand(), nullptr, 0, response);
    }
  }

  if (!s_opt.foreground) {
    if (!setLogFile(s_opt.logFile)) {
      logWrite(lf_main, ll_error, "unable to open log file %s", s_opt.logFile);  // force logging on exit
      cleanup();
      return EINVAL;
    }
    daemonize();  // make daemon
  }

  // trap signals that we expect to receive
  signal(SIGHUP, signalHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  if (s_opt.dumpFile[0]) {
    s_protocol->setDumpFile(s_opt.dumpFile, s_opt.dumpSize, s_opt.dumpFlush);
    if (s_opt.dump) {
      s_protocol->toggleDump();
    }
  }
  if (s_opt.logRawFile[0] && strcmp(s_opt.logRawFile, s_opt.logFile) != 0) {
    s_protocol->setLogRawFile(s_opt.logRawFile, s_opt.logRawSize);
  }
  if (s_opt.logRaw != 0) {
    s_protocol->toggleLogRaw(s_opt.logRaw == 2);
  }

  // open Device
  s_protocol->open();

  // create the MainLoop
  s_requestQueue = new Queue<Request*>();
  s_mainLoop = new MainLoop(s_opt, s_busHandler, s_messageMap, s_scanHelper, s_requestQueue);

  ostringstream ostream;
  s_protocol->formatInfo(&ostream, false, true);
  string deviceInfoStr = ostream.str();
  logNotice(lf_main, PACKAGE_STRING "." REVISION " started%s on device: %s",
      s_opt.scanConfig ? s_opt.initialScan == ESC ? " with auto scan"
      : s_opt.initialScan == BROADCAST ? " with broadcast scan" : s_opt.initialScan == SYN ? " with full scan"
      : " with single scan" : "",
      deviceInfoStr.c_str());

  // load configuration files
  s_scanHelper->loadConfigFiles(!s_opt.scanConfig);

  // start the MainLoop
  if (s_opt.injectCommands) {
    int scanAdrCount = 0;
    bool scanAddresses[256] = {};
    for (int arg_index = argc - s_opt.injectCount; arg_index < argc; arg_index++) {
      string arg = argv[arg_index];
      if (arg.empty()) {
        continue;
      }
      if (arg.find_first_of(' ') != string::npos || arg.find_first_of('/') == string::npos) {
        RequestImpl req(false);
        req.add(argv[arg_index]);
        bool connected = true;
        RequestMode reqMode = {};
        string user;
        bool reload = false;
        ostringstream ostream;
        result_t ret = s_mainLoop->decodeRequest(&req, &connected, &reqMode, &user, &reload, &ostream);
        string output = ostream.str();
        if (ret != RESULT_OK || output.substr(0, 3) == "ERR" || output.substr(0, 5) == "usage") {
          if (output.empty()) {
            output = getResultCode(ret);
          }
          logError(lf_main, "executing command \"%s\" failed: %s", argv[arg_index], output.c_str());
        } else if (s_opt.stopAfterInject) {
          logNotice(lf_main, "executed command \"%s\": %s", argv[arg_index], output.c_str());
        }
        continue;
      }
      // old style inject message
      MasterSymbolString master;
      SlaveSymbolString slave;
      if (!s_scanHelper->parseMessage(arg, false, &master, &slave)) {
        continue;
      }
      s_protocol->injectMessage(master, slave);
      if (s_opt.scanConfig && master.size() >= 5 && master[4] == 0 && master[2] == 0x07 && master[3] == 0x04
        && isValidAddress(master[1], false) && !isMaster(master[1]) && !scanAddresses[master[1]]) {
        // scan message, simulate scanning
        scanAddresses[master[1]] = true;
        scanAdrCount++;
      }
    }
    s_protocol->start("bushandler");
    for (symbol_t address = 0; scanAdrCount > 0; address++) {
      if (scanAddresses[address]) {
        scanAdrCount--;
        s_busHandler->scanAndWait(address, true);
      }
    }
    if (s_opt.stopAfterInject) {
      shutdown();
      return 0;
    }
  } else {
    s_protocol->start("bushandler");
  }
  s_mainLoop->start("mainloop");

  s_network = new Network(s_opt.localOnly, s_opt.port, s_opt.httpPort, s_requestQueue);
  s_network->start("network");

  // wait for end of MainLoop
  s_mainLoop->join();

  // shutdown
  shutdown();
  return 0;
}

}  // namespace ebusd

int main(int argc, char* argv[], char* envp[]) {
  return ebusd::main(argc, argv, envp);
}

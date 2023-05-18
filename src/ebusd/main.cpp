/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2022 John Baier <ebusd@ebusd.eu>
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
#include <argp.h>
#include <csignal>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <map>
#include <vector>
#include "ebusd/mainloop.h"
#include "lib/utils/log.h"
#include "lib/utils/httpclient.h"
#include "ebusd/scan.h"


/** the version string of the program. */
const char *argp_program_version = "" PACKAGE_STRING "." REVISION "";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = "" PACKAGE_BUGREPORT "";

namespace ebusd {

using std::dec;
using std::hex;
using std::setfill;
using std::setw;
using std::nouppercase;
using std::cout;

/** the path and name of the PID file. */
#ifdef PACKAGE_PIDFILE
#define PID_FILE_NAME PACKAGE_PIDFILE
#else
#define PID_FILE_NAME "/var/run/ebusd.pid"
#endif

/** the path and name of the log file. */
#ifdef PACKAGE_LOGFILE
#define LOG_FILE_NAME PACKAGE_LOGFILE
#else
#define LOG_FILE_NAME "/var/log/ebusd.log"
#endif

/** the config path part behind the scheme (scheme without "://"). */
#define CONFIG_PATH_SUFFIX "://cfg.ebusd.eu/"

/** the previous config path part to rewrite to the current one. */
#define PREVIOUS_CONFIG_PATH_SUFFIX "://ebusd.eu/config/"

/** the default path of the configuration files. */
#ifdef HAVE_SSL
#define CONFIG_PATH "https" CONFIG_PATH_SUFFIX
#else
#define CONFIG_PATH "http" CONFIG_PATH_SUFFIX
#endif

/** the opened PID file, or nullptr. */
static FILE* s_pidFile = nullptr;

/** the program options. */
static struct options s_opt = {
  "/dev/ttyUSB0",  // device
  false,  // noDeviceCheck
  false,  // readOnly
  false,  // initialSend
  0,  // extraLatency

  false,  // scanConfig
  0,  // initialScan
  getenv("LANG"),  // preferLanguage
  false,  // checkConfig
  OF_NONE,  // dumpConfig
  nullptr,  // dumpConfigTo
  5,  // pollInterval
  false,  // injectMessages
  false,  // stopAfterInject
  nullptr,  // caFile
  nullptr,  // caPath

  0x31,  // address
  false,  // answer
  10,  // acquireTimeout
  3,  // acquireRetries
  2,  // sendRetries
  SLAVE_RECV_TIMEOUT*5/3,  // receiveTimeout
  0,  // masterCount
  false,  // generateSyn

  "",  // accessLevel
  "",  // aclFile
  false,  // foreground
  false,  // enableHex
  false,  // enableDefine
  PID_FILE_NAME,  // pidFile
  8888,  // port
  false,  // localOnly
  0,  // httpPort
  "/var/" PACKAGE "/html",  // htmlPath
  true,  // updateCheck

  PACKAGE_LOGFILE,  // logFile
  -1,  // logAreas
  ll_COUNT,  // logLevel
  false,  // multiLog

  0,  // logRaw
  PACKAGE_LOGFILE,  // logRawFile
  100,  // logRawSize

  false,  // dump
  "/tmp/" PACKAGE "_dump.bin",  // dumpFile
  100,  // dumpSize
  false,  // dumpFlush
};

/** the @a MessageMap instance, or nullptr. */
static MessageMap* s_messageMap = nullptr;

/** the @a ScanHelper instance, or nullptr. */
static ScanHelper* s_scanHelper = nullptr;

/** the @a MainLoop instance, or nullptr. */
static MainLoop* s_mainLoop = nullptr;

/** the (optionally corrected) config path for retrieving configuration files from. */
static string s_configPath = CONFIG_PATH;

/** the documentation of the program. */
static const char argpdoc[] =
  "A daemon for communication with eBUS heating systems.";

#define O_INISND -2
#define O_DEVLAT (O_INISND-1)
#define O_CFGLNG (O_DEVLAT-1)
#define O_CHKCFG (O_CFGLNG-1)
#define O_DMPCFG (O_CHKCFG-1)
#define O_DMPCTO (O_DMPCFG-1)
#define O_POLINT (O_DMPCTO-1)
#define O_CAFILE (O_POLINT-1)
#define O_CAPATH (O_CAFILE-1)
#define O_ANSWER (O_CAPATH-1)
#define O_ACQTIM (O_ANSWER-1)
#define O_ACQRET (O_ACQTIM-1)
#define O_SNDRET (O_ACQRET-1)
#define O_RCVTIM (O_SNDRET-1)
#define O_MASCNT (O_RCVTIM-1)
#define O_GENSYN (O_MASCNT-1)
#define O_ACLDEF (O_GENSYN-1)
#define O_ACLFIL (O_ACLDEF-1)
#define O_HEXCMD (O_ACLFIL-1)
#define O_DEFCMD (O_HEXCMD-1)
#define O_PIDFIL (O_DEFCMD-1)
#define O_LOCAL  (O_PIDFIL-1)
#define O_HTTPPT (O_LOCAL-1)
#define O_HTMLPA (O_HTTPPT-1)
#define O_UPDCHK (O_HTMLPA-1)
#define O_LOG    (O_UPDCHK-1)
#define O_LOGARE (O_LOG-1)
#define O_LOGLEV (O_LOGARE-1)
#define O_RAW    (O_LOGLEV-1)
#define O_RAWFIL (O_RAW-1)
#define O_RAWSIZ (O_RAWFIL-1)
#define O_DMPFIL (O_RAWSIZ-1)
#define O_DMPSIZ (O_DMPFIL-1)
#define O_DMPFLU (O_DMPSIZ-1)

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
  {nullptr,          0,        nullptr,    0, "Device options:", 1 },
  {"device",         'd',      "DEV",      0, "Use DEV as eBUS device ("
      "\"enh:DEVICE\" or \"enh:IP:PORT\" for enhanced device, "
      "\"ens:DEVICE\" for enhanced high speed serial device, "
      "\"DEVICE\" for serial device, or \"[udp:]IP:PORT\" for network device) [/dev/ttyUSB0]", 0 },
  {"nodevicecheck",  'n',      nullptr,    0, "Skip serial eBUS device test", 0 },
  {"readonly",       'r',      nullptr,    0, "Only read from device, never write to it", 0 },
  {"initsend",       O_INISND, nullptr,    0, "Send an initial escape symbol after connecting device", 0 },
  {"latency",        O_DEVLAT, "MSEC",     0, "Extra transfer latency in ms [0]", 0 },

  {nullptr,          0,        nullptr,    0, "Message configuration options:", 2 },
  {"configpath",     'c',      "PATH",     0, "Read CSV config files from PATH (local folder or HTTPS URL) ["
      CONFIG_PATH "]", 0 },
  {"scanconfig",     's',      "ADDR", OPTION_ARG_OPTIONAL, "Pick CSV config files matching initial scan (ADDR="
      "\"none\" or empty for no initial scan message, \"full\" for full scan, or a single hex address to scan, "
      "default is broadcast ident message). If combined with --checkconfig, you can add scan message data as "
      "arguments for checking a particular scan configuration, e.g. \"FF08070400/0AB5454850303003277201\".", 0 },
  {"configlang",     O_CFGLNG, "LANG",     0,
      "Prefer LANG in multilingual configuration files [system default language]", 0 },
  {"checkconfig",    O_CHKCFG, nullptr,    0, "Check config files, then stop", 0 },
  {"dumpconfig",     O_DMPCFG, "FORMAT", OPTION_ARG_OPTIONAL,
      "Check and dump config files in FORMAT (\"json\" or \"csv\"), then stop", 0 },
  {"dumpconfigto",   O_DMPCTO, "FILE",     0, "Dump config files to FILE", 0 },
  {"pollinterval",   O_POLINT, "SEC",      0, "Poll for data every SEC seconds (0=disable) [5]", 0 },
  {"inject",         'i',      "stop", OPTION_ARG_OPTIONAL, "Inject remaining arguments as already seen messages (e.g. "
      "\"FF08070400/0AB5454850303003277201\"), optionally stop afterwards", 0 },
#ifdef HAVE_SSL
  {"cafile",         O_CAFILE, "FILE",     0, "Use CA FILE for checking certificates (uses defaults,"
                                              " \"#\" for insecure)", 0 },
  {"capath",         O_CAPATH, "PATH",     0, "Use CA PATH for checking certificates (uses defaults)", 0 },
#endif  // HAVE_SSL

  {nullptr,          0,        nullptr,    0, "eBUS options:", 3 },
  {"address",        'a',      "ADDR",     0, "Use ADDR as own bus address [31]", 0 },
  {"answer",         O_ANSWER, nullptr,    0, "Actively answer to requests from other masters", 0 },
  {"acquiretimeout", O_ACQTIM, "MSEC",     0, "Stop bus acquisition after MSEC ms [10]", 0 },
  {"acquireretries", O_ACQRET, "COUNT",    0, "Retry bus acquisition COUNT times [3]", 0 },
  {"sendretries",    O_SNDRET, "COUNT",    0, "Repeat failed sends COUNT times [2]", 0 },
  {"receivetimeout", O_RCVTIM, "MSEC",     0, "Expect a slave to answer within MSEC ms [25]", 0 },
  {"numbermasters",  O_MASCNT, "COUNT",    0, "Expect COUNT masters on the bus, 0 for auto detection [0]", 0 },
  {"generatesyn",    O_GENSYN, nullptr,    0, "Enable AUTO-SYN symbol generation", 0 },

  {nullptr,          0,        nullptr,    0, "Daemon options:", 4 },
  {"accesslevel",    O_ACLDEF, "LEVEL",    0, "Set default access level to LEVEL (\"*\" for everything) [\"\"]", 0 },
  {"aclfile",        O_ACLFIL, "FILE",     0, "Read access control list from FILE", 0 },
  {"foreground",     'f',      nullptr,    0, "Run in foreground", 0 },
  {"enablehex",      O_HEXCMD, nullptr,    0, "Enable hex command", 0 },
  {"enabledefine",   O_DEFCMD, nullptr,    0, "Enable define command", 0 },
  {"pidfile",        O_PIDFIL, "FILE",     0, "PID file name (only for daemon) [" PID_FILE_NAME "]", 0 },
  {"port",           'p',      "PORT",     0, "Listen for command line connections on PORT [8888]", 0 },
  {"localhost",      O_LOCAL,  nullptr,    0, "Listen for command line connections on 127.0.0.1 interface only", 0 },
  {"httpport",       O_HTTPPT, "PORT",     0, "Listen for HTTP connections on PORT, 0 to disable [0]", 0 },
  {"htmlpath",       O_HTMLPA, "PATH",     0, "Path for HTML files served by HTTP port [/var/ebusd/html]", 0 },
  {"updatecheck",    O_UPDCHK, "MODE",     0, "Set automatic update check to MODE (on|off) [on]", 0 },

  {nullptr,          0,        nullptr,    0, "Log options:", 5 },
  {"logfile",        'l',      "FILE",     0, "Write log to FILE (only for daemon, empty string for using syslog) ["
      PACKAGE_LOGFILE "]", 0 },
  {"log",            O_LOG, "AREAS:LEVEL", 0, "Only write log for matching AREA(S) below or equal to LEVEL"
      " (alternative to --logareas/--logevel, may be used multiple times) [all:notice]", 0 },
  {"logareas",       O_LOGARE, "AREAS",    0, "Only write log for matching AREA(S): main|network|bus|update|other"
      "|all [all]", 0 },
  {"loglevel",       O_LOGLEV, "LEVEL",    0, "Only write log below or equal to LEVEL: error|notice|info|debug"
      " [notice]", 0 },

  {nullptr,          0,        nullptr,    0, "Raw logging options:", 6 },
  {"lograwdata",     O_RAW,    "bytes", OPTION_ARG_OPTIONAL,
      "Log messages or all received/sent bytes on the bus", 0 },
  {"lograwdatafile", O_RAWFIL, "FILE",     0, "Write raw log to FILE [" PACKAGE_LOGFILE "]", 0 },
  {"lograwdatasize", O_RAWSIZ, "SIZE",     0, "Make raw log file no larger than SIZE kB [100]", 0 },

  {nullptr,          0,        nullptr,    0, "Binary dump options:", 7 },
  {"dump",           'D',      nullptr,    0, "Enable binary dump of received bytes", 0 },
  {"dumpfile",       O_DMPFIL, "FILE",     0, "Dump received bytes to FILE [/tmp/" PACKAGE "_dump.bin]", 0 },
  {"dumpsize",       O_DMPSIZ, "SIZE",     0, "Make dump file no larger than SIZE kB [100]", 0 },
  {"dumpflush",      O_DMPFLU, nullptr,    0, "Flush each byte", 0 },

  {nullptr,          0,        nullptr,    0, nullptr, 0 },
};

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct options *opt = (struct options*)state->input;
  result_t result = RESULT_OK;
  unsigned int value;

  switch (key) {
  // Device options:
  case 'd':  // --device=/dev/ttyUSB0
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid device");
      return EINVAL;
    }
    opt->device = arg;
    break;
  case 'n':  // --nodevicecheck
    opt->noDeviceCheck = true;
    break;
  case 'r':  // --readonly
    if (opt->answer || opt->generateSyn || opt->initialSend
        || (opt->scanConfig && opt->initialScan != 0 && opt->initialScan != ESC)) {
      argp_error(state, "cannot combine readonly with answer/generatesyn/initsend/scanconfig=*");
      return EINVAL;
    }
    opt->readOnly = true;
    break;
  case O_INISND:  // --initsend
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with answer/generatesyn/initsend/scanconfig=*");
      return EINVAL;
    }
    opt->initialSend = true;
    break;
  case O_DEVLAT:  // --latency=10
    value = parseInt(arg, 10, 0, 200000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 200)) {  // backwards compatible (micros)
      argp_error(state, "invalid latency");
      return EINVAL;
    }
    opt->extraLatency = value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;

  // Message configuration options:
  case 'c':  // --configpath=https://cfg.ebusd.eu/
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid configpath");
      return EINVAL;
    }
    s_configPath = arg;
    break;
  case 's':  // --scanconfig[=ADDR] (ADDR=<empty>|full|<hexaddr>)
  {
    if (opt->pollInterval == 0) {
      argp_error(state, "scanconfig without polling may lead to invalid files included for certain products!");
      return EINVAL;
    }
    symbol_t initialScan = ESC;
    if (!arg || arg[0] == 0 || strcmp("none", arg) == 0) {
      // no further setting needed
    } else if (strcmp("full", arg) == 0) {
      initialScan = SYN;
    } else {
      auto address = (symbol_t)parseInt(arg, 16, 0x00, 0xff, &result);
      if (result != RESULT_OK || !isValidAddress(address)) {
        argp_error(state, "invalid initial scan address");
        return EINVAL;
      }
      if (isMaster(address)) {
        initialScan = getSlaveAddress(address);
      } else {
        initialScan = address;
      }
    }
    if (opt->readOnly && initialScan != ESC) {
      argp_error(state, "cannot combine readonly with answer/generatesyn/initsend/scanconfig=*");
      return EINVAL;
    }
    opt->scanConfig = true;
    opt->initialScan = initialScan;
    break;
  }
  case O_CFGLNG:  // --configlang=LANG
    opt->preferLanguage = arg;
    break;
  case O_CHKCFG:  // --checkconfig
    if (opt->injectMessages) {
      argp_error(state, "invalid checkconfig");
      return EINVAL;
    }
    opt->checkConfig = true;
    break;
  case O_DMPCFG:  // --dumpconfig[=json|csv]
    if (opt->injectMessages) {
      argp_error(state, "invalid checkconfig");
      return EINVAL;
    }
    if (!arg || arg[0] == 0 || strcmp("csv", arg) == 0) {
      // no further flags
      opt->dumpConfig = OF_DEFINITION;
    } else if (strcmp("json", arg) == 0) {
      opt->dumpConfig = OF_DEFINITION | OF_NAMES | OF_UNITS | OF_COMMENTS | OF_VALUENAME | OF_ALL_ATTRS | OF_JSON;
    } else {
      argp_error(state, "invalid dumpconfig");
      return EINVAL;
    }
    opt->checkConfig = true;
    break;
  case O_DMPCTO:  // --dumpconfigto=FILE
    if (!arg || arg[0] == 0) {
      argp_error(state, "invalid dumpconfigto");
      return EINVAL;
    }
    opt->dumpConfigTo = arg;
    break;
  case O_POLINT:  // --pollinterval=5
    value = parseInt(arg, 10, 0, 3600, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid pollinterval");
      return EINVAL;
    }
    if (value == 0 && opt->scanConfig) {
      argp_error(state, "scanconfig without polling may lead to invalid files included for certain products!");
      return EINVAL;
    }
    opt->pollInterval = value;
    break;
  case 'i':  // --inject[=stop]
    if (opt->injectMessages || opt->checkConfig) {
      argp_error(state, "invalid inject");
      return EINVAL;
    }
    opt->injectMessages = true;
    opt->stopAfterInject = arg && strcmp("stop", arg) == 0;
    break;
  case O_CAFILE:  // --cafile=FILE
    opt->caFile = arg;
    break;
  case O_CAPATH:  // --capath=PATH
    opt->caPath = arg;
    break;

  // eBUS options:
  case 'a':  // --address=31
  {
    auto address = (symbol_t)parseInt(arg, 16, 0, 0xff, &result);
    if (result != RESULT_OK || !isMaster(address)) {
      argp_error(state, "invalid address");
      return EINVAL;
    }
    opt->address = address;
    break;
  }
  case O_ANSWER:  // --answer
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with answer/generatesyn/initsend/scanconfig=*");
      return EINVAL;
    }
    opt->answer = true;
    break;
  case O_ACQTIM:  // --acquiretimeout=10
    value = parseInt(arg, 10, 1, 100000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 100)) {  // backwards compatible (micros)
      argp_error(state, "invalid acquiretimeout");
      return EINVAL;
    }
    opt->acquireTimeout = value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;
  case O_ACQRET:  // --acquireretries=3
    value = parseInt(arg, 10, 0, 10, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid acquireretries");
      return EINVAL;
    }
    opt->acquireRetries = value;
    break;
  case O_SNDRET:  // --sendretries=2
    value = parseInt(arg, 10, 0, 10, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid sendretries");
      return EINVAL;
    }
    opt->sendRetries = value;
    break;
  case O_RCVTIM:  // --receivetimeout=25
    value = parseInt(arg, 10, 1, 100000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 100)) {  // backwards compatible (micros)
      argp_error(state, "invalid receivetimeout");
      return EINVAL;
    }
    opt->receiveTimeout =  value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;
  case O_MASCNT:  // --numbermasters=0
    value = parseInt(arg, 10, 0, 25, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid numbermasters");
      return EINVAL;
    }
    opt->masterCount = value;
    break;
  case O_GENSYN:  // --generatesyn
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with answer/generatesyn/initsend/scanconfig=*");
      return EINVAL;
    }
    opt->generateSyn = true;
    break;

  // Daemon options:
  case O_ACLDEF:  // --accesslevel=*
    if (arg == nullptr) {
      argp_error(state, "invalid accesslevel");
      return EINVAL;
    }
    opt->accessLevel = arg;
    break;
  case O_ACLFIL:  // --aclfile=/etc/ebusd/acl
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid aclfile");
      return EINVAL;
    }
    opt->aclFile = arg;
    break;
  case 'f':  // --foreground
    opt->foreground = true;
    break;
  case O_HEXCMD:  // --enablehex
    opt->enableHex = true;
    break;
  case O_DEFCMD:  // --enabledefine
    opt->enableDefine = true;
    break;
  case O_PIDFIL:  // --pidfile=/var/run/ebusd.pid
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid pidfile");
      return EINVAL;
    }
    opt->pidFile = arg;
    break;
  case 'p':  // --port=8888
    value = parseInt(arg, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid port");
      return EINVAL;
    }
    opt->port = (uint16_t)value;
    break;
  case O_LOCAL:  // --localhost
    opt->localOnly = true;
    break;
  case O_HTTPPT:  // --httpport=0
    value = parseInt(arg, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid httpport");
      return EINVAL;
    }
    opt->httpPort = (uint16_t)value;
    break;
  case O_HTMLPA:  // --htmlpath=/var/ebusd/html
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid htmlpath");
      return EINVAL;
    }
    opt->htmlPath = arg;
    break;
  case O_UPDCHK:  // --updatecheck=on
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid updatecheck");
      return EINVAL;
    }
    if (strcmp("on", arg) == 0) {
      opt->updateCheck = true;
    } else if (strcmp("off", arg) == 0) {
      opt->updateCheck = false;
    } else {
      argp_error(state, "invalid updatecheck");
      return EINVAL;
    }
    break;

  // Log options:
  case 'l':  // --logfile=/var/log/ebusd.log
    if (arg == nullptr || strcmp("/", arg) == 0) {
      argp_error(state, "invalid logfile");
      return EINVAL;
    }
    opt->logFile = arg;
    break;
  case O_LOG:  // --log=area(s):level
  {
    char* pos = strchr(arg, ':');
    if (pos == nullptr) {
      pos = strchr(arg, ' ');
      if (pos == nullptr) {
        argp_error(state, "invalid log");
        return EINVAL;
      }
    }
    *pos = 0;
    int facilities = parseLogFacilities(arg);
    if (facilities == -1) {
      argp_error(state, "invalid log: areas");
      return EINVAL;
    }
    LogLevel level = parseLogLevel(pos + 1);
    if (level == ll_COUNT) {
      argp_error(state, "invalid log: level");
      return EINVAL;
    }
    if (opt->logAreas != -1 || opt->logLevel != ll_COUNT) {
      argp_error(state, "invalid log (combined with logareas or loglevel)");
      return EINVAL;
    }
    setFacilitiesLogLevel(facilities, level);
    opt->multiLog = true;
    break;
  }
  case O_LOGARE:  // --logareas=all
  {
    int facilities = parseLogFacilities(arg);
    if (facilities == -1) {
      argp_error(state, "invalid logareas");
      return EINVAL;
    }
    if (opt->multiLog) {
      argp_error(state, "invalid logareas (combined with log)");
      return EINVAL;
    }
    opt->logAreas = facilities;
    break;
  }
  case O_LOGLEV:  // --loglevel=notice
  {
    LogLevel logLevel = parseLogLevel(arg);
    if (logLevel == ll_COUNT) {
      argp_error(state, "invalid loglevel");
      return EINVAL;
    }
    if (opt->multiLog) {
      argp_error(state, "invalid loglevel (combined with log)");
      return EINVAL;
    }
    opt->logLevel = logLevel;
    break;
  }

  // Raw logging options:
  case O_RAW:  // --lograwdata
    opt->logRaw = arg && strcmp("bytes", arg) == 0 ? 2 : 1;
    break;
  case O_RAWFIL:  // --lograwdatafile=/var/log/ebusd.log
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid lograwdatafile");
      return EINVAL;
    }
    opt->logRawFile = arg;
    break;
  case O_RAWSIZ:  // --lograwdatasize=100
    value = parseInt(arg, 10, 1, 1000000, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid lograwdatasize");
      return EINVAL;
    }
    opt->logRawSize = value;
    break;


  // Binary dump options:
  case 'D':  // --dump
    opt->dump = true;
    break;
  case O_DMPFIL:  // --dumpfile=/tmp/ebusd_dump.bin
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid dumpfile");
      return EINVAL;
    }
    opt->dumpFile = arg;
    break;
  case O_DMPSIZ:  // --dumpsize=100
    value = parseInt(arg, 10, 1, 1000000, &result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid dumpsize");
      return EINVAL;
    }
    opt->dumpSize = value;
    break;
  case O_DMPFLU:  // --dumpflush
    opt->dumpFlush = true;
    break;

  case ARGP_KEY_ARG:
    if (opt->injectMessages || (opt->checkConfig && opt->scanConfig)) {
      return ARGP_ERR_UNKNOWN;
    }
    argp_error(state, "invalid arguments starting with \"%s\"", arg);
    return EINVAL;
  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}
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
  if (s_mainLoop) {
    delete s_mainLoop;
    s_mainLoop = nullptr;
  }
  if (s_messageMap) {
    delete s_messageMap;
    s_messageMap = nullptr;
  }
  if (s_scanHelper) {
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
  struct argp aargp = { argpoptions, parse_opt, nullptr, argpdoc, datahandler_getargs(), nullptr, nullptr };
  setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);

  char envname[32] = "--";  // needs to cover at least max length of any option name plus "--"
  char* envopt = envname+2;
  for (char ** env = envp; *env; env++) {
    char* pos = strchr(*env, '=');
    if (!pos || strncmp(*env, "EBUSD_", sizeof("EBUSD_")-1) != 0) {
      continue;
    }
    char* start = *env+sizeof("EBUSD_")-1;
    size_t len = pos-start;
    if (len <= 1 || len > sizeof(envname)-3) {  // no single char long args
      continue;
    }
    for (size_t i=0; i < len; i++) {
      envopt[i] = static_cast<char>(tolower(start[i]));
    }
    envopt[len] = 0;
    if (strcmp(envopt, "version") == 0 || strcmp(envopt, "image") == 0 || strcmp(envopt, "arch") == 0
       || strcmp(envopt, "opts") == 0 || strcmp(envopt, "inject") == 0
       || strcmp(envopt, "checkconfig") == 0 || strncmp(envopt, "dumpconfig", 10) == 0
    ) {
      // ignore those defined in Dockerfile, EBUSD_OPTS, those with final args, and interactive ones
      continue;
    }
    char* envargv[] = {envname, pos+1};
    int cnt = pos[1] ? 2 : 1;
    if (pos[1] && strlen(*env) < sizeof(envname)-3
    && (strcmp(envopt, "scanconfig") == 0 || strcmp(envopt, "lograwdata") == 0)) {
      // only really special case: OPTION_ARG_OPTIONAL with non-empty arg needs to use "=" syntax
      cnt = 1;
      strcat(envopt, pos);
    }
    int idx = -1;
    s_opt.injectMessages = true;  // for skipping unknown values via ARGP_ERR_UNKNOWN
    error_t err = argp_parse(&aargp, cnt, envargv, ARGP_PARSE_ARGV0|ARGP_SILENT|ARGP_IN_ORDER, &idx, &s_opt);
    if (err != 0 && idx == -1) {  // ignore args for non-arg boolean options
      if (err == ESRCH) {  // special value to abort immediately
        logError(lf_main, "invalid argument in env: %s", envopt);
        return EINVAL;
      }
      logError(lf_main, "invalid/unknown argument in env (ignored): %s", envopt);
    }
    s_opt.injectMessages = false;  // restore (was not parsed from cmdline args yet)
  }

  int arg_index = -1;
  if (argp_parse(&aargp, argc, argv, ARGP_IN_ORDER, &arg_index, &s_opt) != 0) {
    logError(lf_main, "invalid arguments");
    return EINVAL;
  }

  if (!s_configPath.empty() && s_configPath[s_configPath.length()-1] != '/') {
    s_configPath += "/";
  }
  const string lang = MappedFileReader::normalizeLanguage(
    s_opt.preferLanguage == nullptr || !s_opt.preferLanguage[0] ? "" : s_opt.preferLanguage
  );
  string configLocalPrefix, configUriPrefix;
  HttpClient* configHttpClient = nullptr;
  if (s_configPath.find("://") == string::npos) {
    configLocalPrefix = s_configPath;
  } else {
    if (!s_opt.scanConfig) {
      logError(lf_main, "invalid configpath without scanconfig");
      return EINVAL;
    }
    size_t pos = s_configPath.find(PREVIOUS_CONFIG_PATH_SUFFIX);
    if (pos != string::npos) {
      string newPath = s_configPath.substr(0, pos) + CONFIG_PATH_SUFFIX
        + s_configPath.substr(pos+strlen(PREVIOUS_CONFIG_PATH_SUFFIX));
      logNotice(lf_main, "replaced old configPath %s with new one: %s", s_configPath.c_str(), newPath.c_str());
      s_configPath = newPath;
    }
    uint16_t configPort = 80;
    string proto, configHost;
    if (!HttpClient::parseUrl(s_configPath, &proto, &configHost, &configPort, &configUriPrefix)) {
#ifndef HAVE_SSL
      if (proto == "https") {
        logError(lf_main, "invalid configPath URL (HTTPS not supported)");
        return EINVAL;
      }
#endif
      logError(lf_main, "invalid configPath URL");
      return EINVAL;
    }
    configHttpClient = new HttpClient(s_opt.caFile, s_opt.caPath);
    if (
      // check with low timeout of 1 second initially:
      !configHttpClient->connect(configHost, configPort, proto == "https", PACKAGE_NAME "/" PACKAGE_VERSION, 1)
      // if that did not work, issue a single retry with default timeout:
      && !configHttpClient->connect(configHost, configPort, proto == "https", PACKAGE_NAME "/" PACKAGE_VERSION)
    ) {
      logError(lf_main, "invalid configPath URL (connect)");
      delete configHttpClient;
      cleanup();
      return EINVAL;
    }
    configHttpClient->disconnect();
  }
  if (!s_opt.readOnly && s_opt.scanConfig && s_opt.initialScan == 0) {
    s_opt.initialScan = BROADCAST;
  }
  if (s_opt.logAreas != -1 || s_opt.logLevel != ll_COUNT) {
    setFacilitiesLogLevel(LF_ALL, ll_none);
    setFacilitiesLogLevel(s_opt.logAreas, s_opt.logLevel);
  }

  s_messageMap = new MessageMap(s_opt.checkConfig, lang);
  s_scanHelper = new ScanHelper(s_messageMap, s_configPath, configLocalPrefix, configUriPrefix,
    lang.empty() ? lang : "?l=" + lang, configHttpClient, s_opt.checkConfig);
  s_messageMap->setResolver(s_scanHelper);
  if (s_opt.checkConfig) {
    logNotice(lf_main, PACKAGE_STRING "." REVISION " performing configuration check...");

    result_t result = s_scanHelper->loadConfigFiles(!s_opt.scanConfig || arg_index >= argc);
    result_t overallResult = s_scanHelper->executeInstructions(nullptr);
    MasterSymbolString master;
    SlaveSymbolString slave;
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
      s_messageMap->dump(true, s_opt.dumpConfig, out);
      if (fout.is_open()) {
        fout.close();
      }
    }

    cleanup();
    return overallResult == RESULT_OK ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // open the device
  Device *device = Device::create(s_opt.device, s_opt.extraLatency, !s_opt.noDeviceCheck, s_opt.readOnly,
                                  s_opt.initialSend);
  if (device == nullptr) {
    logError(lf_main, "unable to create device %s", s_opt.device);
    cleanup();
    return EINVAL;
  }

  if (!s_opt.foreground) {
    if (!setLogFile(s_opt.logFile)) {
      logError(lf_main, "unable to open log file %s", s_opt.logFile);
      cleanup();
      return EINVAL;
    }
    daemonize();  // make daemon
  }

  // trap signals that we expect to receive
  signal(SIGHUP, signalHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  logNotice(lf_main, PACKAGE_STRING "." REVISION " started%s%s on%s device %s",
      device->isReadOnly() ? " read only" : "",
      s_opt.scanConfig ? s_opt.initialScan == ESC ? " with auto scan"
      : s_opt.initialScan == BROADCAST ? " with broadcast scan" : s_opt.initialScan == SYN ? " with full scan"
      : " with single scan" : "",
      device->isEnhancedProto() ? " enhanced" : "",
      device->getName());

  // load configuration files
  s_scanHelper->loadConfigFiles(s_messageMap);

  // create the MainLoop and start it
  s_mainLoop = new MainLoop(s_opt, device, s_messageMap, s_scanHelper);
  if (s_opt.injectMessages) {
    BusHandler* busHandler = s_mainLoop->getBusHandler();
    while (arg_index < argc) {
      // add each passed message
      MasterSymbolString master;
      SlaveSymbolString slave;
      if (!s_scanHelper->parseMessage(argv[arg_index++], false, &master, &slave)) {
        continue;
      }
      busHandler->injectMessage(master, slave);
    }
    if (s_opt.stopAfterInject) {
      shutdown();
      return 0;
    }
  }
  s_mainLoop->start("mainloop");

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

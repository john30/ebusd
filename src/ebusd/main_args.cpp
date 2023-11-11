/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023 John Baier <ebusd@ebusd.eu>
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
#include <string>
#include "ebusd/datahandler.h"
#include "lib/utils/log.h"
#include "lib/utils/arg.h"
#include "ebusd/scan.h"

namespace ebusd {

/** the default path of the configuration files. */
#ifdef HAVE_SSL
#define CONFIG_PATH "https" CONFIG_PATH_SUFFIX
#else  // HAVE_SSL
#define CONFIG_PATH "http" CONFIG_PATH_SUFFIX
#endif  // HAVE_SSL

/** the default program options. */
static const options_t s_default_opt = {
  .device = "/dev/ttyUSB0",
  .noDeviceCheck = false,
  .readOnly = false,
  .initialSend = false,
  .extraLatency = 0,

  .configPath = nullptr,
  .scanConfigOrPathSet = false,
  .scanConfig = false,
  .initialScan = 0,
  .scanRetries = 5,
  .preferLanguage = getenv("LANG"),
  .checkConfig = false,
  .dumpConfig = OF_NONE,
  .dumpConfigTo = nullptr,
  .pollInterval = 5,
  .injectMessages = false,
  .stopAfterInject = false,
  .injectCount = 0,
#ifdef HAVE_SSL
  .caFile = nullptr,
  .caPath = nullptr,
#endif  // HAVE_SSL
  .address = 0x31,
  .answer = false,
  .acquireTimeout = 10,
  .acquireRetries = 3,
  .sendRetries = 2,
  .receiveTimeout = SLAVE_RECV_TIMEOUT*5/3,
  .masterCount = 0,
  .generateSyn = false,

  .accessLevel = "",
  .aclFile = "",
  .foreground = false,
  .enableHex = false,
  .enableDefine = false,
  .pidFile = PACKAGE_PIDFILE,
  .port = 8888,
  .localOnly = false,
  .httpPort = 0,
  .htmlPath = "/var/" PACKAGE "/html",
  .updateCheck = true,

  .logFile = PACKAGE_LOGFILE,
  .logAreas = -1,
  .logLevel = ll_COUNT,
  .multiLog = false,

  .logRaw = 0,
  .logRawFile = PACKAGE_LOGFILE,
  .logRawSize = 100,

  .dump = false,
  .dumpFile = "/tmp/" PACKAGE "_dump.bin",
  .dumpSize = 100,
  .dumpFlush = false
};

/** the (optionally corrected) config path for retrieving configuration files from. */
static string s_configPath = CONFIG_PATH;

#define O_INISND -2
#define O_DEVLAT (O_INISND-1)
#define O_SCNRET (O_DEVLAT-1)
#define O_CFGLNG (O_SCNRET-1)
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
#define O_INJPOS 0x100

/** the definition of the known program arguments. */
static const argDef argDefs[] = {
  {nullptr,          0,        nullptr,    0, "Device options:"},
  {"device",         'd',      "DEV",      0, "Use DEV as eBUS device ("
      "prefix \"ens:\" for enhanced high speed device or "
      "\"enh:\" for enhanced device, with "
      "\"IP:PORT\" for network device or "
      "\"DEVICE\" for serial device"
      ") [/dev/ttyUSB0]"},
  {"nodevicecheck",  'n',      nullptr,    0, "Skip serial eBUS device test"},
  {"readonly",       'r',      nullptr,    0, "Only read from device, never write to it"},
  {"initsend",       O_INISND, nullptr,    0, "Send an initial escape symbol after connecting device"},
  {"latency",        O_DEVLAT, "MSEC",     0, "Extra transfer latency in ms [0]"},

  {nullptr,          0,        nullptr,    0, "Message configuration options:"},
  {"configpath",     'c',      "PATH",     0, "Read CSV config files from PATH (local folder or HTTPS URL) ["
      CONFIG_PATH "]"},
  {"scanconfig",     's',      "ADDR", af_optional, "Pick CSV config files matching initial scan ADDR: "
      "empty for broadcast ident message (default when configpath is not given), "
      "\"none\" for no initial scan message, "
      "\"full\" for full scan, "
      "a single hex address to scan, or "
      "\"off\" for not picking CSV files by scan result (default when configpath is given).\n"
      "If combined with --checkconfig, you can add scan message data as "
      "arguments for checking a particular scan configuration, e.g. \"FF08070400/0AB5454850303003277201\"."},
  {"scanretries",    O_SCNRET, "COUNT",    0, "Retry scanning devices COUNT times [5]"},
  {"configlang",     O_CFGLNG, "LANG",     0,
      "Prefer LANG in multilingual configuration files [system default language]"},
  {"checkconfig",    O_CHKCFG, nullptr,    0, "Check config files, then stop"},
  {"dumpconfig",     O_DMPCFG, "FORMAT", af_optional,
      "Check and dump config files in FORMAT (\"json\" or \"csv\"), then stop"},
  {"dumpconfigto",   O_DMPCTO, "FILE",     0, "Dump config files to FILE"},
  {"pollinterval",   O_POLINT, "SEC",      0, "Poll for data every SEC seconds (0=disable) [5]"},
  {"inject",         'i',      "stop", af_optional, "Inject remaining arguments as already seen messages (e.g. "
      "\"FF08070400/0AB5454850303003277201\"), optionally stop afterwards"},
  {nullptr,          O_INJPOS, "INJECT", af_optional|af_multiple, "Message(s) to inject (if --inject was given)"},
#ifdef HAVE_SSL
  {"cafile",         O_CAFILE, "FILE",     0, "Use CA FILE for checking certificates (uses defaults,"
                                              " \"#\" for insecure)"},
  {"capath",         O_CAPATH, "PATH",     0, "Use CA PATH for checking certificates (uses defaults)"},
#endif  // HAVE_SSL

  {nullptr,          0,        nullptr,    0, "eBUS options:"},
  {"address",        'a',      "ADDR",     0, "Use hex ADDR as own master bus address [31]"},
  {"answer",         O_ANSWER, nullptr,    0, "Actively answer to requests from other masters"},
  {"acquiretimeout", O_ACQTIM, "MSEC",     0, "Stop bus acquisition after MSEC ms [10]"},
  {"acquireretries", O_ACQRET, "COUNT",    0, "Retry bus acquisition COUNT times [3]"},
  {"sendretries",    O_SNDRET, "COUNT",    0, "Repeat failed sends COUNT times [2]"},
  {"receivetimeout", O_RCVTIM, "MSEC",     0, "Expect a slave to answer within MSEC ms [25]"},
  {"numbermasters",  O_MASCNT, "COUNT",    0, "Expect COUNT masters on the bus, 0 for auto detection [0]"},
  {"generatesyn",    O_GENSYN, nullptr,    0, "Enable AUTO-SYN symbol generation"},

  {nullptr,          0,        nullptr,    0, "Daemon options:"},
  {"accesslevel",    O_ACLDEF, "LEVEL",    0, "Set default access level to LEVEL (\"*\" for everything) [\"\"]"},
  {"aclfile",        O_ACLFIL, "FILE",     0, "Read access control list from FILE"},
  {"foreground",     'f',      nullptr,    0, "Run in foreground"},
  {"enablehex",      O_HEXCMD, nullptr,    0, "Enable hex command"},
  {"enabledefine",   O_DEFCMD, nullptr,    0, "Enable define command"},
  {"pidfile",        O_PIDFIL, "FILE",     0, "PID file name (only for daemon) [" PACKAGE_PIDFILE "]"},
  {"port",           'p',      "PORT",     0, "Listen for command line connections on PORT [8888]"},
  {"localhost",      O_LOCAL,  nullptr,    0, "Listen for command line connections on 127.0.0.1 interface only"},
  {"httpport",       O_HTTPPT, "PORT",     0, "Listen for HTTP connections on PORT, 0 to disable [0]"},
  {"htmlpath",       O_HTMLPA, "PATH",     0, "Path for HTML files served by HTTP port [/var/ebusd/html]"},
  {"updatecheck",    O_UPDCHK, "MODE",     0, "Set automatic update check to MODE (on|off) [on]"},

  {nullptr,          0,        nullptr,    0, "Log options:"},
  {"logfile",        'l',      "FILE",     0, "Write log to FILE (only for daemon, empty string for using syslog) ["
      PACKAGE_LOGFILE "]"},
  {"log",            O_LOG, "AREAS:LEVEL", 0, "Only write log for matching AREA(S) below or equal to LEVEL"
      " (alternative to --logareas/--logevel, may be used multiple times) [all:notice]"},
  {"logareas",       O_LOGARE, "AREAS",    0, "Only write log for matching AREA(S): main|network|bus|update|other"
      "|all [all]"},
  {"loglevel",       O_LOGLEV, "LEVEL",    0, "Only write log below or equal to LEVEL: error|notice|info|debug"
      " [notice]"},

  {nullptr,          0,        nullptr,    0, "Raw logging options:"},
  {"lograwdata",     O_RAW,    "bytes", af_optional,
      "Log messages or all received/sent bytes on the bus"},
  {"lograwdatafile", O_RAWFIL, "FILE",     0, "Write raw log to FILE [" PACKAGE_LOGFILE "]"},
  {"lograwdatasize", O_RAWSIZ, "SIZE",     0, "Make raw log file no larger than SIZE kB [100]"},

  {nullptr,          0,        nullptr,    0, "Binary dump options:"},
  {"dump",           'D',      nullptr,    0, "Enable binary dump of received bytes"},
  {"dumpfile",       O_DMPFIL, "FILE",     0, "Dump received bytes to FILE [/tmp/" PACKAGE "_dump.bin]"},
  {"dumpsize",       O_DMPSIZ, "SIZE",     0, "Make dump file no larger than SIZE kB [100]"},
  {"dumpflush",      O_DMPFLU, nullptr,    0, "Flush each byte"},

  {nullptr,          0,        nullptr,    0, nullptr},
};

/**
 * The program argument parsing function.
 * @param key the key from @a argDefs.
 * @param arg the option argument, or nullptr.
 * @param parseOpt the parse options.
 */
static int parse_opt(int key, char *arg, const argParseOpt *parseOpt, struct options *opt) {
  result_t result = RESULT_OK;
  unsigned int value;

  switch (key) {
  // Device options:
  case 'd':  // --device=/dev/ttyUSB0
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid device");
      return EINVAL;
    }
    opt->device = arg;
    break;
  case 'n':  // --nodevicecheck
    opt->noDeviceCheck = true;
    break;
  case 'r':  // --readonly
    opt->readOnly = true;
    break;
  case O_INISND:  // --initsend
    opt->initialSend = true;
    break;
  case O_DEVLAT:  // --latency=10
    value = parseInt(arg, 10, 0, 200000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 200)) {  // backwards compatible (micros)
      argParseError(parseOpt, "invalid latency");
      return EINVAL;
    }
    opt->extraLatency = value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;

  // Message configuration options:
  case 'c':  // --configpath=https://cfg.ebusd.eu/
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid configpath");
      return EINVAL;
    }
    s_configPath = arg;
    opt->scanConfigOrPathSet = true;
    break;
  case 's':  // --scanconfig[=ADDR] (ADDR=<empty>|none|full|<hexaddr>|off)
  {
    symbol_t initialScan = 0;
    if (!arg || arg[0] == 0) {
      initialScan = BROADCAST;  // default for no or empty argument
    } else if (strcmp("none", arg) == 0) {
      initialScan = ESC;
    } else if (strcmp("full", arg) == 0) {
      initialScan = SYN;
    } else if (strcmp("off", arg) == 0) {
      // zero turns scanConfig off
    } else {
      auto address = (symbol_t)parseInt(arg, 16, 0x00, 0xff, &result);
      if (result != RESULT_OK || !isValidAddress(address)) {
        argParseError(parseOpt, "invalid initial scan address");
        return EINVAL;
      }
      if (isMaster(address)) {
        initialScan = getSlaveAddress(address);
      } else {
        initialScan = address;
      }
    }
    opt->scanConfig = initialScan != 0;
    opt->initialScan = initialScan;
    opt->scanConfigOrPathSet = true;
    break;
  }
  case O_SCNRET:  // --scanretries=10
    value = parseInt(arg, 10, 0, 100, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid scanretries");
      return EINVAL;
    }
    opt->scanRetries = value;
    break;
  case O_CFGLNG:  // --configlang=LANG
    opt->preferLanguage = arg;
    break;
  case O_CHKCFG:  // --checkconfig
    opt->checkConfig = true;
    break;
  case O_DMPCFG:  // --dumpconfig[=json|csv]
    if (!arg || arg[0] == 0 || strcmp("csv", arg) == 0) {
      // no further flags
      opt->dumpConfig = OF_DEFINITION;
    } else if (strcmp("json", arg) == 0) {
      opt->dumpConfig = OF_DEFINITION | OF_NAMES | OF_UNITS | OF_COMMENTS | OF_VALUENAME | OF_ALL_ATTRS | OF_JSON;
    } else {
      argParseError(parseOpt, "invalid dumpconfig");
      return EINVAL;
    }
    opt->checkConfig = true;
    break;
  case O_DMPCTO:  // --dumpconfigto=FILE
    if (!arg || arg[0] == 0) {
      argParseError(parseOpt, "invalid dumpconfigto");
      return EINVAL;
    }
    opt->dumpConfigTo = arg;
    break;
  case O_POLINT:  // --pollinterval=5
    value = parseInt(arg, 10, 0, 3600, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid pollinterval");
      return EINVAL;
    }
    opt->pollInterval = value;
    break;
  case 'i':  // --inject[=stop]
    opt->injectMessages = true;
    opt->stopAfterInject = arg && strcmp("stop", arg) == 0;
    break;
#ifdef HAVE_SSL
  case O_CAFILE:  // --cafile=FILE
    opt->caFile = arg;
    break;
  case O_CAPATH:  // --capath=PATH
    opt->caPath = arg;
    break;
#endif  // HAVE_SSL
  // eBUS options:
  case 'a':  // --address=31
  {
    auto address = (symbol_t)parseInt(arg, 16, 0, 0xff, &result);
    if (result != RESULT_OK || !isMaster(address)) {
      argParseError(parseOpt, "invalid address");
      return EINVAL;
    }
    opt->address = address;
    break;
  }
  case O_ANSWER:  // --answer
    opt->answer = true;
    break;
  case O_ACQTIM:  // --acquiretimeout=10
    value = parseInt(arg, 10, 1, 100000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 100)) {  // backwards compatible (micros)
      argParseError(parseOpt, "invalid acquiretimeout");
      return EINVAL;
    }
    opt->acquireTimeout = value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;
  case O_ACQRET:  // --acquireretries=3
    value = parseInt(arg, 10, 0, 10, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid acquireretries");
      return EINVAL;
    }
    opt->acquireRetries = value;
    break;
  case O_SNDRET:  // --sendretries=2
    value = parseInt(arg, 10, 0, 10, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid sendretries");
      return EINVAL;
    }
    opt->sendRetries = value;
    break;
  case O_RCVTIM:  // --receivetimeout=25
    value = parseInt(arg, 10, 1, 100000, &result);  // backwards compatible (micros)
    if (result != RESULT_OK || (value <= 1000 && value > 100)) {  // backwards compatible (micros)
      argParseError(parseOpt, "invalid receivetimeout");
      return EINVAL;
    }
    opt->receiveTimeout =  value > 1000 ? value/1000 : value;  // backwards compatible (micros)
    break;
  case O_MASCNT:  // --numbermasters=0
    value = parseInt(arg, 10, 0, 25, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid numbermasters");
      return EINVAL;
    }
    opt->masterCount = value;
    break;
  case O_GENSYN:  // --generatesyn
    opt->generateSyn = true;
    break;

  // Daemon options:
  case O_ACLDEF:  // --accesslevel=*
    if (arg == nullptr) {
      argParseError(parseOpt, "invalid accesslevel");
      return EINVAL;
    }
    opt->accessLevel = arg;
    break;
  case O_ACLFIL:  // --aclfile=/etc/ebusd/acl
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid aclfile");
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
      argParseError(parseOpt, "invalid pidfile");
      return EINVAL;
    }
    opt->pidFile = arg;
    break;
  case 'p':  // --port=8888
    value = parseInt(arg, 10, 1, 65535, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid port");
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
      argParseError(parseOpt, "invalid httpport");
      return EINVAL;
    }
    opt->httpPort = (uint16_t)value;
    break;
  case O_HTMLPA:  // --htmlpath=/var/ebusd/html
    if (arg == nullptr || arg[0] == 0 || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid htmlpath");
      return EINVAL;
    }
    opt->htmlPath = arg;
    break;
  case O_UPDCHK:  // --updatecheck=on
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid updatecheck");
      return EINVAL;
    }
    if (strcmp("on", arg) == 0) {
      opt->updateCheck = true;
    } else if (strcmp("off", arg) == 0) {
      opt->updateCheck = false;
    } else {
      argParseError(parseOpt, "invalid updatecheck");
      return EINVAL;
    }
    break;

  // Log options:
  case 'l':  // --logfile=/var/log/ebusd.log
    if (arg == nullptr || strcmp("/", arg) == 0) {
      argParseError(parseOpt, "invalid logfile");
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
        argParseError(parseOpt, "invalid log");
        return EINVAL;
      }
    }
    *pos = 0;
    int facilities = parseLogFacilities(arg);
    if (facilities == -1) {
      argParseError(parseOpt, "invalid log: areas");
      return EINVAL;
    }
    LogLevel level = parseLogLevel(pos + 1);
    if (level == ll_COUNT) {
      argParseError(parseOpt, "invalid log: level");
      return EINVAL;
    }
    if (opt->logAreas != -1 || opt->logLevel != ll_COUNT) {
      argParseError(parseOpt, "invalid log (combined with logareas or loglevel)");
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
      argParseError(parseOpt, "invalid logareas");
      return EINVAL;
    }
    if (opt->multiLog) {
      argParseError(parseOpt, "invalid logareas (combined with log)");
      return EINVAL;
    }
    opt->logAreas = facilities;
    break;
  }
  case O_LOGLEV:  // --loglevel=notice
  {
    LogLevel logLevel = parseLogLevel(arg);
    if (logLevel == ll_COUNT) {
      argParseError(parseOpt, "invalid loglevel");
      return EINVAL;
    }
    if (opt->multiLog) {
      argParseError(parseOpt, "invalid loglevel (combined with log)");
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
      argParseError(parseOpt, "invalid lograwdatafile");
      return EINVAL;
    }
    opt->logRawFile = arg;
    break;
  case O_RAWSIZ:  // --lograwdatasize=100
    value = parseInt(arg, 10, 1, 1000000, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid lograwdatasize");
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
      argParseError(parseOpt, "invalid dumpfile");
      return EINVAL;
    }
    opt->dumpFile = arg;
    break;
  case O_DMPSIZ:  // --dumpsize=100
    value = parseInt(arg, 10, 1, 1000000, &result);
    if (result != RESULT_OK) {
      argParseError(parseOpt, "invalid dumpsize");
      return EINVAL;
    }
    opt->dumpSize = value;
    break;
  case O_DMPFLU:  // --dumpflush
    opt->dumpFlush = true;
    break;

  default:
    if (key >= O_INJPOS) {  // INJECT
      if (!opt->injectMessages || !arg || !arg[0]) {
        return ESRCH;
      }
      opt->injectCount++;
    } else {
      return ESRCH;
    }
  }

  // check for invalid arg combinations
  if (opt->readOnly && (opt->answer || opt->generateSyn || opt->initialSend
      || (opt->scanConfig && opt->initialScan != ESC))) {
    argParseError(parseOpt, "cannot combine readonly with answer/generatesyn/initsend/scanconfig");
    return EINVAL;
  }
  if (opt->scanConfig && opt->pollInterval == 0) {
    argParseError(parseOpt, "scanconfig without polling may lead to invalid files included for certain products!");
    return EINVAL;
  }
  if (opt->injectMessages && (opt->checkConfig || opt->dumpConfig)) {
    argParseError(parseOpt, "cannot combine inject with checkconfig/dumpconfig");
    return EINVAL;
  }
  return 0;
}

int parse_main_args(int argc, char* argv[], char* envp[], options_t *opt) {
  *opt = s_default_opt;
  const argParseOpt parseOpt = {
    argDefs,
    reinterpret_cast<parse_function_t>(parse_opt),
    0,
    "A daemon for communication with eBUS heating systems.",
    "Report bugs to " PACKAGE_BUGREPORT " .",
    datahandler_getargs()
  };

  char envname[32] = "--";  // needs to cover at least max length of any option name plus "--"
  char* envopt = envname+2;
  for (char ** env = envp; env && *env; env++) {
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
    char* envargv[] = {argv[0], envname, pos+1};
    int cnt = pos[1] ? 2 : 1;
    if (pos[1] && strlen(*env) < sizeof(envname)-3
    && (strcmp(envopt, "scanconfig") == 0 || strcmp(envopt, "lograwdata") == 0)) {
      // only really special case: af_optional with non-empty arg needs to use "=" syntax
      cnt = 1;
      strcat(envopt, pos);
    }
    int idx = -1;
    int err = argParse(&parseOpt, 1+cnt, envargv, &idx);
    if (err != 0 && idx == -1) {  // ignore args for non-arg boolean options
      if (err == ESRCH) {  // special value to abort immediately
        logWrite(lf_main, ll_error, "invalid argument in env: %s", *env);  // force logging on exit
        return EINVAL;
      }
      logWrite(lf_main, ll_error, "invalid/unknown argument in env (ignored): %s", *env);  // force logging
    }
  }

  int ret = argParse(&parseOpt, argc, argv, reinterpret_cast<void*>(opt));
  if (ret != 0) {
    return ret;
  }

  if (!opt->readOnly && !opt->scanConfigOrPathSet) {
    opt->scanConfig = true;
    opt->initialScan = BROADCAST;
  }
  if (!s_configPath.empty()) {
    if (s_configPath[s_configPath.length()-1] != '/') {
      s_configPath += "/";
    }
    opt->configPath = s_configPath.c_str();  // OK as s_configPath is kept
  }
  return 0;
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
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

/** the default path of the configuration files. */
#ifdef PACKAGE_CONFIGPATH
#define CONFIG_PATH PACKAGE_CONFIGPATH
#else
#define CONFIG_PATH "/etc/ebusd"
#endif

/** the opened PID file, or NULL. */
static FILE* pidFile = NULL;

/** true when forked into daemon mode. */
static bool isDaemon = false;

/** the program options. */
static struct options opt = {
  "/dev/ttyUSB0",  // device
  false,  // noDeviceCheck
  false,  // readOnly
  false,  // initialSend
  -1,  // latency

  CONFIG_PATH,  // configPath
  false,  // scanConfig
  BROADCAST,  // initialScan
  false,  // checkConfig
  false,  // dumpConfig
  5,  // pollInterval

  0x31,  // address
  false,  // answer
  9400,  // acquireTimeout
  3,  // acquireRetries
  2,  // sendRetries
  SLAVE_RECV_TIMEOUT*5/3,  // receiveTimeout
  0,  // masterCount
  false,  // generateSyn

  "",  // accessLevel
  "",  // aclFile
  false,  // foreground
  false,  // enableHex
  PID_FILE_NAME,  // pidFile
  8888,  // port
  false,  // localOnly
  0,  // httpPort
  "/var/" PACKAGE "/html",  // htmlPath

  PACKAGE_LOGFILE,  // logFile
  -1,  // logAreas
  ll_COUNT,  // logLevel
  false,  // multiLog

  false,  // logRaw
  PACKAGE_LOGFILE,  // logRawFile
  100,  // logRawSize

  false,  // dump
  "/tmp/" PACKAGE "_dump.bin",  // dumpFile
  100,  // dumpSize
};

/** the @a MessageMap instance, or NULL. */
static MessageMap* s_messageMap = NULL;

/** the @a MainLoop instance, or NULL. */
static MainLoop* s_mainLoop = NULL;

/** the documentation of the program. */
static const char argpdoc[] =
  "A daemon for communication with eBUS heating systems.";

#define O_INISND 1
#define O_DEVLAT (O_INISND+1)
#define O_CHKCFG (O_DEVLAT+1)
#define O_DMPCFG (O_CHKCFG+1)
#define O_POLINT (O_DMPCFG+1)
#define O_ANSWER (O_POLINT+1)
#define O_ACQTIM (O_ANSWER+1)
#define O_ACQRET (O_ACQTIM+1)
#define O_SNDRET (O_ACQRET+1)
#define O_RCVTIM (O_SNDRET+1)
#define O_MASCNT (O_RCVTIM+1)
#define O_GENSYN (O_MASCNT+1)
#define O_ACLDEF (O_GENSYN+1)
#define O_ACLFIL (O_ACLDEF+1)
#define O_HEXCMD (O_ACLFIL+1)
#define O_PIDFIL (O_HEXCMD+1)
#define O_LOCAL  (O_PIDFIL+1)
#define O_HTTPPT (O_LOCAL+1)
#define O_HTMLPA (O_HTTPPT+1)
#define O_LOG    (O_HTMLPA+1)
#define O_LOGARE (O_LOG+1)
#define O_LOGLEV (O_LOGARE+1)
#define O_RAW    (O_LOGLEV+1)
#define O_RAWFIL (O_RAW+1)
#define O_RAWSIZ (O_RAWFIL+1)
#define O_DMPFIL (O_RAWSIZ+1)
#define O_DMPSIZ (O_DMPFIL+1)

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
  {NULL,             0,        NULL,    0, "Device options:", 1 },
  {"device",         'd',      "DEV",   0, "Use DEV as eBUS device (serial or [udp:]ip:port) [/dev/ttyUSB0]", 0 },
  {"nodevicecheck",  'n',      NULL,    0, "Skip serial eBUS device test", 0 },
  {"readonly",       'r',      NULL,    0, "Only read from device, never write to it", 0 },
  {"initsend",       O_INISND, NULL,    0, "Send an initial escape symbol after connecting device", 0 },
  {"latency",        O_DEVLAT, "USEC",  0, "Transfer latency in us [0 for USB, 10000 for IP]", 0 },

  {NULL,             0,        NULL,    0, "Message configuration options:", 2 },
  {"configpath",     'c',      "PATH",  0, "Read CSV config files from PATH [" CONFIG_PATH "]", 0 },
  {"scanconfig",     's',      "ADDR",  OPTION_ARG_OPTIONAL, "Pick CSV config files matching initial scan (ADDR="
      "\"none\" or empty for no initial scan message, \"full\" for full scan, or a single hex address to scan, "
      "default is broadcast ident message). If combined with --checkconfig, you can add scan message data as "
      "arguments for checking a particular scan configuration, e.g. \"FF08070400/0AB5454850303003277201\".", 0 },
  {"checkconfig",    O_CHKCFG, NULL,    0, "Check CSV config files, then stop", 0 },
  {"dumpconfig",     O_DMPCFG, NULL,    0, "Check and dump CSV config files, then stop", 0 },
  {"pollinterval",   O_POLINT, "SEC",   0, "Poll for data every SEC seconds (0=disable) [5]", 0 },

  {NULL,             0,        NULL,    0, "eBUS options:", 3 },
  {"address",        'a',      "ADDR",  0, "Use ADDR as own bus address [31]", 0 },
  {"answer",         O_ANSWER, NULL,    0, "Actively answer to requests from other masters", 0 },
  {"acquiretimeout", O_ACQTIM, "USEC",  0, "Stop bus acquisition after USEC us [9400]", 0 },
  {"acquireretries", O_ACQRET, "COUNT", 0, "Retry bus acquisition COUNT times [3]", 0 },
  {"sendretries",    O_SNDRET, "COUNT", 0, "Repeat failed sends COUNT times [2]", 0 },
  {"receivetimeout", O_RCVTIM, "USEC",  0, "Expect a slave to answer within USEC us [25000]", 0 },
  {"numbermasters",  O_MASCNT, "COUNT", 0, "Expect COUNT masters on the bus, 0 for auto detection [0]", 0 },
  {"generatesyn",    O_GENSYN, NULL,    0, "Enable AUTO-SYN symbol generation", 0 },

  {NULL,             0,        NULL,    0, "Daemon options:", 4 },
  {"accesslevel",    O_ACLDEF, "LEVEL", 0, "Set default access level to LEVEL (\"*\" for everything) [\"\"]", 0 },
  {"aclfile",        O_ACLFIL, "FILE",  0, "Read access control list from FILE", 0 },
  {"foreground",     'f',      NULL,    0, "Run in foreground", 0 },
  {"enablehex",      O_HEXCMD, NULL,    0, "Enable hex command", 0 },
  {"pidfile",        O_PIDFIL, "FILE",  0, "PID file name (only for daemon) [" PID_FILE_NAME "]", 0 },
  {"port",           'p',      "PORT",  0, "Listen for command line connections on PORT [8888]", 0 },
  {"localhost",      O_LOCAL,  NULL,    0, "Listen for command line connections on 127.0.0.1 interface only", 0 },
  {"httpport",       O_HTTPPT, "PORT",  0, "Listen for HTTP connections on PORT, 0 to disable [0]", 0 },
  {"htmlpath",       O_HTMLPA, "PATH",  0, "Path for HTML files served by HTTP port [/var/ebusd/html]", 0 },

  {NULL,             0,        NULL,    0, "Log options:", 5 },
  {"logfile",        'l',      "FILE",  0, "Write log to FILE (only for daemon) [" PACKAGE_LOGFILE "]", 0 },
  {"log",            O_LOG,    "AREAS LEVEL", 0, "Only write log for matching AREA(S) below or equal to LEVEL"
      " (alternative to --logareas/--logevel, may be used multiple times) [all notice]", 0 },
  {"logareas",       O_LOGARE, "AREAS", 0, "Only write log for matching AREA(S): main|network|bus|update|all"
      " [all]", 0 },
  {"loglevel",       O_LOGLEV, "LEVEL", 0, "Only write log below or equal to LEVEL: error|notice|info|debug"
      " [notice]", 0 },

  {NULL,             0,        NULL,    0, "Raw logging options:", 6 },
  {"lograwdata",     O_RAW,    NULL,    0, "Log each received/sent byte on the bus", 0 },
  {"lograwdatafile", O_RAWFIL, "FILE",  0, "Write raw log to FILE [" PACKAGE_LOGFILE "]", 0 },
  {"lograwdatasize", O_RAWSIZ, "SIZE",  0, "Make raw log file no larger than SIZE kB [100]", 0 },

  {NULL,             0,        NULL,    0, "Binary dump options:", 7 },
  {"dump",           'D',      NULL,    0, "Enable binary dump of received bytes", 0 },
  {"dumpfile",       O_DMPFIL, "FILE",  0, "Dump received bytes to FILE [/tmp/" PACKAGE "_dump.bin]", 0 },
  {"dumpsize",       O_DMPSIZ, "SIZE",  0, "Make dump file no larger than SIZE kB [100]", 0 },

  {NULL,             0,        NULL,    0, NULL, 0 },
};

/** the global @a DataFieldTemplates. */
static DataFieldTemplates s_globalTemplates;

/**
 * the loaded @a DataFieldTemplates by path (may also carry
 * @a globalTemplates as replacement for missing file).
 */
static map<string, DataFieldTemplates*> s_templatesByPath;

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct options *opt = (struct options*)state->input;
  result_t result = RESULT_OK;

  switch (key) {
  // Device options:
  case 'd':  // --device=/dev/ttyUSB0
    if (arg == NULL || arg[0] == 0) {
      argp_error(state, "invalid device");
      return EINVAL;
    }
    opt->device = arg;
    break;
  case 'n':  // --nodevicecheck
    opt->noDeviceCheck = true;
    break;
  case 'r':  // --readonly
    opt->readOnly = true;
    if (opt->scanConfig || opt->answer || opt->generateSyn) {
      argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
      return EINVAL;
    }
    break;
  case O_INISND:  // --initsend
    opt->initialSend = true;
    break;
  case O_DEVLAT:  // --latency=10000
    opt->latency = parseInt(arg, 10, 0, 200000, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid latency");
      return EINVAL;
    }
    break;

  // Message configuration options:
  case 'c':  // --configpath=/etc/ebusd
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid configpath");
      return EINVAL;
    }
    opt->configPath = arg;
    break;
  case 's':  // --scanconfig[=ADDR] (ADDR=<empty>|full|<hexaddr>)
    opt->scanConfig = true;
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
      return EINVAL;
    }
    if (opt->pollInterval == 0) {
      argp_error(state, "scanconfig without polling may lead to invalid files included for certain products!");
      return EINVAL;
    }
    if (arg != NULL) {
      if (arg[0] == 0 || strcmp("none", arg) == 0) {
        opt->initialScan = ESC;
      } else if (strcmp("full", arg) == 0) {
        opt->initialScan = SYN;
      } else {
        opt->initialScan = (symbol_t)parseInt(arg, 16, 0x00, 0xff, result);
        if (!isValidAddress(opt->initialScan)) {
          argp_error(state, "invalid initial scan address");
          return EINVAL;
        }
        if (isMaster(opt->initialScan)) {
          opt->initialScan = getSlaveAddress(opt->initialScan);
        }
      }
    }
    break;
  case O_CHKCFG:  // --checkconfig
    opt->checkConfig = true;
    break;
  case O_DMPCFG:  // --dumpconfig
    opt->checkConfig = true;
    opt->dumpConfig = true;
    break;
  case O_POLINT:  // --pollinterval=5
    opt->pollInterval = parseInt(arg, 10, 0, 3600, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid pollinterval");
      return EINVAL;
    }
    if (opt->pollInterval == 0 && opt->scanConfig) {
      argp_error(state, "scanconfig without polling may lead to invalid files included for certain products!");
      return EINVAL;
    }
    break;

  // eBUS options:
  case 'a':  // --address=31
    opt->address = (symbol_t)parseInt(arg, 16, 0, 0xff, result);
    if (result != RESULT_OK || !isMaster(opt->address)) {
      argp_error(state, "invalid address");
      return EINVAL;
    }
    break;
  case O_ANSWER:  // --answer
    opt->answer = true;
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
      return EINVAL;
    }
    break;
  case O_ACQTIM:  // --acquiretimeout=9400
    opt->acquireTimeout = parseInt(arg, 10, 1000, 100000, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid acquiretimeout");
      return EINVAL;
    }
    break;
  case O_ACQRET:  // --acquireretries=3
    opt->acquireRetries = parseInt(arg, 10, 0, 10, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid acquireretries");
      return EINVAL;
    }
    break;
  case O_SNDRET:  // --sendretries=2
    opt->sendRetries = parseInt(arg, 10, 0, 10, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid sendretries");
      return EINVAL;
    }
    break;
  case O_RCVTIM:  // --receivetimeout=25000
    opt->receiveTimeout = parseInt(arg, 10, 1000, 100000, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid receivetimeout");
      return EINVAL;
    }
    break;
  case O_MASCNT:  // --numbermasters=0
    opt->masterCount = parseInt(arg, 10, 0, 25, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid numbermasters");
      return EINVAL;
    }
    break;
  case O_GENSYN:  // --generatesyn
    opt->generateSyn = true;
    if (opt->readOnly) {
      argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
      return EINVAL;
    }
    break;

  // Daemon options:
  case O_ACLDEF:  // --accesslevel=*
    if (arg == NULL) {
      argp_error(state, "invalid accesslevel");
      return EINVAL;
    }
    opt->accessLevel = arg;
    break;
  case O_ACLFIL:  // --aclfile=/etc/ebusd/acl
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
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
  case O_PIDFIL:  // --pidfile=/var/run/ebusd.pid
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid pidfile");
      return EINVAL;
    }
    opt->pidFile = arg;
    break;
  case 'p':  // --port=8888
    opt->port = (uint16_t)parseInt(arg, 10, 1, 65535, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid port");
      return EINVAL;
    }
    break;
  case O_LOCAL:  // --localhost
    opt->localOnly = true;
    break;
  case O_HTTPPT:  // --httpport=0
    opt->httpPort = (uint16_t)parseInt(arg, 10, 1, 65535, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid httpport");
      return EINVAL;
    }
    break;
  case O_HTMLPA:  // --htmlpath=/var/ebusd/html
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid htmlpath");
      return EINVAL;
    }
    opt->htmlPath = arg;
    break;

  // Log options:
  case 'l':  // --logfile=/var/log/ebusd.log
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid logfile");
      return EINVAL;
    }
    opt->logFile = arg;
    break;
  case O_LOG:  // --log=area(s) level
    {
      char* pos = strchr(arg, ' ');
      if (pos == NULL) {
        argp_error(state, "invalid log");
        return EINVAL;
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
    }
    break;
  case O_LOGARE:  // --logareas=all
    opt->logAreas = parseLogFacilities(arg);
    if (opt->logAreas == -1) {
      argp_error(state, "invalid logareas");
      return EINVAL;
    }
    if (opt->multiLog) {
      argp_error(state, "invalid logareas (combined with log)");
      return EINVAL;
    }
    break;
  case O_LOGLEV:  // --loglevel=notice
    opt->logLevel = parseLogLevel(arg);
    if (opt->logLevel == ll_COUNT) {
      argp_error(state, "invalid loglevel");
      return EINVAL;
    }
    if (opt->multiLog) {
      argp_error(state, "invalid loglevel (combined with log)");
      return EINVAL;
    }
    break;

  // Raw logging options:
  case O_RAW:  // --lograwdata
    opt->logRaw = true;
    break;
  case O_RAWFIL:  // --lograwdatafile=/var/log/ebusd.log
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid lograwdatafile");
      return EINVAL;
    }
    opt->logRawFile = arg;
    break;
  case O_RAWSIZ:  // --lograwdatasize=100
    opt->logRawSize = parseInt(arg, 10, 1, 1000000, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid lograwdatasize");
      return EINVAL;
    }
    break;


  // Binary dump options:
  case 'D':  // --dump
    opt->dump = true;
    break;
  case O_DMPFIL:  // --dumpfile=/tmp/ebusd_dump.bin
    if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
      argp_error(state, "invalid dumpfile");
      return EINVAL;
    }
    opt->dumpFile = arg;
    break;
  case O_DMPSIZ:  // --dumpsize=100
    opt->dumpSize = parseInt(arg, 10, 1, 1000000, result);
    if (result != RESULT_OK) {
      argp_error(state, "invalid dumpsize");
      return EINVAL;
    }
    break;

  case ARGP_KEY_ARG:
    if (!opt->checkConfig) {
      argp_error(state, "invalid arguments starting with \"%s\"", arg);
      return EINVAL;
    }
    return ARGP_ERR_UNKNOWN;
  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

void daemonize() {
  // fork off the parent process
  pid_t pid = fork();

  if (pid < 0) {
    logError(lf_main, "fork() failed");
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
    logError(lf_main, "setsid() failed");
    exit(EXIT_FAILURE);
  }

  // Change the current working directory. This prevents the current
  // directory from being locked; hence not being able to remove it.
  if (chdir("/tmp") < 0) {
    logError(lf_main, "daemon chdir() failed");
    exit(EXIT_FAILURE);
  }

  // Close stdin, stdout and stderr
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // create pid file and try to lock it
  pidFile = fopen(opt.pidFile, "w+");

  umask(S_IWGRP | S_IRWXO);  // set permissions of newly created files to 750

  if (pidFile != NULL) {
    setbuf(pidFile, NULL);  // disable buffering
    if (lockf(fileno(pidFile), F_TLOCK, 0) < 0
      || fprintf(pidFile, "%d\n", getpid())  <= 0) {
      fclose(pidFile);
      pidFile = NULL;
    }
  }
  if (pidFile == NULL) {
    logError(lf_main, "can't open pidfile: %s", opt.pidFile);
    exit(EXIT_FAILURE);
  }

  isDaemon = true;
}

void closePidFile() {
  if (pidFile != NULL) {
    if (fclose(pidFile) != 0) {
      return;
    }
    remove(opt.pidFile);
  }
}

/**
 * Helper method performing shutdown.
 */
void shutdown() {
  // stop main loop and all dependent components
  if (s_mainLoop != NULL) {
    delete s_mainLoop;
    s_mainLoop = NULL;
  }
  if (s_messageMap != NULL) {
    delete s_messageMap;
    s_messageMap = NULL;
  }
  // free templates
  for (map<string, DataFieldTemplates*>::iterator it = s_templatesByPath.begin(); it != s_templatesByPath.end(); it++) {
    if (it->second != &s_globalTemplates) {
      delete it->second;
    }
    it->second = NULL;
  }
  s_templatesByPath.clear();

  // reset all signal handlers to default
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  // delete daemon pid file if necessary
  closePidFile();

  logNotice(lf_main, "ebusd stopped");
  closeLogFile();

  exit(EXIT_SUCCESS);
}

/**
 * The signal handling function.
 * @param sig the received signal.
 */
void signalHandler(int sig) {
  switch (sig) {
  case SIGHUP:
    logNotice(lf_main, "SIGHUP received");
    if (!opt.foreground) {
      closeLogFile();
      setLogFile(opt.logFile);
    }
    break;
  case SIGINT:
    logNotice(lf_main, "SIGINT received");
    shutdown();
    break;
  case SIGTERM:
    logNotice(lf_main, "SIGTERM received");
    shutdown();
    break;
  default:
    logNotice(lf_main, "undefined signal %s", strsignal(sig));
    break;
  }
}

/**
 * Collect configuration files matching the prefix and extension from the specified path.
 * @param path the path from which to collect the files.
 * @param prefix the filename prefix the files have to match, or empty.
 * @param extension the filename extension the files have to match.
 * @param files the @a vector to which to add the matching files.
 * @param dirs the @a vector to which to add found directories (without any name check), or NULL to ignore.
 * @param hasTemplates the bool to set when the templates file was found in the path, or NULL to ignore.
 * @return the result code.
 */
static result_t collectConfigFiles(const string path, const string prefix, const string extension,
    vector<string>& files, vector<string>* dirs = NULL, bool* hasTemplates = NULL) {
  DIR* dir = opendir(path.c_str());

  if (dir == NULL) {
    return RESULT_ERR_NOTFOUND;
  }
  dirent* d;
  while ((d = readdir(dir)) != NULL) {
    string name = d->d_name;

    if (name == "." || name == "..") {
      continue;
    }
    const string p = path + "/" + name;
    struct stat stat_buf;

    if (stat(p.c_str(), &stat_buf) != 0) {
      continue;
    }
    if (S_ISDIR(stat_buf.st_mode)) {
      if (dirs != NULL) {
        dirs->push_back(p);
      }
    } else if (S_ISREG(stat_buf.st_mode) && name.length() >= extension.length()
    && name.substr(name.length()-extension.length()) == extension) {
      if (name == "_templates"+extension) {
        if (hasTemplates) {
          *hasTemplates = true;
        }
      } else if (prefix.length() == 0
          || (name.length() >= prefix.length() && name.substr(0, prefix.length()) == prefix)) {
        files.push_back(p);
      }
    }
  }
  closedir(dir);

  return RESULT_OK;
}

DataFieldTemplates* getTemplates(const string filename) {
  string path;
  size_t pos = filename.find_last_of('/');
  if (pos != string::npos) {
    path = filename.substr(0, pos);
  }
  map<string, DataFieldTemplates*>::iterator it = s_templatesByPath.find(path);
  if (it != s_templatesByPath.end()) {
    return it->second;
  }
  return &s_globalTemplates;
}

/**
 * Read the @a DataFieldTemplates for the specified path if necessary.
 * @param path the path from which to read the files.
 * @param extension the filename extension of the files to read.
 * @param available whether the templates file is available in the path.
 * @param verbose whether to verbosely log problems.
 * @return false when the templates for the path were already loaded before, true when the templates for the path were added (independent from @a available).
 * @return the @a DataFieldTemplates.
 */
static bool readTemplates(const string path, const string extension, bool available, bool verbose = false) {
  map<string, DataFieldTemplates*>::iterator it = s_templatesByPath.find(path);
  if (it != s_templatesByPath.end()) {
    return false;
  }
  DataFieldTemplates* templates;
  if (path == opt.configPath || !available) {
    templates = &s_globalTemplates;
  } else {
    templates = new DataFieldTemplates(s_globalTemplates);
  }
  s_templatesByPath[path] = templates;
  if (!available) {
    // global templates are stored as replacement in order to determine whether the directory was already loaded
    return true;
  }
  result_t result = templates->readFromFile(path+"/_templates"+extension, verbose);
  if (result == RESULT_OK) {
    logInfo(lf_main, "read templates in %s", path.c_str());
    return true;
  }
  logError(lf_main, "error reading templates in %s: %s, last error: %s", path.c_str(), getResultCode(result),
           templates->getLastError().c_str());
  return false;
}

/**
 * Read the configuration files from the specified path.
 * @param path the path from which to read the files.
 * @param extension the filename extension of the files to read.
 * @param messages the @a MessageMap to load the messages into.
 * @param recursive whether to load all files recursively.
 * @param verbose whether to verbosely log problems.
 * @return the result code.
 */
static result_t readConfigFiles(const string path, const string extension, MessageMap* messages, bool recursive,
    bool verbose) {
  vector<string> files, dirs;
  bool hasTemplates = false;
  result_t result = collectConfigFiles(path, "", extension, files, &dirs, &hasTemplates);
  if (result != RESULT_OK) {
    return result;
  }
  readTemplates(path, extension, hasTemplates, verbose);
  for (vector<string>::iterator it = files.begin(); it != files.end(); it++) {
    string name = *it;
    logInfo(lf_main, "reading file %s", name.c_str());
    result = messages->readFromFile(name, verbose);
    if (result != RESULT_OK) {
      return result;
    }
  }
  if (recursive) {
    for (vector<string>::iterator it = dirs.begin(); it != dirs.end(); it++) {
      string name = *it;
      logInfo(lf_main, "reading dir  %s", name.c_str());
      result = readConfigFiles(name, extension, messages, true, verbose);
      if (result != RESULT_OK) {
        return result;
      }
    }
  }
  return RESULT_OK;
}

/**
 * Helper method for immediate reading of a @a Message from the bus.
 * @param message the @a Message to read.
 */
void readMessage(Message* message) {
  if (!s_mainLoop || !message) {
    return;
  }
  BusHandler* busHandler = s_mainLoop->getBusHandler();
  result_t result = busHandler->readFromBus(message, "");
  if (result != RESULT_OK) {
    logError(lf_main, "error reading message %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(result));
  }
}

/**
 * Helper method for executing all loaded and resolvable instructions.
 * @param messages the @a MessageMap instance.
 * @param verbose whether to verbosely log all problems.
 */
void executeInstructions(MessageMap* messages, bool verbose) {
  result_t result = messages->resolveConditions(verbose);
  if (result != RESULT_OK) {
    logError(lf_main, "error resolving conditions: %s, last error: %s", getResultCode(result),
        messages->getLastError().c_str());
  }
  ostringstream log;
  result = messages->executeInstructions(log, readMessage);
  if (result != RESULT_OK) {
    logError(lf_main, "error executing instructions: %s, last error: %s, %s", getResultCode(result),
        messages->getLastError().c_str(), log.str().c_str());
  } else if (verbose && log.tellp() > 0) {
    logInfo(lf_main, log.str().c_str());
  }
  logNotice(lf_main, "found messages: %d (%d conditional on %d conditions, %d poll, %d update)", messages->size(),
      messages->sizeConditional(), messages->sizeConditions(), messages->sizePoll(), messages->sizePassive());
}

result_t loadConfigFiles(MessageMap* messages, bool verbose, bool denyRecursive) {
  logInfo(lf_main, "loading configuration files from %s", opt.configPath);
  messages->clear();
  s_globalTemplates.clear();
  for (map<string, DataFieldTemplates*>::iterator it = s_templatesByPath.begin(); it != s_templatesByPath.end();
      it++) {
    if (it->second != &s_globalTemplates) {
      delete it->second;
    }
    it->second = NULL;
  }
  s_templatesByPath.clear();

  result_t result = readConfigFiles(string(opt.configPath), ".csv", messages,
      (!opt.scanConfig || opt.checkConfig) && !denyRecursive, verbose);
  if (result == RESULT_OK) {
    logInfo(lf_main, "read config files");
  } else {
    logError(lf_main, "error reading config files: %s, last error: %s", getResultCode(result),
        messages->getLastError().c_str());
  }
  executeInstructions(messages, verbose);
  return RESULT_OK;
}

result_t loadScanConfigFile(MessageMap* messages, symbol_t address, string& relativeFile, bool verbose) {
  Message* message = messages->getScanMessage(address);
  if (!message) {
    return RESULT_ERR_NOTFOUND;
  }
  SlaveSymbolString& data = message->getLastSlaveData();
  if (data.getDataSize() < 1+5+2+2) {
    logError(lf_main, "unable to load scan config %2.2x: slave part too short", address);
    return RESULT_EMPTY;
  }
  DataFieldSet* identFields = DataFieldSet::getIdentFields();
  string path, prefix, ident;  // path: cfgpath/MANUFACTURER, prefix: ZZ., ident: C[C[C[C[C]]]], SW: xxxx, HW: xxxx
  unsigned int sw = 0, hw = 0;
  ostringstream out;
  size_t offset = 0;
  size_t field = 0;
  result_t result = (*identFields)[field]->read(data, offset, out, 0);  // manufacturer name
  if (result == RESULT_ERR_NOTFOUND) {
    result = (*identFields)[field]->read(data, offset, out, OF_NUMERIC);  // manufacturer name
  }
  if (result == RESULT_OK) {
    path = out.str();
    transform(path.begin(), path.end(), path.begin(), ::tolower);
    path = string(opt.configPath) + "/" + path;
    out.str("");
    out << setw(2) << hex << setfill('0') << nouppercase << static_cast<unsigned>(address) << ".";
    prefix = out.str();
    out.str("");
    out.clear();
    offset += (*identFields)[field++]->getLength(pt_slaveData);
    result = (*identFields)[field]->read(data, offset, out, 0);  // identification string
  }
  if (result == RESULT_OK) {
    ident = out.str();
    out.str("");
    offset += (*identFields)[field++]->getLength(pt_slaveData);
    result = (*identFields)[field]->read(data, offset, sw, 0);  // software version number
    if (result == RESULT_ERR_OUT_OF_RANGE) {
      sw = (data.dataAt(offset) << 16) | data.dataAt(offset+1);  // use hex value instead
      result = RESULT_OK;
    }
  }
  if (result == RESULT_OK) {
    offset += (*identFields)[field++]->getLength(pt_slaveData);
    result = (*identFields)[field]->read(data, offset, hw, 0);  // hardware version number
    if (result == RESULT_ERR_OUT_OF_RANGE) {
      hw = (data.dataAt(offset) << 16) | data.dataAt(offset+1);  // use hex value instead
      result = RESULT_OK;
    }
  }
  if (result != RESULT_OK) {
    logError(lf_main, "unable to load scan config %2.2x: decode field %s %s", address,
             identFields->getName(field).c_str(), getResultCode(result));
    return result;
  }
  vector<string> files;
  bool hasTemplates = false;
  // find files matching MANUFACTURER/ZZ.*csv in cfgpath
  result = collectConfigFiles(path, prefix, ".csv", files, NULL, &hasTemplates);
  if (result != RESULT_OK) {
    logError(lf_main, "unable to load scan config %2.2x: list files in %s %s", address, path.c_str(),
        getResultCode(result));
    return result;
  }
  if (files.empty()) {
    logError(lf_main, "unable to load scan config %2.2x: no file from %s with prefix %s found", address, path.c_str(),
        prefix.c_str());
    return RESULT_ERR_NOTFOUND;
  }
  logDebug(lf_main, "found %d matching scan config files from %s with prefix %s: %s", files.size(), path.c_str(),
      prefix.c_str(), getResultCode(result));
  for (string::iterator it = ident.begin(); it != ident.end(); it++) {
    if (::isspace(*it)) {
      ident.erase(it--);
    } else {
      *it = static_cast<char>(::tolower(*it));
    }
  }
  // complete name: cfgpath/MANUFACTURER/ZZ[.C[C[C[C[C]]]]][.circuit][.suffix][.*][.SWxxxx][.HWxxxx][.*].csv
  size_t bestMatch = 0;
  string best;
  for (vector<string>::iterator it = files.begin(); it != files.end(); it++) {
    string name = *it;
    symbol_t checkDest;
    string checkIdent, useCircuit, useSuffix;
    unsigned int checkSw, checkHw;
    if (!FileReader::extractDefaultsFromFilename(name.substr(path.length()+1), checkDest, checkIdent, useCircuit,
        useSuffix, checkSw, checkHw)) {
      continue;
    }
    if (address != checkDest || (checkSw != UINT_MAX && sw != checkSw) || (checkHw != UINT_MAX && hw != checkHw)) {
      continue;
    }
    size_t match = 1;
    if (!checkIdent.empty()) {
      string remain = ident;
      bool matches = false;
      while (remain.length() > 0 && remain.length() >= checkIdent.length()) {
        if (checkIdent == remain) {
          matches = true;
          break;
        }
        if (remain[remain.length()-1] < '0' || remain[remain.length()-1] > '9') {
          break;
        }
        remain.erase(remain.length()-1);  // remove trailing digit
      }
      if (!matches) {
        continue;  // IDENT mismatch
      }
      match += remain.length();
    }
    if (match >= bestMatch) {
      bestMatch = match;
      best = name;
    }
    break;
  }

  if (best.empty()) {
    logError(lf_main,
        "unable to load scan config %2.2x: no file from %s with prefix %s matches ID \"%s\", SW%4.4d, HW%4.4d",
        address, path.c_str(), prefix.c_str(), ident.c_str(), sw, hw);
    return RESULT_ERR_NOTFOUND;
  }

  // found the right file. load the templates if necessary, then load the file itself
  bool readCommon = readTemplates(path, ".csv", hasTemplates, opt.checkConfig);
  if (readCommon) {
    result = collectConfigFiles(path, "", ".csv", files);
    if (result == RESULT_OK && !files.empty()) {
      for (vector<string>::iterator it = files.begin(); it != files.end(); it++) {
        string name = *it;
        name = name.substr(path.length()+1, name.length()-path.length()-strlen(".csv"));  // *.
        if (name == "_templates.") {  // skip templates
          continue;
        }
        if (name.length() < 3 || name.find_first_of('.') != 2) {  // different from the scheme "ZZ."
          name = *it;
          result = messages->readFromFile(name, opt.checkConfig);
          if (result == RESULT_OK) {
            logNotice(lf_main, "read common config file %s", name.c_str());
          } else {
            logError(lf_main, "error reading common config file %s: %s", name.c_str(), getResultCode(result));
          }
        }
      }
    }
  }
  result = messages->readFromFile(best, opt.checkConfig, "", ident);
  if (result != RESULT_OK) {
    logError(lf_main, "error reading scan config file %s for ID \"%s\", SW%4.4d, HW%4.4d: %s", best.c_str(),
        ident.c_str(), sw, hw, getResultCode(result));
    return result;
  }
  logNotice(lf_main, "read scan config file %s for ID \"%s\", SW%4.4d, HW%4.4d", best.c_str(), ident.c_str(), sw, hw);
  relativeFile = best.substr(strlen(opt.configPath)+1);
  executeInstructions(messages, verbose);
  return RESULT_OK;
}


/**
 * Main function.
 * @param argc the number of command line arguments.
 * @param argv the command line arguments.
 * @return the exit code.
 */
int main(int argc, char* argv[]) {
  struct argp aargp = { argpoptions, parse_opt, NULL, argpdoc, datahandler_getargs(), NULL, NULL };
  int arg_index = -1;
  setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);

  if (argp_parse(&aargp, argc, argv, ARGP_IN_ORDER, &arg_index, &opt) != 0) {
    logError(lf_main, "invalid arguments");
    return EINVAL;
  }

  if (opt.logAreas != -1 || opt.logLevel != ll_COUNT) {
    setFacilitiesLogLevel(LF_ALL, ll_none);
    setFacilitiesLogLevel(opt.logAreas, opt.logLevel);
  }

  s_messageMap = new MessageMap(opt.checkConfig && opt.scanConfig && arg_index >= argc);
  if (opt.checkConfig) {
    logNotice(lf_main, PACKAGE_STRING "." REVISION " performing configuration check...");

    result_t result = loadConfigFiles(s_messageMap, true, opt.scanConfig && arg_index < argc);

    while (result == RESULT_OK && opt.scanConfig && arg_index < argc) {
      // check scan config for each passed ident message
      string arg = argv[arg_index++];
      size_t pos = arg.find_first_of('/');
      if (pos == string::npos) {
        logError(lf_main, "invalid scan message %s: missing \"/\"", arg.c_str());
        continue;
      }
      MasterSymbolString master;
      SlaveSymbolString slave;
      result_t res = master.parseHex(arg.substr(0, pos));
      if (res == RESULT_OK) {
        res = slave.parseHex(arg.substr(pos+1));
      }
      if (res != RESULT_OK) {
        logError(lf_main, "invalid scan message %s: %s", arg.c_str(), getResultCode(res));
        continue;
      }
      if (master.size() < 5) {  // skip QQ ZZ PB SB NN
        logError(lf_main, "invalid scan message %s: master part too short", arg.c_str());
        continue;
      }
      symbol_t address = master[1];
      Message* message = s_messageMap->getScanMessage(address);
      if (!message) {
        logError(lf_main, "invalid scan address %2.2x", address);
      } else {
        message->storeLastData(master, slave);
        string file;
        res = loadScanConfigFile(s_messageMap, address, file, true);
        if (res == RESULT_OK) {
          logInfo(lf_main, "scan config %2.2x: file %s loaded", address, file.c_str());
        }
      }
    }
    if (result == RESULT_OK && opt.dumpConfig) {
      logNotice(lf_main, "configuration dump:");
      s_messageMap->dump(cout, true);
    }
    shutdown();
    return 0;
  }


  // open the device
  Device *device = Device::create(opt.device, !opt.noDeviceCheck, opt.readOnly, opt.initialSend);
  if (device == NULL) {
    logError(lf_main, "unable to create device %s", opt.device);
    return EINVAL;
  }

  if (!opt.foreground) {
    setLogFile(opt.logFile);
    daemonize();  // make me daemon
  }

  // trap signals that we expect to receive
  signal(SIGHUP, signalHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  logNotice(lf_main, PACKAGE_STRING "." REVISION " started");

  // create the MainLoop and start it
  s_mainLoop = new MainLoop(opt, device, s_messageMap);
  s_mainLoop->start("mainloop");

  // load configuration files
  loadConfigFiles(s_messageMap);
  if (s_messageMap->sizeConditions() > 0 && opt.pollInterval == 0) {
    logError(lf_main, "conditions require a poll interval > 0");
  }
  // wait for end of MainLoop
  s_mainLoop->join();

  // shutdown
  shutdown();
  return 0;
}

}  // namespace ebusd

int main(int argc, char* argv[]) {
  return ebusd::main(argc, argv);
}

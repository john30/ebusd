/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
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
#include <config.h>
#endif

#include "main.h"
#include "mainloop.h"
#include "bushandler.h"
#include "log.h"
#include <stdlib.h>
#include <argp.h>
#include <csignal>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

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
	"/dev/ttyUSB0", // device
	false, // noDeviceCheck
	false, // readonly
	CONFIG_PATH, // configPath
	false, // scanConfig
	0, // checkConfig
	5, // pollInterval
	0xFF, // address
	false, // answer
	9400, // acquireTimeout
	3, // acquireRetries
	2, // sendRetries
	SLAVE_RECV_TIMEOUT, // receiveTimeout
	0, // masterCount
	false, // generateSyn
	false, // foreground
	8888, // port
	false, // localOnly
	0, // httpPort
	"/var/ebusd/html", // htmlPath
	PACKAGE_LOGFILE, // logFile
	false, // logRaw
	false, // dump
	"/tmp/ebus_dump.bin", // dumpFile
	100 // dumpSize
};

/** the @a MessageMap instance, or NULL. */
static MessageMap* s_messageMap = NULL;

/** the @a MainLoop instance, or NULL. */
static MainLoop* s_mainLoop = NULL;

/** the version string of the program. */
const char *argp_program_version = ""PACKAGE_STRING "." REVISION"";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = ""PACKAGE_BUGREPORT"";

/** the documentation of the program. */
static const char argpdoc[] =
	"A daemon for communication with eBUS heating systems.";

#define O_CHKCFG 1
#define O_DMPCFG (O_CHKCFG+1)
#define O_POLINT (O_DMPCFG+1)
#define O_ANSWER (O_POLINT+1)
#define O_ACQTIM (O_ANSWER+1)
#define O_ACQRET (O_ACQTIM+1)
#define O_SNDRET (O_ACQRET+1)
#define O_RCVTIM (O_SNDRET+1)
#define O_MASCNT (O_RCVTIM+1)
#define O_GENSYN (O_MASCNT+1)
#define O_LOCAL  (O_GENSYN+1)
#define O_HTTPPT (O_LOCAL+1)
#define O_HTMLPA (O_HTTPPT+1)
#define O_LOGARE (O_HTMLPA+1)
#define O_LOGLEV (O_LOGARE+1)
#define O_LOGRAW (O_LOGLEV+1)
#define O_DMPFIL (O_LOGRAW+1)
#define O_DMPSIZ (O_DMPFIL+1)

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
	{NULL,             0,        NULL,    0, "Device options:", 1 },
	{"device",         'd',      "DEV",   0, "Use DEV as eBUS device (serial device or ip:port) [/dev/ttyUSB0]", 0 },
	{"nodevicecheck",  'n',      NULL,    0, "Skip serial eBUS device test", 0 },
	{"readonly",       'r',      NULL,    0, "Only read from device, never write to it", 0 },

	{NULL,             0,        NULL,    0, "Message configuration options:", 2 },
	{"configpath",     'c',      "PATH",  0, "Read CSV config files from PATH [" CONFIG_PATH "]", 0 },
	{"scanconfig",     's',      NULL,    0, "Pick CSV config files matching initial scan. If combined with --checkconfig, you can add scan message data as arguments for checking a particular scan configuration, e.g. \"FF08070400/0AB5454850303003277201\".", 0 },
	{"checkconfig",    O_CHKCFG, NULL,    0, "Check CSV config files, then stop", 0 },
	{"dumpconfig",     O_DMPCFG, NULL,    0, "Check and dump CSV config files, then stop", 0 },
	{"pollinterval",   O_POLINT, "SEC",   0, "Poll for data every SEC seconds (0=disable) [5]", 0 },

	{NULL,             0,        NULL,    0, "eBUS options:", 3 },
	{"address",        'a',      "ADDR",  0, "Use ADDR as own bus address [FF]", 0 },
	{"answer",         O_ANSWER, NULL,    0, "Actively answer to requests from other masters", 0 },
	{"acquiretimeout", O_ACQTIM, "USEC",  0, "Stop bus acquisition after USEC us [9400]", 0 },
	{"acquireretries", O_ACQRET, "COUNT", 0, "Retry bus acquisition COUNT times [3]", 0 },
	{"sendretries",    O_SNDRET, "COUNT", 0, "Repeat failed sends COUNT times [2]", 0 },
	{"receivetimeout", O_RCVTIM, "USEC",  0, "Expect a slave to answer within USEC us [15000]", 0 },
	{"numbermasters",  O_MASCNT, "COUNT", 0, "Expect COUNT masters on the bus, 0 for auto detection [0]", 0 },
	{"generatesyn",    O_GENSYN, NULL,    0, "Enable AUTO-SYN symbol generation", 0 },

	{NULL,             0,        NULL,    0, "Daemon options:", 4 },
	{"foreground",     'f',      NULL,    0, "Run in foreground", 0 },
	{"port",           'p',      "PORT",  0, "Listen for command line connections on PORT [8888]", 0 },
	{"localhost",      O_LOCAL,  NULL,    0, "Listen for command line connections on 127.0.0.1 interface only", 0 },
	{"httpport",       O_HTTPPT, "PORT",  0, "Listen for HTTP connections on PORT, 0 to disable [0]", 0 },
	{"htmlpath",       O_HTMLPA, "PATH",  0, "Path for HTML files served by HTTP port [/var/ebusd/html]", 0 },

	{NULL,             0,        NULL,    0, "Log options:", 5 },
	{"logfile",        'l',      "FILE",  0, "Write log to FILE (only for daemon) [" PACKAGE_LOGFILE "]", 0 },
	{"logareas",       O_LOGARE, "AREAS", 0, "Only write log for matching AREA(S): main,network,bus,update,all [all]", 0 },
	{"loglevel",       O_LOGLEV, "LEVEL", 0, "Only write log below or equal to LEVEL: error/notice/info/debug [notice]", 0 },
	{"lograwdata",     O_LOGRAW, NULL,    0, "Log each received/sent byte on the bus", 0 },

	{NULL,             0,        NULL,    0, "Dump options:", 6 },
	{"dump",           'D',      NULL,    0, "Enable dump of received bytes", 0 },
	{"dumpfile",       O_DMPFIL, "FILE",  0, "Dump received bytes to FILE [/tmp/ebus_dump.bin]", 0 },
	{"dumpsize",       O_DMPSIZ, "SIZE",  0, "Make dump files no larger than SIZE kB [100]", 0 },

	{NULL,             0,        NULL,    0, NULL, 0 },
};

/** the global @a DataFieldTemplates. */
static DataFieldTemplates globalTemplates;

/**
 * the loaded @a DataFieldTemplates by path (may also carry
 * @a globalTemplates as replacement for missing file).
 */
static map<string, DataFieldTemplates*> templatesByPath;

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or NULL.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct options *opt = (struct options*)state->input;
	result_t result = RESULT_OK;
	switch (key) {

	// Device options:
	case 'd': // --device=/dev/ttyUSB0
		if (arg == NULL || arg[0] == 0) {
			argp_error(state, "invalid device");
			return EINVAL;
		}
		opt->device = arg;
		break;
	case 'n': // --nodevicecheck
		opt->noDeviceCheck = true;
		break;
	case 'r': // --readonly
		opt->readonly = true;
		if (opt->scanConfig || opt->answer || opt->generateSyn) {
			argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
			return EINVAL;
		}
		break;

	// Message configuration options:
	case 'c': // --configpath=/etc/ebusd
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid configpath");
			return EINVAL;
		}
		opt->configPath = arg;
		break;
	case 's': // --scanconfig
		opt->scanConfig = true;
		if (opt->readonly) {
			argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
			return EINVAL;
		}
		break;
	case O_CHKCFG: // --checkconfig
		if (opt->checkConfig==0)
			opt->checkConfig = 1;
		break;
	case O_DMPCFG: // --dumpconfig
		opt->checkConfig = 2;
		break;
	case O_POLINT: // --pollinterval=5
		opt->pollInterval = parseInt(arg, 10, 0, 3600, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid pollinterval");
			return EINVAL;
		}
		break;

	// eBUS options:
	case 'a': // --address=FF
		opt->address = (unsigned char)parseInt(arg, 16, 0, 0xff, result);
		if (result != RESULT_OK || !isMaster(opt->address)) {
			argp_error(state, "invalid address");
			return EINVAL;
		}
		break;
	case O_ANSWER: // --answer
		opt->answer = true;
		if (opt->readonly) {
			argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
			return EINVAL;
		}
		break;
	case O_ACQTIM: // --acquiretimeout=9400
		opt->acquireTimeout = parseInt(arg, 10, 1000, 100000, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid acquiretimeout");
			return EINVAL;
		}
		break;
	case O_ACQRET: // --acquireretries=2
		opt->acquireRetries = parseInt(arg, 10, 0, 10, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid acquireretries");
			return EINVAL;
		}
		break;
	case O_SNDRET: // --sendretries=2
		opt->sendRetries = parseInt(arg, 10, 0, 10, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid sendretries");
			return EINVAL;
		}
		break;
	case O_RCVTIM: // --receivetimeout=15000
		opt->receiveTimeout = parseInt(arg, 10, 1000, 100000, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid receivetimeout");
			return EINVAL;
		}
		break;
	case O_MASCNT: // --numbermasters=0
		opt->masterCount = parseInt(arg, 10, 0, 25, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid numbermasters");
			return EINVAL;
		}
		break;
	case O_GENSYN: // --generatesyn
		opt->generateSyn = true;
		if (opt->readonly) {
			argp_error(state, "cannot combine readonly with scanconfig/answer/generatesyn");
			return EINVAL;
		}
		break;

	// Daemon options:
	case 'f': // --foreground
		opt->foreground = true;
		break;
	case 'p': // --port=8888
		opt->port = (uint16_t)parseInt(arg, 10, 1, 65535, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid port");
			return EINVAL;
		}
		break;
	case O_LOCAL: // --localhost
		opt->localOnly = true;
		break;
	case O_HTTPPT: // --httpport
		opt->httpPort = (uint16_t)parseInt(arg, 10, 1, 65535, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid port");
			return EINVAL;
		}
		break;
	case O_HTMLPA: // --htmlpath=/var/ebusd/html
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid htmlpath");
			return EINVAL;
		}
		opt->htmlPath = arg;
		break;

	// Log options:
	case 'l': // --logfile=/var/log/ebusd.log
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid logfile");
			return EINVAL;
		}
		opt->logFile = arg;
		break;
	case O_LOGARE: // --logareas=all
		if (!setLogFacilities(arg)) {
			argp_error(state, "invalid logareas");
			return EINVAL;
		}
		break;
	case O_LOGLEV: // --loglevel=event
		if (!setLogLevel(arg)) {
			argp_error(state, "invalid loglevel");
			return EINVAL;
		}
		break;
	case O_LOGRAW:  // --lograwdata
		opt->logRaw = true;
		break;

	// Dump options:
	case 'D':  // --dump
		opt->dump = true;
		break;
	case O_DMPFIL: // --dumpfile=/tmp/ebus_dump.bin
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid dumpfile");
			return EINVAL;
		}
		opt->dumpFile = arg;
		break;
	case O_DMPSIZ: // --dumpsize=100
		opt->dumpSize = parseInt(arg, 10, 1, 1000000, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid dumpsize");
			return EINVAL;
		}
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

void daemonize()
{
	// fork off the parent process
	pid_t pid = fork();

	if (pid < 0) {
		logError(lf_main, "fork() failed");
		exit(EXIT_FAILURE);
	}

	// If we got a good PID, then we can exit the parent process
	if (pid > 0)
		exit(EXIT_SUCCESS);

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
	pidFile = fopen(PID_FILE_NAME, "w+");

	umask(S_IWGRP | S_IRWXO); // set permissions of newly created files to 750

	if (pidFile != NULL) {
		setbuf(pidFile, NULL); // disable buffering
		if (lockf(fileno(pidFile), F_TLOCK, 0) < 0
			|| fprintf(pidFile, "%d\n", getpid()) <=0) {
			fclose(pidFile);
			pidFile = NULL;
		}
	}
	if (pidFile == NULL) {
		logError(lf_main, "can't open pidfile: " PID_FILE_NAME);
		exit(EXIT_FAILURE);
	}

	isDaemon = true;
}

void closePidFile()
{
	if (pidFile != NULL) {
		if (fclose(pidFile) != 0)
			return;

		remove(PID_FILE_NAME);
	}
}

/**
 * Helper method performing shutdown.
 */
void shutdown()
{
	// stop main loop and all dependent components
	if (s_mainLoop != NULL) {
		delete s_mainLoop;
		s_mainLoop = NULL;
	}
	if (s_messageMap!=NULL) {
		delete s_messageMap;
		s_messageMap = NULL;
	}
	// free templates
	for (map<string, DataFieldTemplates*>::iterator it = templatesByPath.begin(); it != templatesByPath.end(); it++) {
		if (it->second!=&globalTemplates)
			delete it->second;
		it->second = NULL;
	}
	templatesByPath.clear();

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
void signalHandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		logNotice(lf_main, "SIGHUP received");
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
		vector<string>& files, vector<string>* dirs=NULL, bool* hasTemplates=NULL)
{

	DIR* dir = opendir(path.c_str());

	if (dir == NULL)
		return RESULT_ERR_NOTFOUND;

	dirent* d;
	while ((d = readdir(dir)) != NULL) {
		string name = d->d_name;

		if (name == "." || name == "..")
			continue;

		const string p = path + "/" + name;
		struct stat stat_buf;

		if (stat(p.c_str(), &stat_buf) != 0)
			continue;

		if (S_ISDIR(stat_buf.st_mode)) {
			if (dirs!=NULL)
				dirs->push_back(p);
		} else if (S_ISREG(stat_buf.st_mode) && name.length()>=extension.length()
		&& name.substr(name.length()-extension.length())==extension) {
			if (name=="_templates"+extension) {
				if (hasTemplates) {
					*hasTemplates = true;
				}
			} else if (prefix.length()==0 || (name.length()>=prefix.length() && name.substr(0, prefix.length())==prefix)) {
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
	if (pos!=string::npos)
		path = filename.substr(0, pos);
	map<string, DataFieldTemplates*>::iterator it = templatesByPath.find(path);
	if (it!=templatesByPath.end()) {
		return it->second;
	}
	return &globalTemplates;
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
static bool readTemplates(const string path, const string extension, bool available, bool verbose=false) {
	map<string, DataFieldTemplates*>::iterator it = templatesByPath.find(path);
	if (it!=templatesByPath.end()) {
		return false;
	}
	DataFieldTemplates* templates;
	if (path==opt.configPath || !available) {
		templates = &globalTemplates;
	} else {
		templates = new DataFieldTemplates(globalTemplates);
	}
	templatesByPath[path] = templates;
	if (!available) {
		// global templates are stored as replacement in order to determine whether the directory was already loaded
		return true;
	}
	result_t result = templates->readFromFile(path+"/_templates"+extension, verbose);
	if (result == RESULT_OK)
		logInfo(lf_main, "read templates in %s", path.c_str());
	else
		logError(lf_main, "error reading templates in %s: %s, %s", path.c_str(), getResultCode(result), templates->getLastError().c_str());
	return templates;
}

/**
 * Read the configuration files from the specified path.
 * @param path the path from which to read the files.
 * @param extension the filename extension of the files to read.
 * @param templates the available @a DataFieldTemplates.
 * @param messages the @a MessageMap to load the messages into.
 * @param recursive whether to load all files recursively.
 * @param verbose whether to verbosely log problems.
 * @return the result code.
 */
static result_t readConfigFiles(const string path, const string extension, MessageMap* messages, bool recursive, bool verbose)
{
	vector<string> files, dirs;
	bool hasTemplates = false;
	result_t result = collectConfigFiles(path, "", extension, files, &dirs, &hasTemplates);
	if (result!=RESULT_OK)
		return result;

	readTemplates(path, extension, hasTemplates, verbose);
	for (vector<string>::iterator it = files.begin(); it != files.end(); it++) {
		string name = *it;
		logInfo(lf_main, "reading file %s", name.c_str());
		result_t result = messages->readFromFile(name, verbose);
		if (result != RESULT_OK)
			return result;
	}
	if (recursive) {
		for (vector<string>::iterator it = dirs.begin(); it != dirs.end(); it++) {
			string name = *it;
			logInfo(lf_main, "reading dir  %s", name.c_str());
			result_t result = readConfigFiles(name, extension, messages, true, verbose);
			if (result != RESULT_OK)
				return result;
		}
	}
	return RESULT_OK;
};

result_t loadConfigFiles(MessageMap* messages, bool verbose, bool denyRecursive)
{
	logInfo(lf_main, "loading configuration files from %s", opt.configPath);
	messages->clear();
	globalTemplates.clear();
	for (map<string, DataFieldTemplates*>::iterator it = templatesByPath.begin(); it != templatesByPath.end(); it++) {
		if (it->second!=&globalTemplates)
			delete it->second;
		it->second = NULL;
	}
	templatesByPath.clear();

	result_t result = readConfigFiles(string(opt.configPath), ".csv", messages, (!opt.scanConfig || opt.checkConfig) && !denyRecursive, verbose);
	if (result == RESULT_OK)
		logInfo(lf_main, "read config files");
	else
		logError(lf_main, "error reading config files: %s, %s", getResultCode(result), messages->getLastError().c_str());

	result = messages->resolveConditions(verbose);
	if (result != RESULT_OK)
		logError(lf_main, "error resolving conditions: %s, %s", getResultCode(result), messages->getLastError().c_str());

	logNotice(lf_main, "found messages: %d (%d conditional on %d conditions, %d poll, %d update)", messages->size(), messages->sizeConditional(), messages->sizeConditions(), messages->sizePoll(), messages->sizePassive());

	return result;
}

result_t loadScanConfigFile(MessageMap* messages, unsigned char address, SymbolString& data, string& relativeFile)
{
	PartType partType;
	if (isMaster(address)) {
		address = (unsigned char)(data[0]+5); // slave address of sending master
		partType = pt_masterData;
		if (data.size()<5+1+5+2+2) { // skip QQ ZZ PB SB NN
			logError(lf_main, "unable to load scan config %2.2x: master part too short", address);
			return RESULT_EMPTY;
		}
	} else {
		partType = pt_slaveData;
		if (data.size()<1+1+5+2+2) { // skip NN
			logError(lf_main, "unable to load scan config %2.2x: slave part too short", address);
			return RESULT_EMPTY;
		}
	}
	DataFieldSet* identFields = DataFieldSet::getIdentFields();
	string path, prefix, ident, sw, hw; // path: cfgpath/MANUFACTURER, prefix: ZZ., ident: C[C[C[C[C]]]], SW: xxxx, HW: xxxx
	ostringstream out;
	unsigned char offset = 0;
	size_t field = 0;
	result_t result = (*identFields)[field]->read(partType, data, offset, out, 0); // manufacturer name
	if (result==RESULT_ERR_NOTFOUND)
		result = (*identFields)[field]->read(partType, data, offset, out, OF_NUMERIC); // manufacturer name
	if (result==RESULT_OK) {
		path = out.str();
		transform(path.begin(), path.end(), path.begin(), ::tolower);
		path = string(opt.configPath) + "/" + path;
		out.str("");
		out << setw(2) << hex << setfill('0') << nouppercase << static_cast<unsigned>(address) << ".";
		prefix = out.str();
		out.str("");
		out.clear();
		offset = (unsigned char)(offset+(*identFields)[field++]->getLength(partType));
		result = (*identFields)[field]->read(partType, data, offset, out, 0); // identification string
	}
	if (result==RESULT_OK) {
		ident = out.str();
		out.str("");
		offset = (unsigned char)(offset+(*identFields)[field++]->getLength(partType));
		result = (*identFields)[field]->read(partType, data, offset, out, 0); // software version number
	}
	if (result==RESULT_OK) {
		sw = out.str();
		out.str("");
		offset = (unsigned char)(offset+(*identFields)[field++]->getLength(partType));
		result = (*identFields)[field]->read(partType, data, offset, out, 0); // hardware version number
	}
	if (result!=RESULT_OK) {
		logError(lf_main, "unable to load scan config %2.2x: decode %s", address, getResultCode(result));
		return result;
	}
	vector<string> files;
	bool hasTemplates = false;
	hw = out.str();
	// find files matching MANUFACTURER/ZZ.*csv in cfgpath
	result = collectConfigFiles(path, prefix, ".csv", files, NULL, &hasTemplates);
	logDebug(lf_main, "found %d matching scan config files from %s with prefix %s: %s", files.size(), path.c_str(), prefix.c_str(), getResultCode(result));
	if (result!=RESULT_OK) {
		logError(lf_main, "unable to load scan config %2.2x: list files in %s %s", address, path.c_str(), getResultCode(result));
		return result;
	}
	if (files.empty()) {
		logError(lf_main, "unable to load scan config %2.2x: no file from %s with prefix %s found", address, path.c_str(), prefix.c_str());
		return RESULT_ERR_NOTFOUND;
	}

	// complete name: cfgpath/MANUFACTURER/ZZ[.C[C[C[C[C]]]]][.index][.*][.SWxxxx][.HWxxxx][.*].csv
	for (string::iterator it = ident.begin(); it!=ident.end(); it++) {
		if (::isspace(*it)) {
			ident.erase(it--);
		} else {
			*it = (char)::tolower(*it);
		}
	}
	size_t prefixLen = path.length()+1+prefix.length()-1;
	size_t bestMatch = 0;
	string best;
	for (vector<string>::iterator it = files.begin(); it!=files.end(); it++) {
		string name = *it;
		name = name.substr(prefixLen, name.length()-prefixLen+1-strlen(".csv")); // .*.
		size_t match = 1;
		if (name.length()>2) { // more than just "."
			size_t pos = name.rfind(".SW"); // check for ".SWxxxx."
			if (pos!=string::npos && name.find(".", pos+1)==pos+7) {
				if (name.substr(pos+3, 4)==sw)
					match += 6;
				else {
					continue; // SW mismatch
				}
			}
			pos = name.rfind(".HW"); // check for ".HWxxxx."
			if (pos!=string::npos && name.find(".", pos+1)==pos+7) {
				if (name.substr(pos+3, 4)==hw)
					match += 6;
				else {
					continue; // HW mismatch
				}
			}
			pos = name.find(".", 1); // check for ".C[C[C[C[C]]]]."
			if (ident.length()>0 && pos!=string::npos && pos>1 && pos<=6) { // up to 5 chars between two "."s, immediately after "ZZ."
				string check = name.substr(1, pos-1);
				string remain = ident;
				bool matches = false;
				while (remain.length()>0 && remain.length()>=check.length()) {
					if (check==remain) {
						matches = true;
						break;
					}
					if (remain[remain.length()-1]<'0' || remain[remain.length()-1]>'9')
						break;
					remain.erase(remain.length()-1); // remove trailing digit
				}
				if (matches)
					match += remain.length();
				else {
					continue; // IDENT mismatch
				}
			}
		}
		if (match>=bestMatch) {
			bestMatch = match;
			best = *it;
		}
	}

	if (best.length()==0) {
		logError(lf_main, "unable to load scan config %2.2x: no file from %s with prefix %s matches ID \"%s\", SW%s, HW%s", address, path.c_str(), prefix.c_str(), ident.c_str(), sw.c_str(), hw.c_str());
		return RESULT_ERR_NOTFOUND;
	}

	// found the right file. load the templates if necessary, then load the file itself
	bool readCommon = readTemplates(path, ".csv", hasTemplates, opt.checkConfig);
	if (readCommon) {
		result = collectConfigFiles(path, "", ".csv", files);
		if (result==RESULT_OK && !files.empty()) {
			for (vector<string>::iterator it = files.begin(); it!=files.end(); it++) {
				string name = *it;
				name = name.substr(path.length()+1, name.length()-path.length()-strlen(".csv")); // *.
				if (name=="_templates.") // skip templates
					continue;
				if (name.length()<3 || name.find_first_of('.')!=2) { // different from the scheme "ZZ."
					name = *it;
					result = messages->readFromFile(name, opt.checkConfig);
					if (result==RESULT_OK)
						logNotice(lf_main, "read common config file %s", name.c_str());
					else
						logError(lf_main, "error reading common config file %s: %s", name.c_str(), getResultCode(result));
				}
			}
		}
	}
	result = messages->readFromFile(best, opt.checkConfig);
	if (result!=RESULT_OK) {
		logError(lf_main, "error reading scan config file %s for ID \"%s\", SW%s, HW%s: %s", best.c_str(), ident.c_str(), sw.c_str(), hw.c_str(), getResultCode(result));
		return result;
	}
	logNotice(lf_main, "read scan config file %s for ID \"%s\", SW%s, HW%s", best.c_str(), ident.c_str(), sw.c_str(), hw.c_str());
	result = messages->resolveConditions(false);
	if (result != RESULT_OK)
		logError(lf_main, "error resolving conditions: %s, %s", getResultCode(result), messages->getLastError().c_str());

	logNotice(lf_main, "found messages: %d (%d conditional on %d conditions, %d poll, %d update)", messages->size(), messages->sizeConditional(), messages->sizeConditions(), messages->sizePoll(), messages->sizePassive());
	relativeFile = best.substr(strlen(opt.configPath)+1);
	return RESULT_OK;
}


/**
 * Create a log message for a received/sent raw data byte.
 * @param byte the raw data byte.
 * @param received true if the byte was received, false if it was sent.
 */
static void logRawData(const unsigned char byte, bool received)
{
	if (received)
		logNotice(lf_bus, "<%02x", byte);
	else
		logNotice(lf_bus, ">%02x", byte);
}

/**
 * Main method.
 *
 * @param argc the number of command line arguments.
 * @param argv the command line arguments.
 */
int main(int argc, char* argv[])
{
	struct argp argp = { argpoptions, parse_opt, NULL, argpdoc, NULL, NULL, NULL };
	int arg_index = -1;
	setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &arg_index, &opt) != 0)
		return EINVAL;

	s_messageMap = new MessageMap(opt.checkConfig && opt.scanConfig && arg_index >= argc);
	if (opt.checkConfig) {
		logNotice(lf_main, "Performing configuration check...");

		result_t result = loadConfigFiles(s_messageMap, true, opt.scanConfig && arg_index < argc);

		while (result == RESULT_OK && opt.scanConfig && arg_index < argc) {
			// check scan config for each passed ident message
			string arg = argv[arg_index++];
			size_t pos = arg.find_first_of('/');
			if (pos==string::npos) {
				logError(lf_main, "invalid scan message %s: missing \"/\"", arg.c_str());
				continue;
			}
			SymbolString master(false), slave(false);
			result_t res = master.parseHex(arg.substr(0, pos));
			if (res==RESULT_OK) {
				res = slave.parseHex(arg.substr(pos+1));
			}
			if (res!=RESULT_OK) {
				logError(lf_main, "invalid scan message %s: %s", arg.c_str(), getResultCode(res));
				continue;
			}
			if (master.size()<5) { // skip QQ ZZ PB SB NN
				logError(lf_main, "invalid scan message %s: master part too short", arg.c_str());
				continue;
			}
			unsigned char address = master[1];
			string file;
			res = loadScanConfigFile(s_messageMap, address, slave, file);
			if (res==RESULT_OK)
				logInfo(lf_main, "scan config %2.2x: file %s loaded", address, file.c_str());
		}
		if (result == RESULT_OK && opt.checkConfig > 1) {
			logNotice(lf_main, "Configuration dump:");
			s_messageMap->dump(cout, true);
		}
		delete s_messageMap;
		s_messageMap = NULL;
		globalTemplates.clear();

		return 0;
	}

	if (arg_index < argc)
		return EINVAL;

	// open the device
	Device *device = Device::create(opt.device, !opt.noDeviceCheck, opt.readonly, &logRawData);
	if (device == NULL) {
		logError(lf_main, "unable to create device %s", opt.device);
		return EINVAL;
	}

	if (!opt.foreground) {
		setLogFile(opt.logFile);
		daemonize(); // make me daemon
	}

	// trap signals that we expect to receive
	signal(SIGHUP, signalHandler);
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	logNotice(lf_main, PACKAGE_STRING "." REVISION " started");

	// load configuration files
	loadConfigFiles(s_messageMap);
	if (s_messageMap->sizeConditions()>0 && opt.pollInterval==0)
		logError(lf_main, "conditions require a poll interval > 0");

	// create the MainLoop and run it
	s_mainLoop = new MainLoop(opt, device, s_messageMap);
	s_mainLoop->start("mainloop");
	s_mainLoop->join();

	// shutdown
	shutdown();
}

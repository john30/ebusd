/*
 * Copyright (C) John Baier 2014-2015 <ebusd@johnm.de>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
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
	CONFIG_PATH, // configPath
	0, // checkConfig
	5, // pollInterval
	0xFF, // address
	false, // answer
	9400, // acquireTimeout
	2, // acquireRetries
	2, // sendRetries
	SLAVE_RECV_TIMEOUT, // receiveTimeout
	0, // masterCount
	false, // generateSyn
	false, // foreground
	8888, // port
	false, // localOnly
	PACKAGE_LOGFILE, // logFile
	false, // logRaw
	false, // dump
	"/tmp/ebus_dump.bin", // dumpFile
	100 // dumpSize
};

/** the @a MainLoop instance, or NULL. */
static MainLoop* mainLoop = NULL;

/** the version string of the program. */
const char *argp_program_version = ""PACKAGE_STRING"";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = ""PACKAGE_BUGREPORT"";

/** the documentation of the program. */
static const char argpdoc[] =
	"A daemon for access to eBUS devices.";

#define O_CHKCFG  1
#define O_DMPCFG  2
#define O_POLINT  3
#define O_ANSWER  4
#define O_ACQTIM  5
#define O_ACQRET  6
#define O_SNDRET  7
#define O_RCVTIM  8
#define O_MASCNT  9
#define O_GENSYN 10
#define O_LOCAL  11
#define O_LOGARE 12
#define O_LOGLEV 13
#define O_LOGRAW 14
#define O_DMPFIL 15
#define O_DMPSIZ 16

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
	{NULL,             0,        NULL,    0, "Device options:", 1 },
	{"device",         'd',      "DEV",   0, "Use DEV as eBUS device (serial device or ip:port) [/dev/ttyUSB0]", 0 },
	{"nodevicecheck",  'n',      NULL,    0, "Skip serial eBUS device test", 0 },

	{NULL,             0,        NULL,    0, "Message configuration options:", 2 },
	{"configpath",     'c',      "PATH",  0, "Read CSV config files from PATH [" CONFIG_PATH "]", 0 },
	{"checkconfig",    O_CHKCFG, NULL,    0, "Check CSV config files, then stop", 0 },
	{"dumpconfig",     O_DMPCFG, NULL,    0, "Check and dump CSV config files, then stop", 0 },
	{"pollinterval",   O_POLINT, "SEC",   0, "Poll for data every SEC seconds (0=disable) [5]", 0 },

	{NULL,             0,        NULL,    0, "eBUS options:", 3 },
	{"address",        'a',      "ADDR",  0, "Use ADDR as own bus address [FF]", 0 },
	{"answer",         O_ANSWER, NULL,    0, "Actively answer to requests from other masters", 0 },
	{"acquiretimeout", O_ACQTIM, "USEC",  0, "Stop bus acquisition after USEC us [9400]", 0 },
	{"acquireretries", O_ACQRET, "COUNT", 0, "Retry bus acquisition COUNT times [2]", 0 },
	{"sendretries",    O_SNDRET, "COUNT", 0, "Repeat failed sends COUNT times [2]", 0 },
	{"receivetimeout", O_RCVTIM, "USEC",  0, "Expect a slave to answer within USEC us [15000]", 0 },
	{"numbermasters",  O_MASCNT, "COUNT", 0, "Expect COUNT masters on the bus, 0 for auto detection [0]", 0 },
	{"generatesyn",    O_GENSYN, NULL,    0, "Enable AUTO-SYN symbol generation", 0 },

	{NULL,             0,        NULL,    0, "Daemon options:", 4 },
	{"foreground",     'f',      NULL,    0, "Run in foreground", 0 },
	{"port",           'p',      "PORT",  0, "Listen for client connections on PORT [8888]", 0 },
	{"localhost",      O_LOCAL,  NULL,    0, "Listen on 127.0.0.1 interface only", 0 },

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

	// Message configuration options:
	case 'c': // --configpath=/etc/ebusd
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid configpath");
			return EINVAL;
		}
		opt->configPath = arg;
		break;
	case O_CHKCFG: // --checkconfig
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
	if (mainLoop != NULL) {
		delete mainLoop;
		mainLoop = NULL;
	}

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
 * Read the configuration files from the specified path.
 * @param path the path from which to read the files.
 * @param extension the filename extension of the files to read.
 * @param logFunc the function to call for logging, or @a NULL to be silent.
 * @param templates the available @a DataFieldTemplates.
 * @param messages the @a MessageMap to load the messages into.
 * @param verbose whether to verbosely log problems.
 * @return the result code.
 */
static result_t readConfigFiles(const string path, const string extension, DataFieldTemplates* templates, MessageMap* messages, bool verbose)
{
	DIR* dir = opendir(path.c_str());

	if (dir == NULL)
		return RESULT_ERR_NOTFOUND;

	dirent* d;

	while ((d = readdir(dir)) != NULL) {
		string fn = d->d_name;

		if (fn == "." || fn == "..")
			continue;

		const string p = path + "/" + d->d_name;
		struct stat stat_buf;

		if (stat(p.c_str(), &stat_buf) != 0)
			continue;

		if (S_ISDIR(stat_buf.st_mode)) {
			result_t result = readConfigFiles(p, extension, templates, messages, verbose);
			if (result != RESULT_OK)
				return result;
		}
		else if (S_ISREG(stat_buf.st_mode)) {
			if (fn.find(extension, (fn.length() - extension.length())) != string::npos
				&& fn != "_templates" + extension) {
				result_t result = messages->readFromFile(p, templates, verbose);
				if (result != RESULT_OK)
					return result;
			}
		}
	}
	closedir(dir);

	return RESULT_OK;
};

result_t loadConfigFiles(DataFieldTemplates* templates, MessageMap* messages, bool verbose) {
	logInfo(lf_main, "path to ebus configuration files: %s", opt.configPath);
	string path = string(opt.configPath);
	messages->clear();
	templates->clear();
	result_t result = templates->readFromFile(path+"/_templates.csv", NULL, verbose);
	if (result == RESULT_OK)
		logInfo(lf_main, "read templates");
	else
		logError(lf_main, "error reading templates: %s", getResultCode(result));

	result = readConfigFiles(path, ".csv", templates, messages, verbose);
	if (result == RESULT_OK)
		logInfo(lf_main, "read config files");
	else
		logError(lf_main, "error reading config files: %s", getResultCode(result));

	logNotice(lf_main, "found messages: %d (%d poll, %d update)", messages->size(), messages->sizePoll(), messages->size(true));

	return result;
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
	setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &opt) != 0)
		return EINVAL;

	DataFieldTemplates templates;
	MessageMap messages;
	if (opt.checkConfig) {
		logNotice(lf_main, "Performing configuration check...");

		result_t result = loadConfigFiles(&templates, &messages, true);

		if (result == RESULT_OK && opt.checkConfig > 1) {
			logNotice(lf_main, "Configuration dump:");
			messages.dump(cout);
		}
		messages.clear();
		templates.clear();

		return 0;
	}

	// open the device
	Device *device = Device::create(opt.device, !opt.noDeviceCheck, &logRawData);
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

	logNotice(lf_main, "ebusd started");

	// load configuration files
	loadConfigFiles(&templates, &messages);

	// create the MainLoop and run it
	mainLoop = new MainLoop(opt, device, &templates, &messages);
	mainLoop->run();

	// shutdown
	shutdown();
}

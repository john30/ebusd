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

/** the name of the PID file. */
#ifdef PACKAGE_PIDFILE
#define PID_FILE_NAME PACKAGE_PIDFILE
#else
#define PID_FILE_NAME "/var/run/ebusd.pid"
#endif

/** the opened PID file, or NULL. */
static FILE* pidFile = NULL;

/** true when forked into daemon mode. */
static bool isDaemon = false;

/** the program options. */
static struct options opt = {
	"/dev/ttyUSB0", // device
	false, // noDeviceCheck
	"/etc/ebusd", // configPath
	false, // checkConfig
	5, // pollInterval
	0xFF, // address
	false, // answer
	9400, // acquireTimeout
	2, // acquireRetries
	2, // sendRetries
	15000, // receiveTimeout
	5, // numberMasters
	false, // foreground
	8888, // port
	false, // localhost
	"/var/log/ebusd.log", // logFile
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
	PACKAGE " - a daemon for access to eBUS devices.";

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
	{NULL,               0, NULL,    0, "Device settings:", 1 },
	{"device",         'd', "DEV",   0, "Use DEV as eBUS device (serial device or ip:port) [/dev/ttyUSB0]", 0 },
	{"nodevicecheck",  'n', NULL,    0, "Skip serial eBUS device test", 0 },

	{NULL,               0, NULL,    0, "Message configuration settings:", 2 },
	{"configpath",     'c', "PATH",  0, "Read CSV config files from PATH [/etc/ebusd]", 0 },
	{"checkconfig",      1, NULL,    0, "Only check CSV config files, then stop", 0 },
	{"pollinterval",     2, "SEC",   0, "Poll for data every SEC seconds (0=disable) [5]", 0 },

	{NULL,               0, NULL,    0, "E-Bus settings:", 3 },
	{"address",        'a', "ADDR",  0, "Use ADDR as own bus address [FF]", 0 },
	{"answer",           3, NULL,    0, "Actively answer to requests from other masters", 0 },
	{"acquiretimeout",   4, "USEC",  0, "Stop bus acquisition after USEC us [9400]", 0 },
	{"acquireretries",   5, "COUNT", 0, "Retry bus acquisition COUNT times [2]", 0 },
	{"sendretries",      6, "COUNT", 0, "Repeat failed sends COUNT times [2]", 0 },
	{"receivetimeout",   7, "USEC",  0, "Expect a slave to answer within USEC us [15000]", 0 },
	{"numbermasters",    8, "COUNT", 0, "Expect COUNT masters on the bus [5]", 0 },

	{NULL,               0, NULL,    0, "Daemon settings:", 4 },
	{"foreground",     'f', NULL,    0, "Run in foreground", 0 },
	{"port",           'p', "PORT",  0, "Listen for client connections on PORT [8888]", 0 },
	{"localhost",        9, NULL,    0, "Listen on 127.0.0.1 interface only", 0 },

	{NULL,               0, NULL,    0, "Log settings:", 5 },
	{"logfile",        'l', "FILE",  0, "Write log to FILE (only for daemon) [/var/log/ebusd.log]", 0 },
	{"logareas",        10, "AREAS", 0, "Only write log for matching AREAS: main,network,bus,update,all [all]", 0 },
	{"loglevel",        11, "LEVEL", 0, "Only write log below or equal to LEVEL: error/notice/info/debug [notice]", 0 },
	{"lograwdata",      12, NULL,    0, "Log each received/sent byte on the bus", 0 },

	{NULL,               0, NULL,    0, "Dump settings:", 6 },
	{"dump",           'D', NULL,    0, "Enable dump of received bytes", 0 },
	{"dumpfile",        13, "FILE",  0, "Dump received bytes to FILE [/tmp/ebus_dump.bin]", 0 },
	{"dumpsize",        14, "SIZE",  0, "Make dump files no larger than SIZE kB [100]", 0 },

	{NULL,               0, NULL,    0, NULL, 0 },
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

	// Device settings:
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

	// Message configuration settings:
	case 'c': // --configpath=/etc/ebusd
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid configpath");
			return EINVAL;
		}
		opt->configPath = arg;
		break;
	case 1: // --checkconfig
		opt->checkConfig = true;
		break;
	case 2: // --pollinterval=5
		opt->pollInterval = parseInt(arg, 10, 0, 3600, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid pollinterval");
			return EINVAL;
		}
		break;

	// E-Bus settings:
	case 'a': // --address=FF
		opt->address = parseInt(arg, 16, 0, 0xff, result);
		if (result != RESULT_OK || !isMaster(opt->address)) {
			argp_error(state, "invalid address");
			return EINVAL;
		}
		break;
	case 3: // --answer
		opt->answer = true;
		break;
	case 4: // --acquiretimeout=9400
		opt->acquireTimeout = parseInt(arg, 10, 1000, 100000, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid acquiretimeout");
			return EINVAL;
		}
		break;
	case 5: // --acquireretries=2
		opt->acquireRetries = parseInt(arg, 10, 0, 10, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid acquireretries");
			return EINVAL;
		}
		break;
	case 6: // --sendretries=2
		opt->sendRetries = parseInt(arg, 10, 0, 10, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid sendretries");
			return EINVAL;
		}
		break;
	case 7: // --receivetimeout=15000
		opt->receiveTimeout = parseInt(arg, 10, 1000, 100000, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid receivetimeout");
			return EINVAL;
		}
		break;
	case 8: // --numbermasters=5
		opt->numberMasters = parseInt(arg, 10, 1, 10, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid numbermasters");
			return EINVAL;
		}
		break;

	// Daemon settings:
	case 'f': // --foreground
		opt->foreground = true;
		break;
	case 'p': // --port=8888
		opt->port = parseInt(arg, 10, 1, 65535, result);
		if (result != RESULT_OK) {
			argp_error(state, "invalid port");
			return EINVAL;
		}
		break;
	case 9: // --localhost
		opt->localhost = true;
		break;

	// Log settings:
	case 'l': // --logfile=/var/log/ebusd.log
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid logfile");
			return EINVAL;
		}
		opt->logFile = arg;
		break;
	case 10: // --logareas=all
		if (!setLogFacilities(arg)) {
			argp_error(state, "invalid logareas");
			return EINVAL;
		}
		break;
	case 11: // --loglevel=event
		if (!setLogLevel(arg)) {
			argp_error(state, "invalid loglevel");
			return EINVAL;
		}
		break;
	case 12:  // --lograwdata
		opt->logRaw = true;
		break;

	// Dump settings:
	case 'D':  // --dump
		opt->dump = true;
		break;
	case 13: // --dumpfile=/tmp/ebus_dump.bin
		if (arg == NULL || arg[0] == 0 || strcmp("/", arg) == 0) {
			argp_error(state, "invalid dumpfile");
			return EINVAL;
		}
		opt->dumpFile = arg;
		break;
	case 14: // --dumpsize=100
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
	if (chdir("/tmp") < 0) { // TODO use constant
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

	dirent* d = readdir(dir);

	while (d != NULL) {
		if (d->d_type == DT_DIR) {
			string fn = d->d_name;

			if (fn != "." && fn != "..") {
				const string p = path + "/" + d->d_name;
				result_t result = readConfigFiles(p, extension, templates, messages, verbose);
				if (result != RESULT_OK)
					return result;
			}
		}
		else if (d->d_type == DT_REG || d->d_type == DT_LNK) {
			string fn = d->d_name;

			if (fn.find(extension, (fn.length() - extension.length())) != string::npos
				&& fn != "_templates" + extension) {
				const string p = path + "/" + d->d_name;
				result_t result = messages->readFromFile(p, templates, verbose);
				if (result != RESULT_OK)
					return result;
			}
		}

		d = readdir(dir);
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

	logNotice(lf_main, "message DB: %d ", messages->size());
	logNotice(lf_main, "updates DB: %d ", messages->size(true));
	logNotice(lf_main, "polling DB: %d ", messages->sizePoll());

	return result;
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
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &opt) != 0)
		return EINVAL;


	DataFieldTemplates templates;
	MessageMap messages;
	if (opt.checkConfig == true) {
		logNotice(lf_main, "Performing configuration check...");

		loadConfigFiles(&templates, &messages, true);

		messages.clear();
		templates.clear();

		return 0;
	}

	if (opt.foreground == false) {
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
	mainLoop = new MainLoop(opt, &templates, &messages);
	mainLoop->run();

	// shutdown
	shutdown();
}

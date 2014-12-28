/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "logger.h"
#include "daemon.h"
#include "appl.h"
#include "baseloop.h"
#include <csignal>
#include <iostream>
#include <dirent.h>


using namespace std;

Appl& A = Appl::Instance();
Daemon& D = Daemon::Instance();
Logger& L = Logger::Instance();

BaseLoop* baseloop = NULL;

void define_args()
{

	A.setVersion(""PACKAGE_STRING"");

	A.addText("Options:\n");

	A.addOption("address", "a", OptVal(0xff), dt_hex, ot_mandatory,
		    "\tebus device address [FF]");

	A.addOption("answer", "", OptVal(false), dt_bool, ot_none,
		    "\tanswers to requests from other devices");

	A.addOption("foreground", "f", OptVal(false), dt_bool, ot_none,
		    "run in foreground\n");

	A.addOption("device", "d", OptVal("/dev/ttyUSB0"), dt_string, ot_mandatory,
		    "\tebus device (serial or network) [/dev/ttyUSB0]");

	A.addOption("nodevicecheck", "n", OptVal(false), dt_bool, ot_none,
		    "disable valid ebus device test\n");

	A.addOption("acquiretimeout", "", OptVal(9400), dt_long, ot_mandatory,
		    "bus acquisition timeout in 'us' [9400]");

	A.addOption("acquireretries", "", OptVal(2), dt_int, ot_mandatory,
		    "number retries to acquire ebus [2]");

	A.addOption("sendretries", "", OptVal(2), dt_int, ot_mandatory,
		    "number retries send ebus command [2]");

	A.addOption("receivetimeout", "", OptVal(15000), dt_long, ot_mandatory,
		    "receive timeout in 'us' [15000]");

	A.addOption("numbermasters", "", OptVal(5), dt_int, ot_mandatory,
		    "max number of master bus participant [5]");

	A.addOption("pollinterval", "", OptVal(5), dt_int, ot_mandatory,
		    "polling interval in 's' [5]\n");

	A.addOption("configpath", "c", OptVal("/etc/ebusd"), dt_string, ot_mandatory,
		    "path to ebus configuration files [/etc/ebusd]");

	A.addOption("checkconfig", "", OptVal(false), dt_bool, ot_none,
		    "check of configuration files\n");

	A.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory,
		    "\tlisten port [8888]");

	A.addOption("localhost", "", OptVal(false), dt_bool, ot_none,
		    "listen localhost only\n");

	A.addOption("logfile", "l", OptVal("/var/log/ebusd.log"), dt_string, ot_mandatory,
		    "\tlog file name [/var/log/ebusd.log]");

	A.addOption("logareas", "", OptVal("all"), dt_string, ot_mandatory,
		    "\tlog areas - bas|net|bus|upd|all [all]");

	A.addOption("loglevel", "", OptVal("trace"), dt_string, ot_mandatory,
		    "\tlog level - error|event|trace|debug [event]");

	A.addOption("lograwdata", "", OptVal(false), dt_bool, ot_none,
		    "log raw data (bytes)\n");

	A.addOption("dump", "D", OptVal(false), dt_bool, ot_none,
		    "\tenable dump");

	A.addOption("dumpfile", "", OptVal("/tmp/ebus_dump.bin"), dt_string, ot_mandatory,
		    "\tdump file name [/tmp/ebus_dump.bin]");

	A.addOption("dumpsize", "", OptVal(100), dt_long, ot_mandatory,
		    "\tmax size for dump file in 'kB' [100]\n");
}

void shutdown()
{
	// stop threads
	if (baseloop != NULL) {
		delete baseloop;
		baseloop = NULL;
	}

	// reset all signal handlers to default
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	// delete daemon pid file
	if (D.status() == true)
		D.stop();

	// stop logger
	L.log(bas, event, "ebusd stopped");
	L.stop();
	L.join();

	exit(EXIT_SUCCESS);
}

void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		L.log(bas, event, "SIGHUP received");
		break;
	case SIGINT:
		L.log(bas, event, "SIGINT received");
		shutdown();
		break;
	case SIGTERM:
		L.log(bas, event, "SIGTERM received");
		shutdown();
		break;
	default:
		L.log(bas, event, "undefined signal %s", strsignal(sig));
		break;
	}
}

/**
 * @brief Read the configuration files from the specified path.
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
		} else if (d->d_type == DT_REG || d->d_type == DT_LNK) {
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

/**
 * @brief Load the message definitions from the configuration files.
 * @param templates the @a DataFieldTemplates to load the templates into.
 * @param messages the @a MessageMap to load the messages into.
 * @param verbose whether to verbosely log problems.
 * @return the result code.
 */
result_t loadConfigFiles(DataFieldTemplates* templates, MessageMap* messages, bool verbose=false) {
	string path = A.getOptVal<const char*>("configpath");
	L.log(bas, trace, "path to ebus configuration files: %s", path.c_str());
	messages->clear();
	templates->clear();
	result_t result = templates->readFromFile(path+"/_templates.csv", NULL, verbose);
	if (result == RESULT_OK)
		L.log(bas, trace, "read templates");
	else
		L.log(bas, error, "error reading templates: %s", getResultCode(result));

	result = readConfigFiles(path, ".csv", templates, messages, verbose);
	if (result == RESULT_OK)
		L.log(bas, trace, "read config files");
	else
		L.log(bas, error, "error reading config files: %s", getResultCode(result));

		L.log(bas, event, "message DB: %d ", messages->size());
		L.log(bas, event, "updates DB: %d ", messages->size(true));
		L.log(bas, event, "polling DB: %d ", messages->sizePoll());

	return result;
}


int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	A.parseArgs(argc, argv);

	if (A.getOptVal<bool>("checkconfig") == true) {
		L += new LogConsole(calcAreaMask(A.getOptVal<const char*>("logareas")),
					calcLevel(A.getOptVal<const char*>("loglevel")),
					"logconsole");

		L.log(bas, event, "ebusd started");

		DataFieldTemplates templates;
		MessageMap messages;

		loadConfigFiles(&templates, &messages, true);

		messages.clear();
		templates.clear();

		shutdown();

		return 0;
	}

	if (A.getOptVal<bool>("foreground") == true) {
		L += new LogConsole(calcAreaMask(A.getOptVal<const char*>("logareas")),
				    calcLevel(A.getOptVal<const char*>("loglevel")),
				    "logconsole");
	} else {
		// make me daemon
		D.run("/var/run/ebusd.pid");
		L += new LogFile(calcAreaMask(A.getOptVal<const char*>("logareas")),
				 calcLevel(A.getOptVal<const char*>("loglevel")),
				 "logfile", A.getOptVal<const char*>("logfile"));
	}

	// trap signals that we expect to receive
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// start logger
	L.start("logger");
	// wait for logger be ready
	usleep(100000);
	L.log(bas, event, "ebusd started");

	// create baseloop
	baseloop = new BaseLoop();
	baseloop->start();

	// shutdown
	shutdown();
}


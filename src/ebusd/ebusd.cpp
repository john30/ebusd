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

using namespace libebus;

Appl& A = Appl::Instance();
Daemon& D = Daemon::Instance();
LogInstance& L = LogInstance::Instance();

BaseLoop* baseloop;

void define_args()
{

	A.setVersion(""PACKAGE_STRING"");

	A.addOption("address", "a", OptVal("FF"),  dt_string, ot_mandatory,
		    "\tebus device address (FF)");

	A.addOption("device", "d", OptVal("/dev/ttyUSB0"), dt_string, ot_mandatory,
		    "\tebus device (serial or network) (/dev/ttyUSB0)");

	A.addOption("nodevicecheck", "n", OptVal(false), dt_bool, ot_none,
		    "disable valid ebus device test\n");

	A.addOption("sendretries", "s", OptVal(2), dt_int, ot_mandatory,
		    "number retries send ebus command (2)");

	A.addOption("lockretries", "", OptVal(2), dt_int, ot_mandatory,
		    "number retries to lock ebus (2)");

	A.addOption("lockcounter", "", OptVal(5), dt_int, ot_mandatory,
		    "number of SYN to unlock send function (5)");

	A.addOption("recvtimeout", "", OptVal(15000), dt_long, ot_mandatory,
		    "receive timeout in 'us' (15000)");

	A.addOption("acquiretime", "", OptVal(4200), dt_long, ot_mandatory,
		    "waiting time for bus acquire in 'us' (4200)\n");

	A.addOption("pollinterval", "", OptVal(5), dt_int, ot_mandatory,
		    "polling interval in 's' (5)\n");

	A.addOption("ebusconfdir", "e", OptVal("/etc/ebusd"), dt_string, ot_mandatory,
		    "directory for ebus configuration (/etc/ebusd)\n");

	A.addOption("foreground", "f", OptVal(false), dt_bool, ot_none,
		    "run in foreground\n");

	A.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory,
		    "\tlisten port (8888)");

	A.addOption("localhost", "", OptVal(false), dt_bool, ot_none,
		    "listen localhost only\n");

	A.addOption("logfile", "l", OptVal("/var/log/ebusd.log"), dt_string, ot_mandatory,
		    "\tlog file name (/var/log/ebusd.log)");

	A.addOption("logareas", "", OptVal("all"), dt_string, ot_mandatory,
		    "\tlog areas - bas|net|bus|cyc|all (all)");

	A.addOption("loglevel", "", OptVal("trace"), dt_string, ot_mandatory,
		    "\tlog level - error|event|trace|debug (event)");

	A.addOption("lograwdata", "", OptVal(false), dt_bool, ot_none,
		    "log raw data (bytes)\n");

	A.addOption("dump", "D", OptVal(false), dt_bool, ot_none,
		    "\tenable dump");

	A.addOption("dumpfile", "", OptVal("/tmp/ebus_dump.bin"), dt_string, ot_mandatory,
		    "\tdump file name (/tmp/ebus_dump.bin)");

	A.addOption("dumpsize", "", OptVal(100), dt_long, ot_mandatory,
		    "\tmax size for dump file in 'kB' (100)\n");
}

void shutdown()
{
	// stop threads
	delete baseloop;

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

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	A.parseArgs(argc, argv);

	// make me daemon
	if (A.getOptVal<bool>("foreground") == true) {
		L += new LogConsole(calcAreas(A.getOptVal<const char*>("logareas")),
				    calcLevel(A.getOptVal<const char*>("loglevel")),
				    "logconsole");
	} else {
		D.run("/var/run/ebusd.pid");
		L += new LogFile(calcAreas(A.getOptVal<const char*>("logareas")),
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


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
	A.addArgs("", 0);

	A.addItem("p_address", Appl::Param("FF"), "a", "address",
		  "\tebus device address (FF)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_device", Appl::Param("/dev/ttyUSB0"), "d", "device",
		  "\tebus device (serial or network) (/dev/ttyUSB0)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_nodevicecheck", Appl::Param(false), "n", "nodevicecheck",
		  "disable valid ebus device test\n",
		  Appl::type_bool, Appl::opt_none);

	A.addItem("p_sendretries", Appl::Param(2), "s", "sendretries",
		  "number retries send ebus command (2)",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_lockretries", Appl::Param(2), "", "lockretries",
		  "number retries to lock ebus (2)",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_lockcounter", Appl::Param(5), "", "lockcounter",
		  "number of SYN to unlock send function (5)",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_recvtimeout", Appl::Param(15000), "", "recvtimeout",
		  "receive timeout in 'us' (15000)",
		  Appl::type_long, Appl::opt_mandatory);

	A.addItem("p_acquiretime", Appl::Param(4200), "", "acquiretime",
		  "waiting time for bus acquire in 'us' (4200)\n",
		  Appl::type_long, Appl::opt_mandatory);

	A.addItem("p_pollinterval", Appl::Param(5), "", "pollinterval",
		  "polling interval in 's' (5)\n",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_ebusconfdir", Appl::Param("/etc/ebusd"), "e", "ebusconfdir",
		  "directory for ebus configuration (/etc/ebusd)\n",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_foreground", Appl::Param(false), "f", "foreground",
		  "run in foreground\n",
		  Appl::type_bool, Appl::opt_none);

	A.addItem("p_port", Appl::Param(8888), "p", "port",
		  "\tlisten port (8888)",
		  Appl::type_int, Appl::opt_mandatory);

	A.addItem("p_localhost", Appl::Param(false), "", "localhost",
		  "listen localhost only\n",
		  Appl::type_bool, Appl::opt_none);

	A.addItem("p_logfile", Appl::Param("/var/log/ebusd.log"), "l", "logfile",
		  "\tlog file name (/var/log/ebusd.log)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_logareas", Appl::Param("all"), "", "logareas",
		  "\tlog areas - bas|net|bus|cyc|all (all)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_loglevel", Appl::Param("trace"), "", "loglevel",
		  "\tlog level - error|event|trace|debug (event)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_lograwdata", Appl::Param(false), "", "lograwdata",
		  "log raw data (bytes)\n",
		  Appl::type_bool, Appl::opt_none);

	A.addItem("p_dump", Appl::Param(false), "D", "dump",
		  "\tenable dump",
		  Appl::type_bool, Appl::opt_none);

	A.addItem("p_dumpfile", Appl::Param("/tmp/ebus_dump.bin"), "", "dumpfile",
		  "\tdump file name (/tmp/ebus_dump.bin)",
		  Appl::type_string, Appl::opt_mandatory);

	A.addItem("p_dumpsize", Appl::Param(100), "", "dumpsize",
		  "\tmax size for dump file in 'kB' (100)\n",
		  Appl::type_long, Appl::opt_mandatory);

	A.addItem("p_settings", Appl::Param(false), "", "settings",
		  "\tprint daemon settings\n",
		  Appl::type_bool, Appl::opt_none);

	A.addVersion(""PACKAGE_STRING"");
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

	// print daemon settings
	if (A.getParam<bool>("p_settings") == true)
		A.printSettings();

	// make me daemon
	if (A.getParam<bool>("p_foreground") == true) {
		L += new LogConsole(calcAreas(A.getParam<const char*>("p_logareas")),
				    calcLevel(A.getParam<const char*>("p_loglevel")),
				    "logconsole");
	} else {
		D.run("/var/run/ebusd.pid");
		L += new LogFile(calcAreas(A.getParam<const char*>("p_logareas")),
				 calcLevel(A.getParam<const char*>("p_loglevel")),
				 "logfile", A.getParam<const char*>("p_logfile"));
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


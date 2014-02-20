/*
 * Copyright (C) Roland Jax 2012-2013 <roland.jax@liwest.at>
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
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <syslog.h>
#include <termios.h>
#include <errno.h>

#include "log.h"
#include "utils.h"
#include "ebus-decode.h"
#include "ebus-cmd.h"
#include "ebus-bus.h"
#include "ebusd.h"


/* global variables */
const char *progname;

static int pidfile_locked = NO;
static int msg_queue_on = NO;

static int pidfd = UNSET; /* pidfile file descriptor */
static int busfd = UNSET; /* bus file descriptor */
static int socketfd = UNSET; /* socket file descriptor */


static char address[3];
static char cfgdir[CFG_LINELEN];
static char cfgfile[CFG_LINELEN];
static char device[CFG_LINELEN];
static char extension[10];
static int foreground = UNSET;
static char loglevel[CFG_LINELEN];
static char logfile[CFG_LINELEN];
static int nodevicecheck = UNSET;
static char pidfile[CFG_LINELEN];
static int port = UNSET;
static int rawdump = UNSET;
static char rawfile[CFG_LINELEN];
static int showraw = UNSET;
static int settings = UNSET;
static int localhost = UNSET;
static int get_retry = UNSET;
static int skip_ack = UNSET;
static int max_wait = UNSET;
static int send_retry = UNSET;
static int print_size = UNSET;



static char options[] = "a:c:C:d:e:fl:L:nP:p:rR:sStvh";

static struct option opts[] = {
	{"address",       required_argument, NULL, 'a'},
	{"cfgfdir",       required_argument, NULL, 'c'},
	{"cfgfile",       required_argument, NULL, 'C'},
	{"device",        required_argument, NULL, 'd'},
	{"extension",     required_argument, NULL, 'e'},
	{"foreground",    no_argument,       NULL, 'f'},
	{"loglevel",      required_argument, NULL, 'l'},
	{"logfile",       required_argument, NULL, 'L'},
	{"nodevicecheck", no_argument,       NULL, 'n'},
	{"pidfile",       required_argument, NULL, 'P'},
	{"port",          required_argument, NULL, 'p'},
	{"rawdump",       no_argument,       NULL, 'r'},
	{"rawfile",       required_argument, NULL, 'R'},
	{"showraw",       no_argument,       NULL, 's'},	
	{"settings",      no_argument,       NULL, 'S'},
	{"localhost",     no_argument,       NULL, 't'},
	{"version",       no_argument,       NULL, 'v'},
	{"help",          no_argument,       NULL, 'h'},
	{NULL,            no_argument,       NULL,  0 },
};

static struct config cfg[] = {

{"address",       STR, &address, "\tbus address (" NUMSTR(EBUS_QQ) ")"},
{"cfgdir",        STR, &cfgdir, "\tconfiguration directory of command files (" DAEMON_CFGDIR ")"},
{"cfgfile",       STR, &cfgfile, "\tdaemon configuration file (" DAEMON_CFGFILE ")"},
{"device",        STR, &device, "\tbus device (" SERIAL_DEVICE ")"},
{"extension",     STR, &extension, "extension of command files (" DAEMON_EXTENSION ")"},
{"foreground",    BOL, &foreground, "run in foreground"},
{"loglevel",      STR, &loglevel, "\tlog level (INF | " LOGTXT ")"},
{"logfile",       STR, &logfile, "\tlog file (" DAEMON_LOGFILE ")"},
{"nodevicecheck", BOL, &nodevicecheck, "don't check bus device"},
{"pidfile",       STR, &pidfile, "\tpid file (" DAEMON_PIDFILE ")"},
{"port",          NUM, &port, "\tport (" NUMSTR(SOCKET_PORT) ")"},
{"rawdump",       BOL, &rawdump, "\tdump raw ebus data to file"},
{"rawfile",       STR, &rawfile, "\traw file (" DAEMON_RAWFILE ")"},
{"showraw",       BOL, &showraw, "\tprint raw data"},
{"settings",      BOL, &settings, "\tprint daemon settings"},
{"localhost",     BOL, &localhost, "allow only connection from localhost"},
{"get_retry",     NUM, &get_retry, NULL},
{"skip_ack",      NUM, &skip_ack, NULL},
{"max_wait",      NUM, &max_wait, NULL},
{"send_retry",    NUM, &send_retry, NULL},
{"print_size",    NUM, &print_size, NULL},
{"version",       STR, NULL, "\tprint version information"},
{"help",          STR, NULL, "\tprint this message"}
};

const int cfglen = sizeof(cfg) / sizeof(cfg[0]);

void
usage(void)
{
	fprintf(stdout, "\nUsage: %s [OPTIONS]\n", progname);

	int i, skip;

	skip = 0;

	for (i = 0; i < cfglen; i++) {
		if (cfg[i].info != NULL) {
			fprintf(stdout, "  -%c --%s\t%s\n",
				opts[i - skip].val,
				opts[i - skip].name,
				cfg[i].info);
		} else {
			skip++;
		}
	}

	fprintf(stdout, "\n");
}

void
cmdline(int *argc, char ***argv)
{
	for (;;) {
		int i;

		i = getopt_long(*argc, *argv, options, opts, NULL);

		if (i == -1)
			break;

		switch (i) {
		case 'a':
			if (strlen(optarg) > 2)
				strncpy(address, &optarg[strlen(optarg) - 2 ], 2);				
			else
				strncpy(address, optarg, strlen(optarg));

			break;			
		case 'c':
			strncpy(cfgdir, optarg, strlen(optarg));
			break;
		case 'C':
			strncpy(cfgfile, optarg, strlen(optarg));
			break;
		case 'd':
			strncpy(device, optarg, strlen(optarg));
			break;
		case 'e':
			strncpy(extension, optarg, strlen(optarg));
			break;						
		case 'f':
			foreground = YES;
			break;
		case 'l':
			strncpy(loglevel, optarg, strlen(optarg));
			break;
		case 'L':
			strncpy(logfile, optarg, strlen(optarg));
			break;
		case 'n':
			nodevicecheck = YES;
			break;
		case 'P':
			strncpy(pidfile, optarg, strlen(optarg));
			break;
		case 'p':
			if (isdigit(*optarg))
				port = atoi(optarg);
			break;
		case 'r':
			rawdump = YES;
			break;
		case 'R':
			strncpy(rawfile, optarg, strlen(optarg));
			rawdump = YES;
			break;
		case 's':
			showraw = YES;
			break;			
		case 'S':
			settings = YES;
			break;
		case 't':
			localhost = YES;
			break;	
		case 'v':
			fprintf(stdout, DAEMON_NAME " " DAEMON_VERSION "\n");
			exit(EXIT_SUCCESS);
		case 'h':
		default:
			usage();
			exit(EXIT_FAILURE);
			break;
		}
	}
}

void
set_unset(void)
{

	if (*address == '\0')
		strncpy(address , &NUMSTR(EBUS_QQ)[2], 2);

	if (*cfgdir == '\0')
		strncpy(cfgdir , DAEMON_CFGDIR, strlen(DAEMON_CFGDIR));

	if (*device == '\0')
		strncpy(device , SERIAL_DEVICE, strlen(SERIAL_DEVICE));

	if (*extension == '\0') {
		strncpy(extension , DAEMON_EXTENSION, strlen(DAEMON_EXTENSION));
	}

	if (foreground == UNSET)
		foreground = NO;		

	if (*loglevel == '\0')
		strncpy(loglevel , DAEMON_LOGLEVEL, strlen(DAEMON_LOGLEVEL));

	if (*logfile == '\0')
		strncpy(logfile , DAEMON_LOGFILE, strlen(DAEMON_LOGFILE));

	if (nodevicecheck == UNSET)
		nodevicecheck = NO;

	if (*pidfile == '\0')
		strncpy(pidfile , DAEMON_PIDFILE, strlen(DAEMON_PIDFILE));

	if (port == UNSET)
		port = SOCKET_PORT;

	if (rawdump == UNSET)
		rawdump = NO;

	if (*rawfile == '\0')
		strncpy(rawfile , DAEMON_RAWFILE, strlen(DAEMON_RAWFILE));
		
	if (showraw == UNSET)
		showraw = NO;		

	if (settings == UNSET)
		settings = NO;

	if (localhost == UNSET)
		localhost = NO;		

	if (get_retry == UNSET)
		get_retry = EBUS_GET_RETRY;
	/* set max */
	if (get_retry > EBUS_GET_RETRY_MAX)
		get_retry = EBUS_GET_RETRY_MAX;

	if (skip_ack == UNSET)
		skip_ack = EBUS_SKIP_ACK;

	if (max_wait == UNSET)
		max_wait = EBUS_MAX_WAIT;

	if (send_retry == UNSET)
		send_retry = EBUS_SEND_RETRY;
	/* set max */
	if (send_retry > EBUS_SEND_RETRY_MAX)
		send_retry = EBUS_SEND_RETRY_MAX;

	if (print_size == UNSET)
		print_size = EBUS_PRINT_SIZE;		
		
}

void
signal_handler(int sig) {
	switch(sig) {
	case SIGHUP:
		log_print(L_ALL, "SIGHUP received");
		syslog(LOG_INFO, "SIGHUP received");
		break;
	case SIGINT:
		log_print(L_ALL, "SIGINT received - logfile reopen");
		syslog(LOG_INFO, "SIGINT received - logfile reopen");		
		log_open(logfile, foreground);
		break;
	case SIGTERM:
		log_print(L_ALL, "daemon exiting");
		syslog(LOG_INFO, "daemon exiting");		
		cleanup(EXIT_SUCCESS);
		break;
	default:
		log_print(L_ALL, "unknown signal %s", strsignal(sig));
		syslog(LOG_INFO, "unknown signal %s", strsignal(sig));		
		break;
	}
}

void
daemonize(void)
{
	pid_t pid;

	/* fork off the parent process */
	pid = fork();
	if (pid < 0) {
		err_if(1);
		cleanup(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process */
	if (pid > 0) {
		/* printf("Child process created: %d\n", pid); */
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Set file permissions 750 */
	umask(027);

	/* Create a new SID for the child process and */
	/* detach the process from the parent (normally a shell) */
	if (setsid() < 0) {
		err_if(1);
		cleanup(EXIT_FAILURE);
	}

	/* Change the current working directory.  This prevents the current
	   directory from being locked; hence not being able to remove it. */
	if (chdir(DAEMON_WORKDIR) < 0) {
		/* Log any failure here */
		err_if(1);
		cleanup(EXIT_FAILURE);
	}

	/* Route I/O connections */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* write pidfile and try to lock it */
	if (pid_file_open(pidfile, &pidfd) == -1) {
		log_print(L_ERR, "can't open pidfile: %s\n", pidfile);
		cleanup(EXIT_FAILURE);
	} else {
		pidfile_locked = YES;
		log_print(L_INF, "%s created.", pidfile);
	}

	/* Cancel certain signals */
	signal(SIGCHLD, SIG_DFL); /* A child process dies */
	signal(SIGTSTP, SIG_IGN); /* Various TTY signals */
	signal(SIGTTOU, SIG_IGN); /* Ignore TTY background writes */
	signal(SIGTTIN, SIG_IGN); /* Ignore TTY background reads */

	/* Trap signals that we expect to receive */
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
}

void
cleanup(int state)
{

	/* free msg queue */
	if (msg_queue_on == YES) {
		msg_queue_free();
		log_print(L_INF, "msg queue freeed");
	}

	/* close listing tcp socket */
	if (socketfd > 0) {
		if (sock_close(socketfd) == -1)
			log_print(L_ERR, "can't close port: %d", port);
		else
			log_print(L_INF, "port %d closed", port);
	}

	/* close bus device */
	if (busfd > 0) {
		if (eb_bus_close() == -1)
			log_print(L_ERR, "can't close device: %s", device);
		else
			log_print(L_INF, "%s closed", device);
	}

	/* close rawfile */
	if (rawdump == YES) {
		if (eb_raw_file_close() == -1)
			log_print(L_ERR, "can't close rawfile: %s\n", rawfile);
		else
			log_print(L_INF, "%s closed", rawfile);
	}
			

	/* free mem for ebus commands */
	eb_cmd_dir_free();

	if (foreground == NO) {

		/* delete PID file */
		if (pidfile_locked)
			if (pid_file_close(pidfile, pidfd) == -1)
				log_print(L_INF, "%s deleted", pidfile);

		/* Reset all signal handlers to default */
		signal(SIGCHLD, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGHUP,  SIG_DFL);
		signal(SIGINT,  SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		/* print end message */
		log_print(L_ALL, DAEMON_NAME " " DAEMON_VERSION " stopped");
		syslog(LOG_INFO, DAEMON_NAME " " DAEMON_VERSION " stopped");
	}

	/* close logging system */
	log_close();

	exit(state);
}


void
main_loop(void)
{
	int maxfd, sfd_closed, timeout_reached;
	fd_set listenfds;
	struct timeval timeout;

	sfd_closed = NO;
	timeout_reached = NO;

	FD_ZERO(&listenfds);
	FD_SET(busfd, &listenfds);
	FD_SET(socketfd, &listenfds);

	maxfd = socketfd;

	/* busfd should be always lower then socketfd */
	if (busfd > socketfd) {
		log_print(L_ERR, "busfd %d > %d socketfd", busfd, socketfd);
		cleanup(EXIT_FAILURE);
	}

	for (;;) {
		fd_set readfds;
		int readfd;
		int ret;

		/* set select timeout 10 secs */
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;		

		/* set readfds to inital listenfds */
		readfds = listenfds;

		/* check if the bus device is working */
		if (eb_bus_valid() < 0 || timeout_reached == YES) {
			timeout_reached = NO;
				
			if (busfd > 0 && sfd_closed == NO) {
				log_print(L_ERR, "bus device is invalid");
				sfd_closed = YES;

				/* close listing tcp socket */
				if (socketfd > 0) {
					if (sock_close(socketfd) == -1)
						log_print(L_ERR, "can't close port: %d", port);
					else
						log_print(L_INF, "port %d closed", port);
				}
				
				/* close bus device */
				if (eb_bus_close() == -1)
					log_print(L_ERR, "can't close device: %s", device);
				else
					log_print(L_INF, "%s closed", device);

			}

			/* need sleep to prevent high cpu consumption */
			sleep(1);

			/* open bus device */
			if (eb_bus_open(device, &busfd) == 0) {
				log_print(L_INF, "%s opened", device);
				sfd_closed = NO;
			}

			/* open listing tcp socket */
			if (sfd_closed == NO && sock_open(&socketfd, port, localhost) == 0)
				log_print(L_INF, "port %d opened", port);

			continue;
		}

		ret = select(maxfd + 1, &readfds, NULL, NULL, &timeout);

		/* timeout after 10 secs means that ebus is probably
		   disconnected or BUS device is dead */
		if (ret == 0) {
			log_print(L_WAR, "select timeout (%d) reached", timeout.tv_sec);
			timeout_reached = YES;
			continue;

		/* ignore signals */
		} else if ((ret < 0) && (errno == EINTR)) {
			/* log_print(L_NOT, "get signal at select: %s", strerror(errno)); */
			continue;

		/* on other errors */
		} else if (ret < 0) {
			err_if(1);
			cleanup(EXIT_FAILURE);
		}

		/* new data from bus device? */
		if (FD_ISSET(busfd, &readfds)) {
			
			/* get cycle message from bus */
			ret = eb_cyc_data_recv();

			/* send msg to bus - only when cyc buf is empty */
			if (ret == 0 && msg_queue_entries() > 0) {
				char tcpbuf[SOCKET_BUFSIZE];
				char data[MSG_QUEUE_MSG_SIZE];
				int tcpbuflen, id, clientfd;
				
				memset(tcpbuf, '\0', sizeof(tcpbuf));
				tcpbuflen = sizeof(tcpbuf);

				memset(data, '\0', sizeof(data));

				/* get next entry from msg queue */
				msg_queue_msg_del(&id, data, &clientfd);

				/* just do it */		
				eb_execute(id, data, tcpbuf, &tcpbuflen);

				/* send answer */
				sock_client_write(clientfd, tcpbuf, tcpbuflen);
			}
	
		}

		/* new incoming connection at TCP port arrived? */
		if (FD_ISSET(socketfd, &readfds)) {

			/* get new TCP client fd*/
			ret = sock_client_accept(socketfd, &readfd);
			if (readfd >= 0) {
				/* add new TCP client fd to listenfds */
				FD_SET(readfd, &listenfds);
				(readfd > maxfd) ? (maxfd = readfd) : (1);
			}
		}

		/* run through connected sockets for new data */
		for (readfd = socketfd + 1; readfd <= maxfd; ++readfd) {

			/* check all connected clients */
			if (FD_ISSET(readfd, &readfds)) {
				char tcpbuf[SOCKET_BUFSIZE];
				char data[MSG_QUEUE_MSG_SIZE];
				int tcpbuflen;

				memset(tcpbuf, '\0', sizeof(tcpbuf));
				tcpbuflen = sizeof(tcpbuf);

				memset(data, '\0', sizeof(data));

				/* get message from client */
				ret = sock_client_read(readfd, tcpbuf, &tcpbuflen);

				/* remove dead TCP client */
				if (ret < 0) {
					FD_CLR(readfd, &listenfds);
					continue;
				} 

				/* handle different commands */
				if (strncasecmp("shutdown", tcpbuf, 8) == 0)
					cleanup(EXIT_SUCCESS);
					
				if (strncasecmp("loglevel", tcpbuf, 8) == 0) {
					strncpy(loglevel, tcpbuf, strlen(tcpbuf));
					log_level(loglevel);
					continue;
				}
				
				/* search ebus command */
				if (tcpbuflen > 0)
					ret = eb_cmd_search_com(tcpbuf, data);
				else
					ret = -1;

				/* command not found */
				if (ret < 0) {
					memset(tcpbuf, '\0', sizeof(tcpbuf));
					strcpy(tcpbuf, "command not found\n");
					tcpbuflen = strlen(tcpbuf);
					
					/* send answer */
					sock_client_write(readfd, tcpbuf, tcpbuflen);

				} else {
					msg_queue_msg_add(ret, data, readfd);
				}
				
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int tmp;
	
	/* set progname */
	progname = (const char *)strrchr(argv[0], '/');
	progname = progname ? (progname + 1) : argv[0];

	/* read command line  */
	cmdline(&argc, &argv);
	
	/* set default cfgfile */
	if (*cfgfile == '\0')
		strncpy(cfgfile , DAEMON_CFGFILE, strlen(DAEMON_CFGFILE));

	/* read config file */
	if (cfg_file_read(cfgfile, cfg, cfglen) == -1)
		fprintf(stderr, "can't open cfgfile: %s ==> " \
				"build in settings will be used\n", cfgfile);	

	/* set unset configuration */
	set_unset();

	/* print configuration */
	if (settings == YES)
		cfg_print(cfg, cfglen);

	
	/* set ebus configuration */
	eb_set_nodevicecheck(nodevicecheck);
	eb_set_rawdump(rawdump);
	eb_set_showraw(showraw);

	tmp = (eb_htoi(&address[0])) * 16 + (eb_htoi(&address[1]));
	eb_set_qq((unsigned char) tmp);

	eb_set_get_retry(get_retry);
	eb_set_skip_ack(skip_ack);
	eb_set_max_wait(max_wait);
	eb_set_send_retry(send_retry);
	eb_set_print_size(print_size);

	/* open log */
	log_level(loglevel);
	log_open(logfile, foreground);	

	/* to be daemon */
	if (foreground == NO) {
		log_print(L_ALL, DAEMON_NAME " " DAEMON_VERSION " started");
		syslog(LOG_INFO, DAEMON_NAME " " DAEMON_VERSION " started");
		daemonize();
	}

	/* read ebus command configuration files */
	if (eb_cmd_dir_read(cfgdir, extension) == -1)
		log_print(L_WAR, "error during read command file");

	/* open raw file */
	if (rawdump == YES) {
		if (eb_raw_file_open(rawfile) == -1) {
			log_print(L_ALL, "can't open rawfile: %s", rawfile);
			cleanup(EXIT_FAILURE);
		} else {
			log_print(L_INF, "%s opened", rawfile);
		}

	}

	/* open bus device */
	if (eb_bus_open(device, &busfd) == -1) {
		log_print(L_ALL, "can't open device: %s", device);
		cleanup(EXIT_FAILURE);
	} else {
		log_print(L_INF, "%s opened", device);
	}


	/* open listing tcp socket */
	if (sock_open(&socketfd, port, localhost) == -1) {
		log_print(L_ALL, "can't open port: %d", port);
		cleanup(EXIT_FAILURE);
	} else {
		log_print(L_INF, "port %d opened", port);
	}

	/* init msg queue */
	if (msg_queue_init() == -1) {
		log_print(L_ALL, "can't initialize msg queue");
		cleanup(EXIT_FAILURE);
	} else {
		msg_queue_on = YES;
		log_print(L_INF, "msg queue initialized");
	}

	/* enter main loop */
	main_loop();

	cleanup(EXIT_SUCCESS);

	return 0;
}

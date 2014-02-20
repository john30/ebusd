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
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

#include "ebus-decode.h"
#include "ebus-bus.h"


static int serialfd = -1;
static const char *device = SERIAL_DEVICE;
static const char *progname;
static int type = EBUS_MSG_MASTER_SLAVE;
static int prompt = NO;

void
print_msg(const char *pre, const unsigned char *buf, int buflen,
							const char *post)
{
	int i;

	fprintf(stdout, "%s", pre);
	for (i = 0; i < buflen; i++)
		fprintf(stdout, " %02x", buf[i]);
	fprintf(stdout, "%s\n", post);
}

void
usage(void)
{
	fprintf(stdout, "\nUsage: %s [OPTION] <ZZ PB SB NN DBx>\n\n"
	"  <ZZ PB SB NN DBx>  spaces within message be removed.\n\n"  
	"  -a --address  set bus address. (%02x)\n"
	"  -d --device   use a specified serial device. (%s)\n"	
	"  -p --prompt   stay on input prompt.\n"
	"  -r --retry    max retry getting bus. (%d)\n"
	"  -s --skip     skipped ACK bytes after get-bus error. (%d)\n"
	"  -t --type     message type. (%d)\n"
	"                 1 = Broadcast, 2 = Master-Master, 3 = Master-Slave\n"
	"  -w --wait     wait time for QQ compare. (~%d usec)\n"
	"  -h --help     print this message.\n"
	"\n",
	progname,
	EBUS_QQ,
	device,
	EBUS_GET_RETRY,
	EBUS_SKIP_ACK,
	type,
	EBUS_MAX_WAIT);
}

void
cmdline(int *argc, char ***argv)
{
	static struct option opts[] = {
		{"address",    required_argument, NULL, 'a'},
		{"device",     required_argument, NULL, 'd'},
		{"prompt",     no_argument,       NULL, 'p'},
		{"retry",      required_argument, NULL, 'r'},
		{"skip",       required_argument, NULL, 's'},
		{"type",       required_argument, NULL, 't'},
		{"wait",       required_argument, NULL, 'w'},
		{"help",       no_argument,       NULL, 'h'},
		{NULL,         no_argument,       NULL,  0 },
	};

	for (;;) {
		int i;

		i = getopt_long(*argc, *argv, "a:d:pr:s:t:w:h", opts, NULL);

		if (i == -1) {
			*argc = optind;			
			break;
		}

		switch (i) {
		case 'a':
			if (isxdigit(*optarg) && strlen(optarg) == 2) {
				int tmp;
				tmp = (eb_htoi(&optarg[0])) * 16 +
					(eb_htoi(&optarg[1]));

				eb_set_qq((unsigned char) tmp);	
			}
			break;
		case 'd':
			device = optarg;
			break;			
		case 'p':
			prompt = YES;
			break;
		case 'r':
			if (isdigit(*optarg)) {
				eb_set_get_retry(atoi(optarg));
			}
			break;
		case 's':
			if (isdigit(*optarg)) {
				eb_set_skip_ack(atoi(optarg));
			}
			break;
		case 't':
			if (isdigit(*optarg))
				type = atoi(optarg);
			break;			
		case 'w':
			if (isdigit(*optarg)) {
				eb_set_max_wait(atol(optarg));
			}
			break;
		case 'h':
		default:
			usage();
			exit(EXIT_FAILURE);
			break;
		}
	}
}

int
main(int argc, char *argv[])
{
	int i, j, k, end, ret, val, max_argc, buslen;
	int in[SERIAL_BUFSIZE + 1];
	char byte;
	unsigned char msg[SERIAL_BUFSIZE + 1], bus[TMP_BUFSIZE];

	progname = (const char *)strrchr(argv[0], '/');
	progname = progname ? (progname + 1) : argv[0];

	max_argc = argc;
	cmdline(&argc, &argv);

	val = 0;

	if (prompt) {

		end = 0;
		do {
			byte = '\0';
			i = 0;
			printf("msg: ");
			while ((byte = fgetc(stdin)) != EOF) {

				if (byte == '\n')
					break;

				if (byte == 'q') {
					end = 1;
					break;
				}

				if (i < (int) sizeof(in)) {

					ret = eb_htoi(&byte);
					if (ret != -1) {
						in[i] = ret;
						i++;
					}
			
				} else {
					break;
				}
			}

			if (!end && i > 0) {
				
				memset(msg, '\0', sizeof(msg));
				memset(bus, '\0', sizeof(bus));

				for (j = 0, k = 0; j < i; j += 2, k++)
					msg[k] = (unsigned char)
							(in[j]*16 + in[j+1]);
		
				ret = eb_serial_open(device, &serialfd);
				if (ret < 0)
					fprintf(stdout, "Error open %s.\n", device);
					
				if (ret == 0) {
					ret = eb_send_data(msg, k, type, bus, &buslen);
					if (ret == 0) {
						fprintf(stdout, "res:");
						eb_print_result();
					}
						
				}
				
				ret = eb_serial_close();
				if (ret < 0)
					fprintf(stdout, "Error close %s.\n", device);

			}

		} while (end == 0);
	} else {

		memset(in, '\0', sizeof(in));
		k = 0;
		i = 0;
		for (k = argc; k < max_argc; k++) {
			j = 0;
			while (argv[k][j] != '\0') {
				byte = argv[k][j];
				if (i < (int) sizeof(in)) {

					ret = eb_htoi(&byte);
					if (ret != -1) {
						in[i] = ret;
						i++;
					}
				}
				j++;
			}
		}

		memset(bus, '\0', sizeof(bus));
		memset(msg, '\0', sizeof(msg));
		for (j = 0, k = 0; j < i; j += 2, k++)
			msg[k] = (unsigned char) (in[j]*16 + in[j+1]);

		if (k > 0) {

			ret = eb_serial_open(device, &serialfd);
			if (ret < 0)
				fprintf(stdout, "Error open %s.\n", device);
				
			if (ret == 0) {
				ret = eb_send_data(msg, k, type, bus, &buslen);
				val = ret;
				if (ret == 0) {
					if (type == EBUS_MSG_MASTER_SLAVE)
						eb_print_result();

					if (type == EBUS_MSG_MASTER_MASTER)
						fprintf(stdout, " %d\n", ret);
				}
					
			}
			
			ret = eb_serial_close();
			if (ret < 0)
				fprintf(stdout, "Error close %s.\n", device);
		}
		
	}
	
	return val;
}

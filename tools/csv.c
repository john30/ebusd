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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>
#include <ctype.h>

#include "log.h"
#include "ebus-cmd.h"
#include "ebus-bus.h"

/* global variables */
const char *progname;

static const char *cfgdir = NULL;
static int all = NO;
static int cyc = NO;
static int detail = NO;
static int get = NO;
static int set = NO;
static char loglevel[] = {"ALL"};


void
usage(void)
{
	fprintf(stdout, "\nUsage: %s [OPTION] cfgdir\n\n"
	"  -a --all      print ALL\n"
	"  -c --cyc      print CYC\n"
	"  -d --detail   print DETAIL\n"	
	"  -g --get      print GET\n"
	"  -s --set      print SET\n"
	"  -h --help     print this message.\n"
	"\n",
	progname);
}

void
cmdline(int *argc, char ***argv)
{
	static struct option opts[] = {
		{"all",        no_argument,       NULL, 'a'},
		{"cyc",        no_argument,       NULL, 'c'},
		{"detail",     no_argument,       NULL, 'd'},		
		{"get",        no_argument,       NULL, 'g'},
		{"set",        no_argument,       NULL, 's'},
		{"help",       no_argument,       NULL, 'h'},
		{NULL,         no_argument,       NULL,  0 },
	};

	for (;;) {
		int i;

		i = getopt_long(*argc, *argv, "acdgsh", opts, NULL);

		if (i == -1) {
			*argc = optind;			
			break;
		}

		switch (i) {
		case 'a':
			cyc = YES;
			get = YES;
			set = YES;
			break;			
		case 'c':
			cyc = YES;
			break;
		case 'd':
			detail = YES;
			break;			
		case 'g':
			get = YES;
			break;	
		case 's':
			set = YES;
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
	
	/* set progname */
	progname = (const char *)strrchr(argv[0], '/');
	progname = progname ? (progname + 1) : argv[0];

	cmdline(&argc, &argv);

	cfgdir = argv[argc];

	/* open log */
	log_level(loglevel);
	log_open("", 1);

	if (cfgdir)
		eb_cmd_dir_read(cfgdir, "csv");
	else
		usage();

	if (cyc)
		eb_cmd_print("cyc", all, detail);

	if (get)
		eb_cmd_print("get", all, detail);
		
	if (set)
		eb_cmd_print("set", all, detail);		



	//~ Master Slave
	//~ const unsigned char tmp[] = {'\x10','\x08','\xb5','\x10','\x09','\x00','\x02','\x40','\x00',
				     //~ '\x00','\x00','\x00','\x00','\x02','\x15','\x00','\x00','\x00','\x00'};
				     //~ '\x00','\x00','\x00','\x00','\x02','\x15','\xff','\x00','\x00','\x00'};	
				     //~ '\x00','\x00','\x00','\x00','\x02','\x15','\x00','\x01','\x02','\x99','\x00'};
				     //~ '\x00','\x00','\x00','\x00','\x02','\x15','\x00','\x02','\xe2','\x67','\x41','\x00'};
	//~ char cmd[] = "cyc ms test ma";
	//~ char cmd[] = "cyc ms test sd";
	//~ char cmd[] = "cyc ms test sa";
	//~ char cmd[] = "cyc ms test md";
	//~ char cmd[] = "cyc ms test";

	//~ const unsigned char tmp[] = {'\x10','\x08','\xb5','\x09','\x03','\x29','\x0f','\x00','\x56','\x00',
	                             //~ '\x05','\x0f','\x00','\xa9','\x01','\x00','\x00','\x86','\x00'};
	//~ char cmd[] = "cyc mv brine_in";

	//~ const unsigned char tmp[] = {'\xff','\x08','\xb5','\x11','\x01','\x01','\x2d','\x00','\x09','\x62',
				     //~ '\xff','\x00','\x80','\x00','\x54','\x00','\x00','\xff','\x23','\x00'};

	//~ char cmd[] = "cyc burner temps";
	//~ char cmd[] = "cyc burner temps_raw";

	// 31835
	//~ const unsigned char tmp[] = {'\xff','\x08','\xb5','\x09','\x03','\x0d','\xbc','\x00','\x12',
				     //~ '\x00','\x04','\x5b','\x7c','\x00','\x00','\x75','\x00'};
//~ 
//~ 
	//~ char cmd[] = "cyc ms test";

	//~ const unsigned char tmp[] = {'\x03','\xe0','\xb5','\x21','\x05','\x00','\x05','\x07','\x00','\xe7',
				     //~ '\x64','\x80','\x42','\x40','\x10','\x11','\xd0','\x05','\x74','\x64',
				     //~ '\xd2','\xea','\x01','\xa9','\x10','\x08','\xb5','\x09','\x03','\x29',
				     //~ '\xb8','\x01','\xfd','\x00','\x03','\xb8','\x01','\x00','\x70','\x00'};

	//~ const unsigned char tmp[] = {'\x03','\xe0','\xb5','\x21','\x05','\x00','\x02','\x07','\x00','\xe7',
				     //~ '\x65','\x80','\x42','\x40','\xa0','\x10','\xd0','\x12','\xd0','\x10',
				     //~ '\x10','\x50','\x1b','\x90','\x0d','\xfc','\x23','\xb5','\x05','\x07',
				     //~ '\x2b','\x00','\x01','\x00','\x00','\x00','\x00','\x79','\x00','\x00',
				     //~ '\x00','\x00'};
//~ 
	//~ char cmd[] = "cyc vwl OutUnitData";

	//~ Master Master
	//~ 10 03 05 07 09 bb 06 2e 02 00 80 ff 6e ff 8f 00
	//~ const unsigned char tmp[] = {'\x10','\x03','\x05','\x07','\x09','\xbb','\x06','\x1b','\x02','\x00',
				     //~ '\x80','\xff','\x6e','\xff','\xf2','\x00'};
				     				     
	//~ const unsigned char tmp[] = {'\x10','\x23','\xb5','\x04','\x01','\x31','\xf6','\x00'};
	//~ const unsigned char tmp[] = {'\x10','\x23','\xb5','\x04','\x01','\x31','\xf6','\xff'};
	//~ char cmd[] = "cyc mm test test";
	//~ char cmd[] = "cyc mm test stat";
	//~ char cmd[] = "cyc mm test";

	//~ Broadcast
	//~ const unsigned char tmp[] = {'\x10','\xfe','\xb5','\x16','\x08','\x00','\x02','\x23','\x16','\x28','\x02','\x04','\x13','\x92'};
	//~ char cmd[] = "cyc broad date_time day";
	//~ char cmd[] = "cyc broad date_time";

	//~ 10 46 11 2e 03 00 02 11 02
	//~ const unsigned char tmp[] = {'\x10','\x46','\x11','\x2e','\x03','\x00','\x02','\x11','\x02'};
	//~ char cmd[] = "cyc broad 1";

//###############################################################

	//~ const int tmplen = sizeof(tmp) / sizeof(unsigned char);
 	//~ char data[CMD_DATA_SIZE], tcpbuf[CMD_DATA_SIZE];
	//~ memset(data, '\0', sizeof(data));
	//~ memset(tcpbuf, '\0', sizeof(tcpbuf));
	//~ int id, tcpbuflen;
//~ 
	//~ fprintf(stdout, "Test command >%s<\n", cmd);
	//~ eb_print_hex(tmp, tmplen);
//~ 
	//~ eb_cyc_data_process(tmp, tmplen);
//~ 
	//~ id = eb_cmd_search_com_cyc(&tmp[1], tmplen - 1);
//~ 
	//~ eb_execute(id, data, tcpbuf, &tcpbuflen);
//~ 
	//~ fprintf(stdout, "tcpbuf: %s tcpbuflen: %d\n", tcpbuf, tcpbuflen);
	
//###############################################################

//~ unsigned long val;
//~ eb_print_hex(&tmp[11], 4);
//~ fprintf(stdout, "%d %d %d %d\n", tmp[11], (tmp[12] << 8), (tmp[13] << 16), (tmp[14] << 24));
//~ fprintf(stdout, "%d\n", tmp[11] + (tmp[12] << 8) + (tmp[13] << 16) + (tmp[14] << 24));

//###############################################################

	eb_cmd_dir_free();	

	return 0;
}

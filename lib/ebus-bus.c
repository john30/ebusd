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

/**
 * @file ebus-bus.c
 * @brief ebus communication functions
 * @author roland.jax@liwest.at
 * @version 0.1
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>

#include "log.h"
#include "ebus-decode.h"
#include "ebus-cmd.h"
#include "ebus-bus.h"



static struct send_data send_data;

static struct recv_data recv_data;

static int rawdump = NO;
static int showraw = NO;

static int get_retry = EBUS_GET_RETRY;
static int skip_ack = EBUS_SKIP_ACK;
static long max_wait = EBUS_MAX_WAIT;
static int send_retry = EBUS_SEND_RETRY;
static int print_size = EBUS_PRINT_SIZE;

static unsigned char qq = EBUS_QQ;

static unsigned char ack = EBUS_ACK;
static unsigned char nak = EBUS_NAK;
static unsigned char syn = EBUS_SYN;

static int sfd;
static struct termios oldtio;

static FILE *rawfp = NULL;



void
eb_set_rawdump(int dump)
{
	rawdump = dump;	
}

void
eb_set_showraw(int show)
{
	showraw = show;	
}

void
eb_set_qq(unsigned char src)
{
	qq = src;	
}

void
eb_set_get_retry(int retry)
{
	get_retry = retry;
}

void
eb_set_skip_ack(int skip)
{
	skip_ack = skip;
}

void
eb_set_max_wait(long usec)
{
	max_wait = usec;
}

void
eb_set_send_retry(int retry)
{
	send_retry = retry;
}

void
eb_set_print_size(int size)
{
	print_size = size;
}


int
eb_diff_time(const struct timeval *tact, const struct timeval *tlast, struct timeval *tdiff)
{
    long diff;

    diff = (tact->tv_usec + 1000000 * tact->tv_sec) -
	   (tlast->tv_usec + 1000000 * tlast->tv_sec);
	   
    tdiff->tv_sec = diff / 1000000;
    tdiff->tv_usec = diff % 1000000;

    return (diff < 0);
}



int
eb_raw_file_open(const char *file)
{
	rawfp = fopen(file, "w");
	err_ret_if(rawfp == NULL, -1);

	return 0;
}

int
eb_raw_file_close(void)
{
	int ret;

	ret = fflush(rawfp);
	err_ret_if(ret == EOF, -1);

	ret = fclose(rawfp);
	err_ret_if(ret == EOF, -1);

	return 0;
}

int
eb_raw_file_write(const unsigned char *buf, int buflen)
{
	int ret, i;

	for (i = 0; i < buflen; i++) {		
		ret = fputc(buf[i], rawfp);
		err_ret_if(ret == EOF, -1);
	}

	ret = fflush(rawfp);
	err_ret_if(ret == EOF, -1);

	return 0;
}



int
eb_serial_valid()
{
	int serial;

	if (ioctl(sfd, TIOCMGET, &serial) < 0) 
		return -1;
	else
		return 0;	
}

int
eb_serial_open(const char *dev, int *fd)
{
	int ret;
	struct termios newtio;

	sfd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	//~ err_ret_if(sfd < 0, -1);
	if (sfd < 0)
		return -1;

	ret = fcntl(sfd, F_SETFL, 0);
	err_ret_if(ret < 0, -1);

	/* check if the usb device is working */
	ret = eb_serial_valid();
	err_ret_if(ret < 0, -2);

	/* save current settings of serial port */
	ret = tcgetattr(sfd, &oldtio);
	err_ret_if(ret < 0, -1);

	memset(&newtio, '\0', sizeof(newtio));

	newtio.c_cflag = SERIAL_BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag &= ~OPOST;

	newtio.c_cc[VMIN]  = 1;
	newtio.c_cc[VTIME] = 0;

	ret = tcflush(sfd, TCIFLUSH);
	err_ret_if(ret < 0, -1);

	/* activate new settings of serial port */
	ret = tcsetattr(sfd, TCSANOW, &newtio);
	err_ret_if(ret < 0, -1);

	*fd = sfd;

	return 0;
}

int
eb_serial_close(void)
{
	int ret;

	/* activate old settings of serial port */
	ret = tcsetattr(sfd, TCSANOW, &oldtio);
	/* Ignore this error otherwise USB port may stay unclosed */
	/* err_ret_if(ret < 0, -1); */

	/* Close file descriptor from serial device */
	ret = close(sfd);
	err_ret_if(ret < 0, -1);

	return 0;
}

int
eb_serial_send(const unsigned char *buf, int buflen)
{
	int ret, val;
	
	/* write msg to ebus device */
	val = write(sfd, buf, buflen);
	err_ret_if(val < 0, -1);
	
	ret = tcflush(sfd, TCIOFLUSH);
	err_ret_if(ret < 0, -1);

	return val;
}

int
eb_serial_recv(unsigned char *buf, int *buflen)
{
	int ret;

	/* flush deactivated - brings us an error */
	/* tcflush(sfd, TCIOFLUSH); */

	/* read msg from ebus device */
	*buflen = read(sfd, buf, *buflen);
	err_if(*buflen < 0);

	if (*buflen < 0) {
		log_print(L_WAR, "error read serial device");
		return -1;
	}

	if (*buflen > SERIAL_BUFSIZE) {
		log_print(L_WAR, "read data len > %d", SERIAL_BUFSIZE);
		return -3;
	}

	/* print bus */
	if (showraw == YES)
		eb_print_hex(buf, *buflen);

	/* dump raw data*/
	if (rawdump == YES) {
		ret = eb_raw_file_write(buf, *buflen);
		if (ret < 0)
			log_print(L_WAR, "can't write rawdata");
	}

	return 0;
}



void
eb_print_result(void)
{
	int i;
	//fprintf(stdout, ">>> ");
	for (i = 0; i < recv_data.len; i++)
		fprintf(stdout, " %02x", recv_data.msg[i]);
	fprintf(stdout, "\n");
}

void
eb_print_hex(const unsigned char *buf, int buflen)
{
	int i, j, k;
	char msg[3 * print_size];	

	memset(msg, '\0', sizeof(msg));
	k = 1;

	for (i = 0, j = 0; i < buflen; i++, j++) {			
		sprintf(&msg[3 * j], " %02x", buf[i]);
		
		if (j + 1 == print_size) {
			log_print(L_EBH, "%d%s", k, msg);
			memset(msg, '\0', sizeof(msg));		
			j = -1;
			k++;
		}
	}

	if (j > 0) {
		if (k > 1)
			log_print(L_EBH, "%d%s", k, msg);
		else
			log_print(L_EBH, " %s", msg);
	}
}



void
eb_recv_data_get(unsigned char *buf, int *buflen)
{
	memcpy(buf, recv_data.msg, recv_data.len);	
	*buflen = recv_data.len;
}

void
eb_recv_data_prepare(const unsigned char *buf, int buflen)
{
	unsigned char tmp[SERIAL_BUFSIZE];
	int tmplen, crc;

	memset(tmp, '\0', sizeof(tmp));

	/* reset struct */
	memset(&recv_data, '\0', sizeof(recv_data));	

	/* set recv_data.msg_esc */
	memcpy(&recv_data.msg_esc[0], buf, buflen);
	recv_data.len_esc = buflen;

	/* set recv_data.crc_calc and .crc_recv */
	if (buf[buflen - 2] == EBUS_SYN_ESC_A9) {
		recv_data.crc_calc = eb_calc_crc(buf, buflen - 2);
		if (buf[buflen - 1] == EBUS_SYN_ESC_01)
			recv_data.crc_recv = EBUS_SYN;
		else
			recv_data.crc_recv = EBUS_SYN_ESC_A9;

		crc = 2;

	} else {
		recv_data.crc_calc = eb_calc_crc(buf, buflen - 1);
		recv_data.crc_recv = buf[buflen - 1];

		crc = 1;
	}

	/* set recv_data.msg */
	memcpy(tmp, buf, buflen - crc);
	tmplen = buflen - crc;

	eb_unesc(tmp, &tmplen);

	memcpy(&recv_data.msg[0], tmp, tmplen);
	recv_data.len = tmplen;

}

int
eb_recv_data(unsigned char *buf, int *buflen)
{
	unsigned char tmp[SERIAL_BUFSIZE], msg[SERIAL_BUFSIZE];
	int tmplen, msglen, ret, i, esc, found;

	memset(msg, '\0', sizeof(msg));
	msglen = 0;
	
	esc = 0;
	found = 99;
	
	/* do until found necessary string*/
	do {
		memset(tmp, '\0', sizeof(tmp));
		tmplen = sizeof(tmp);

		ret = eb_serial_recv(tmp, &tmplen);
		if (ret < 0)
			return -1;
		
		if (tmplen > 0) {

			/* preset tmp buffer with not read bytes from get_bus */
			if (*buflen > 0) {
				
				/* save temporarily tmp in msg buffer */
				memcpy(msg, tmp, tmplen);
				msglen = tmplen;

				/* copy input data into tmp buffer */
				memset(tmp, '\0', sizeof(tmp));
				memcpy(tmp, buf, *buflen);
				tmplen = *buflen;

				/* set input data buffer len to 0 */
				*buflen = 0;

				/* copy saved bus data back to tmp buffer */
				memcpy(&tmp[tmplen], msg, msglen);
				tmplen += msglen;

				/* reset msg buffer */
				memset(msg, '\0', sizeof(msg));
				msglen = 0;							
			}
			
			
			i = 0;
			while (i < tmplen) {

				msg[msglen] = tmp[i];
				msglen++;
						
				/* SYN */
				if (msg[0] == EBUS_SYN) {
					found = -2;
					break;
				}

				/* get end of message */
				if (msglen > 1) {

					if (msg[msglen - 1] == EBUS_SYN_ESC_A9)
						esc++;

					if (msglen == (2 + msg[0] + esc)) {
						found = 0;
						break;
					}
				}

				/*
				 * something went wrong - plz tell me
				 * we got some SYN during answer or
				 * msglen is out of spec. 1 
				 */
				if (msg[msglen] == EBUS_SYN || msg[0] > 16) {

					found = -3;
					break;
				}

				i++;
			}
		}
	} while (found == 99);

	*buflen = msglen;

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < msglen; i++)
		buf[i] = msg[i];
		
	return found;
}



int
eb_bus_wait_syn(int *skip)
{
	unsigned char buf[SERIAL_BUFSIZE];
	int buflen, ret, i, found;
	
	found = 99;

	/* do until SYN read*/
	do {
		memset(buf, '\0', sizeof(buf));
		buflen = sizeof(buf);

		ret = eb_serial_recv(buf, &buflen);
		if (ret < 0)
			return -1;		

		if (buflen > 0) {
			
			i = 0;
			while (i < buflen) {
				/* break if byte = SYN and it is last byte */
				if (buf[i] == EBUS_SYN && (i + 1) == buflen) {
					found = 1;
					break;
				}
				i++;
			}

			if (*skip > 0)
				*skip -= 1;
		}
	} while (found == 99);
	
	return 0;
}

int
eb_bus_wait(void)
{
	unsigned char buf[SERIAL_BUFSIZE];
	int buflen, ret, skip, retry;
	struct timeval tact, tlast, tdiff;

	skip = 0;
	retry = 0;
	
	do {
		ret = eb_bus_wait_syn(&skip);
		if (ret < 0)
			return -1;	

		/* remember start time */
		gettimeofday(&tlast, NULL);

		/* send QQ */
		ret = eb_serial_send(&qq, 1);
		if (ret < 0)
			return -1;

		gettimeofday(&tact, NULL);
		eb_diff_time(&tact, &tlast, &tdiff);

		/* wait ~4200 usec */
		if (max_wait - tdiff.tv_usec > 0.0 &&
		    max_wait - tdiff.tv_usec <= max_wait)
			usleep(max_wait - tdiff.tv_usec);
		else 
			log_print(L_WAR, "usleep out of range - skipped");

		/* receive 1 byte - must be QQ */
		memset(buf, '\0', sizeof(buf));
		buflen = sizeof(buf);
		ret = eb_serial_recv(buf, &buflen);
		if (ret < 0)
			return -1;		

		/* is sent and read qq byte is equal */
		if (buf[0] == qq && buflen == 1)
			return 0;

		retry++;
		skip = skip_ack + retry;
	
	} while (retry < get_retry);

	/* reached max retry */
	return 1;
}

int
eb_bus_free(void)
{
	int ret, skip;
	ret = 0;
	skip = 0;

	ret = eb_serial_send(&syn, 1);
	if (ret < 0)
		return -1;
		
	ret = eb_bus_wait_syn(&skip);
	if (ret < 0)
		return -1;			

	return 0;
}



int
eb_send_data_get_ack(unsigned char *buf, int *buflen)
{
	unsigned char tmp[SERIAL_BUFSIZE];
	int tmplen, ret, i, j, found;
	
	j = 0;
	found = 99;

	/* do until found necessary string */
	do {
		memset(tmp, '\0', sizeof(tmp));
		tmplen = sizeof(tmp);

		ret = eb_serial_recv(tmp, &tmplen);
		if (ret < 0)
			return -1;	
		
		if (tmplen > 0) {		
			i = 0;
			while (i < tmplen) {
			
				/* compare recv with sent  - is this possible */
				if (tmp[i] != buf[j] && j < *buflen)
					return -2;
				
				/* compare only slaves answer */
				if (j > (*buflen - 1)) {									

					/* ACK */
					if (tmp[i] == EBUS_ACK)
						found = 0;
					
					/* NAK */
					else if (tmp[i] == EBUS_NAK)
						found = 0;
					
					/* SYN */
					else if (tmp[i] == EBUS_SYN)
						found = -3;
					
					/* ??? */
					else 
						found = -4;
					i++;
					break;
					
				}
				i++;
				j++;
			}
		}
	} while (found == 99);

	*buflen = tmplen - i;

	memset(buf, '\0', sizeof(buf));
	for (j = 0; i < tmplen; i++, j++)
		buf[j] = tmp[i];

	return found;
}

void
eb_send_data_prepare(const unsigned char *buf, int buflen)
{
	unsigned char crc[2], tmp[SERIAL_BUFSIZE];
	int tmplen, crclen;

	/* reset struct */
	memset(&send_data, '\0', sizeof(send_data));	

	/* set send_data.msg */
	memcpy(&send_data.msg[0], buf, buflen);
	send_data.len = buflen;


	/* set send_data.msg_esc + send_data.crc */
	memset(tmp, '\0', sizeof(tmp));
	memcpy(tmp, buf, buflen);
	tmplen = buflen;	
	
	eb_esc(tmp, &tmplen);

	memcpy(&send_data.msg_esc[0], &qq, 1);
	memcpy(&send_data.msg_esc[1], tmp, tmplen);
	tmplen++;

	memset(crc, '\0', sizeof(crc));
	send_data.crc = eb_calc_crc(&send_data.msg_esc[0], tmplen);
	crc[0] = send_data.crc;
	crclen = 1;

	if (crc[0] == EBUS_SYN || crc[0] == EBUS_SYN_ESC_A9) {
		/* esc crc */
		eb_esc(crc, &crclen);
		send_data.msg_esc[tmplen] = crc[0];
		tmplen++;
		send_data.msg_esc[tmplen] = crc[1];
		tmplen++;
	} else {
		send_data.msg_esc[tmplen] = crc[0];
		tmplen++;
	}

	send_data.len_esc = tmplen;

}

int
eb_send_data(const unsigned char *buf, int buflen, int type, unsigned char *bus, int *buslen)
{
	unsigned char tmp[SERIAL_BUFSIZE];
	int tmplen, ret, val;
	
	ret = 0;

	eb_send_data_prepare(buf, buflen);
	
	/* fetch AA and send QQ */
	ret = eb_bus_wait();
	if (ret != 0)
		return -1;

	memcpy(bus, send_data.msg_esc, send_data.len_esc);
	*buslen = send_data.len_esc;
	
	/* send message to slave */
	ret = eb_serial_send(&send_data.msg_esc[1], send_data.len_esc - 1);
	if (ret < 0)
		return -1;

	if (type == EBUS_MSG_BROADCAST) {
		/* free bus */
		ret = eb_bus_free();
		if (ret < 0)
			return -1;		
		
		return 0;
	}

	/* get data from bus (we got our sent message too) */
	memset(tmp, '\0', sizeof(tmp));
	memcpy(tmp, &send_data.msg_esc[1], send_data.len_esc - 1);
	tmplen = send_data.len_esc - 1;

	ret = eb_send_data_get_ack(tmp, &tmplen);
	
	if (ret < 0 || ret > 1) {
		/* free bus */
		ret = eb_bus_free();
		if (ret < 0)
			return -1;		
	
		return -1;
	}

	/* first answer from slave is NAK - send message again (inkl. QQ) */
	if (ret == 1) {

		memcpy(&bus[*buslen], &nak, 1);
		*buslen += 1;

		memcpy(&bus[*buslen], send_data.msg_esc, send_data.len_esc);
		*buslen += send_data.len_esc;
			
		/* send message to slave */
		ret = eb_serial_send(&send_data.msg_esc[0], send_data.len_esc);
		if (ret < 0)
			return -1;

		/* get ack from bus (we got our sent message too) */
		memset(tmp, '\0', sizeof(tmp));
		memcpy(tmp, &send_data.msg_esc[0], send_data.len_esc);
		tmplen = send_data.len_esc;

		ret = eb_send_data_get_ack(tmp, &tmplen);		
	
		if (ret == 1) {

			memcpy(&bus[*buslen], &nak, 1);
			*buslen += 1;
			
			/* free bus */
			ret = eb_bus_free();
			if (ret < 0)
				return -1;
	
			return -1;
		}
		
	}

	memcpy(&bus[*buslen], &ack, 1);
	*buslen += 1;

	if (type == EBUS_MSG_MASTER_MASTER) {
		val = ret;
	
		/* free bus */
		ret = eb_bus_free();
		if (ret < 0)
			return -1;		

		return val;
	}

	/* get data - dont reset buffer */	
	ret = eb_recv_data(tmp, &tmplen);
	if (ret < 0)
		return -1;

	eb_recv_data_prepare(tmp, tmplen);

	memcpy(&bus[*buslen], recv_data.msg_esc, recv_data.len_esc);
	*buslen += recv_data.len_esc;

	/* check crc's from recv_data */
	if (recv_data.crc_calc != recv_data.crc_recv) {

		memcpy(&bus[*buslen], &nak, 1);
		*buslen += 1;
		
		/* send message to slave */
		ret = eb_serial_send(&nak, 1);
		if (ret < 0)
			return -1;			

		/* get data from bus (we got our sent message too) */
		memset(tmp, '\0', sizeof(tmp));
		tmplen = 0;

		ret = eb_send_data_get_ack(tmp, &tmplen);		

		/* we compare against nak ! */
		if (ret != 1) {
			/* free bus */
			ret = eb_bus_free();
			if (ret < 0)
				return -1;			
		
			return -1;
		}

		/* get data - don't reset buffer */
		ret = eb_recv_data(tmp, &tmplen);	
		if (ret < 0)
			return -1;

		eb_recv_data_prepare(tmp, tmplen);

		memcpy(&bus[*buslen], recv_data.msg_esc, recv_data.len_esc);
		*buslen += recv_data.len_esc;
		
	}

	if (recv_data.crc_calc != recv_data.crc_recv) {

		memcpy(&bus[*buslen], &nak, 1);
		*buslen += 1;
		
		ret = eb_serial_send(&nak, 1);		
		if (ret < 0)
			return -1;
		
		val = 1;
	} else {

		memcpy(&bus[*buslen], &ack, 1);
		*buslen += 1;
		
		ret = eb_serial_send(&ack, 1);
		if (ret < 0)
			return -1;
			
		val = 0;
	}

	/* free bus */
	ret = eb_bus_free();
	if (ret < 0)
		return -1;	
			
	return val;
}



void
eb_execute(int id, char *data, char *buf, int *buflen)
{
	unsigned char msg[SERIAL_BUFSIZE], hlp[CMD_DATA_SIZE], bus[TMP_BUFSIZE];
	char tmp[TMP_BUFSIZE];
	int ret, msglen, buslen, msgtype, retry, cycdata, pos, len;

	memset(msg, '\0', sizeof(msg));
	msglen = sizeof(msg);
	msgtype = eb_cmd_get_s_type(id);
				
	cycdata = NO;
	ret = -1;
	retry = 0;

	if (eb_cmd_check_type(id, "cyc") == YES) {
		cycdata = YES;		
	} else {
		/* prepare command - if prepare failed buflen > 0 */
		eb_cmd_prepare(id, data, msg, &msglen, buf);
		/* eb_print_hex(msg, msglen); */
	}		

	if (cycdata == NO && strlen(buf) == 0) {
		/* send data to bus */
		while (ret < 0 && retry < send_retry) {
			if (retry > 0)
				log_print(L_NOT, "send retry: %d", retry);
				
			memset(bus, '\0', sizeof(bus));
			buslen = 0;

			ret = eb_send_data(msg, msglen, msgtype, bus, &buslen);
			
			if (buslen > 0)			
				eb_print_hex(bus, buslen);
	
			retry++;
		}
	}

	/* handle answer for sent messages */
	if (cycdata == NO && ret >= 0)  {
				
		if (msgtype == EBUS_MSG_BROADCAST)
			strcpy(buf, "broadcast done\n");

		if (msgtype == EBUS_MSG_MASTER_MASTER) {
			if (ret == 0)
				strcpy(buf, "ACK\n");
			else
				strcpy(buf, "NAK\n");
		}

		if (msgtype == EBUS_MSG_MASTER_SLAVE) {
			if (ret == 0) {
				memset(msg, '\0', sizeof(msg));
				msglen = sizeof(msg);
				eb_recv_data_get(msg, &msglen);
				/* eb_print_hex(msg, msglen); */

				/* decode */
				if (eb_cmd_check_type(id, "set") == YES) {
					strcpy(buf, "ACK\n");
				} else {					
					eb_cmd_decode(id, CMD_PART_SD, data, msg, buf);
					if (strlen(buf) > 0)
						strncat(buf, "\n", 1);
				}
				
			} else {
				strcpy(buf, "NAK\n");
			}
		}

	} else if (cycdata == YES) {
		memset(tmp, '\0', sizeof(tmp));
		memset(hlp, '\0', sizeof(hlp));
		
		eb_cmd_get_cyc_buf(id, msg, &msglen);

		/* CMD_PART_MD */
		pos = 4;
		len = 1 + (int) msg[4];
		memcpy(hlp, &msg[pos], len);


		eb_cmd_decode(id, CMD_PART_MD, data, hlp, tmp);
		strncpy(buf, tmp, strlen(tmp));

		if (msgtype != EBUS_MSG_BROADCAST) {
			/* CMD_PART_SA */
			memset(tmp, '\0', sizeof(tmp));
			memset(hlp, '\0', sizeof(hlp));
			
			pos =  5 + (int) msg[4] + 1;
			len = 2;
			hlp[0] = 0x01;
			memcpy(&hlp[1], &msg[pos], 1);		
			
			eb_cmd_decode(id, CMD_PART_SA, data, hlp, tmp);
			strncat(buf, tmp, strlen(tmp));			
		}

		if (msgtype == EBUS_MSG_MASTER_SLAVE) {
			/* CMD_PART_SD */
			memset(tmp, '\0', sizeof(tmp));
			memset(hlp, '\0', sizeof(hlp));
			
			pos =  5 + (int) msg[4] + 2;
			len = 1 + (int) msg[pos];
			memcpy(hlp, &msg[pos], len);		
			
			eb_cmd_decode(id, CMD_PART_SD, data, hlp, tmp);
			strncat(buf, tmp, strlen(tmp));
			memset(tmp, '\0', sizeof(tmp));
			memset(hlp, '\0', sizeof(hlp));

			/* CMD_PART_MA */
			pos =  5 + (int) msg[4] + 2 +(int) msg[pos] + 2;
			len = 2;
			hlp[0] = 0x01;
			memcpy(&hlp[1], &msg[pos], 1);		
			
			eb_cmd_decode(id, CMD_PART_MA, data, hlp, tmp);
			strncat(buf, tmp, strlen(tmp));			
		}
		
		if (strlen(buf) > 0)
			strncat(buf, "\n", 1);
		else
			strcpy(buf, "error get cyc data\n");
			
	} else {
		strcpy(buf, "error send ebus msg\n");
	}
	
	*buflen = strlen(buf);
	
}



int
eb_cyc_data_process(const unsigned char *buf, int buflen)
{
	unsigned char msg[CMD_DATA_SIZE], hlp[CMD_DATA_SIZE];
	unsigned char crcm_recv, crcm_calc, acks, crcs_recv, crcs_calc, ackm;
	int id, msgtype, msglen, hlplen, mlen, slen, len;

	memset(msg, '\0', sizeof(msg));

	/* search command definition */
	id = eb_cmd_search_com_cyc(&buf[1], buflen - 1);
	
	if (id >= 0) {

		msgtype = eb_cmd_get_s_type(id);

		/* unescape */
		memcpy(msg, buf, buflen);
		msglen = buflen;

		eb_unesc(msg, &msglen);
		
		/* calc crc vom master */
		mlen = 5 + (int) msg[4];
		
		memset(hlp, '\0', sizeof(hlp));
		memcpy(hlp, msg, mlen);
		hlplen = mlen;
		
		eb_esc(hlp, &hlplen);
		
		crcm_calc = eb_calc_crc(hlp, hlplen);
		crcm_recv = msg[mlen];

		if (crcm_calc != crcm_recv) {
			log_print(L_WAR, "%s", "Master CRC Error");
			return -2;
		}

		/* set len */
		len = mlen + 1;
		

		if (msgtype != EBUS_MSG_BROADCAST) {

			/* get ack from slave */
			acks = msg[mlen + 1];

			if (acks == EBUS_NAK) {
				log_print(L_WAR, "%s", "Slave ACK Error");
				//~ return -2;
			}

			/* set len */
			len = mlen + 2;			
		}

		
		if (msgtype == EBUS_MSG_MASTER_SLAVE) {

			/* calc crc vom slave */
			slen = 1 + (int) msg[mlen + 2];

			memset(hlp, '\0', sizeof(hlp));
			memcpy(hlp, &msg[mlen + 2], slen);
			hlplen = slen;

			eb_esc(hlp, &hlplen);

			crcs_calc = eb_calc_crc(hlp, hlplen);
			crcs_recv = msg[mlen + 2 + slen];

			if (crcs_calc != crcs_recv) {
				log_print(L_WAR, "%s", "Slave CRC Error");
				return -2;
			}

			/* get ack from master */
			ackm = msg[mlen + 2 + slen + 1];

			if (ackm == EBUS_NAK) {
				log_print(L_WAR, "%s", "Master ACK Error");
				//~ return -2;
			}

			/* set len */
			len = mlen + 2 + slen + 2;
		}

		/* check len */			
		if (msglen > len) {
			log_print(L_WAR, "%s", "LEN Error");
			return -2;
						
		}
		
		/* save data */
		eb_cmd_set_cyc_buf(id, msg, msglen);
			
	}

	return id;
}

int
eb_cyc_data_recv()
{
	static unsigned char msg[SERIAL_BUFSIZE];
	static int msglen = 0;
	
	unsigned char buf[SERIAL_BUFSIZE];
	int ret, i, buflen;

	char tmp[TMP_BUFSIZE];
	int tmplen;

	memset(buf, '\0', sizeof(buf));
	buflen = sizeof(buf);

	if (msglen == 0)
		memset(msg, '\0', sizeof(msg));

	/* get new data */
	ret = eb_serial_recv(buf, &buflen);
	if (ret < 0)
		return -1;

	i = 0;
	while (i < buflen) {
		
		if (buf[i] != EBUS_SYN) {
			msg[msglen] = buf[i];
			msglen++;
		}

		/* ebus syn sign is reached - decode ebus message */
		if (buf[i] == EBUS_SYN && msglen > 0) {
			eb_print_hex(msg, msglen);
			
			ret = eb_cyc_data_process(msg, msglen);

			if (ret > 0) {
				memset(tmp, '\0', sizeof(tmp));
				eb_execute(ret, "-", tmp, &tmplen);
				tmp[strcspn(tmp,"\n")] = '\0';
				log_print(L_EBS, "%s", tmp);
			}
				
			memset(msg, '\0', sizeof(msg));		
			msglen = 0;
		}
		
		i++;
	}

	return msglen;
}


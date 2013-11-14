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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "log.h"
#include "ebus-bus.h"
#include "utils.h"



struct msg_queue *dummy;
static int msg_entries;

int
msg_queue_entries(void)
{
	return msg_entries;
}

int
msg_queue_init(void)
{
	dummy = (struct msg_queue *) malloc(sizeof(struct msg_queue));
	
	if (dummy != NULL) {
		dummy->id = -1;
		memset(dummy->data, '\0', sizeof(dummy->data));
		dummy->clientfd = -1;
		dummy->prev = NULL;

		msg_entries = 0;
		return 0;
	} 
	
	return -1;
}

void
msg_queue_free(void)
{
	while (dummy->prev != NULL)
		msg_queue_get();
		
	free(dummy);
	msg_entries = 0;
}

void
msg_queue_put(struct msg_queue *new)
{
	struct msg_queue *tmp;

	/* first element */
	if (dummy->prev == NULL) {
		dummy->prev = new;
		new->prev = NULL;	
	} else {
		tmp = dummy;	
		/* get last element */
		while (tmp->prev != NULL)
			tmp = tmp->prev;
			
		tmp->prev = new;
		new->prev = NULL;	
	}
}

void
msg_queue_get(void)
{
	struct msg_queue *tmp;

	/* delete element */
	if (dummy->prev != NULL) {
		tmp = dummy->prev;
		dummy->prev = tmp->prev;
		free(tmp);
	} else {
		log_print(L_ERR, "msg queue empty - should never seen");
	}
}

void
msg_queue_msg_add(int id, char *data, int clientfd)
{
	struct msg_queue *new;

	new = (struct msg_queue *) malloc(sizeof(struct msg_queue));

	if (new != NULL) {
		new->id = id;
		memset(new->data, '\0', sizeof(new->data));
		strncpy(new->data, data, strlen(data));
		new->clientfd = clientfd;
		new->prev = NULL;
		
		msg_queue_put(new);
		msg_entries++;
	
		log_print(L_DBG, "add: id: %d clientfd: %d ==> entries: %d",
					new->id, new->clientfd, msg_entries);
	}
}

void
msg_queue_msg_del(int *id, char *data, int *clientfd)
{
	if (dummy->prev != NULL) {
		*id = dummy->prev->id;
		memset(data, '\0', sizeof(data));
		strncpy(data, dummy->prev->data, strlen(dummy->prev->data));		
		*clientfd = dummy->prev->clientfd;
			
		msg_queue_get();
		msg_entries--;

		log_print(L_DBG, "del: id: %d clientfd: %d ==> entries: %d",
						*id, *clientfd, msg_entries);
		
	} else {
		log_print(L_NOT, "msg queue empty");
	}
}



void
cfg_print(struct config *cfg, int len)
{
	int i;

	fprintf(stdout, "\n");

	for (i = 0; i < len; i++) {

		if (cfg[i].key != NULL && cfg[i].tgt != NULL) {
			fprintf(stdout, "%s = ", cfg[i].key);

			switch (cfg[i].type) {
			case STR:
				fprintf(stdout, "%s\n", (char *) cfg[i].tgt);
				break;
			case BOL:
				if (*(int *) cfg[i].tgt == NO)
					fprintf(stdout, "NO\n");
				else if (*(int *) cfg[i].tgt == YES)
					fprintf(stdout, "YES\n");
				else
					fprintf(stdout, "UNSET\n");
				break;
			case NUM:
				fprintf(stdout, "%d\n", *(int *) cfg[i].tgt);
				break;
			default:
				break;
			}
		}
	}
	fprintf(stdout, "\n");	
}

int
cfg_file_set_param(char *par, struct config *cfg, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		
		if (strncasecmp(par, cfg[i].key, strlen(cfg[i].key)) == 0 &&
		    strlen(par) == strlen(cfg[i].key)) {

			par = strtok(NULL, "\t =\n\r");

			switch (cfg[i].type) {
			case STR:
				if (strlen(cfg[i].tgt) == 0)
					strncpy(cfg[i].tgt , par, strlen(par));
				break;
			case BOL:
				if (*(int *) cfg[i].tgt == UNSET) {
					if (strncasecmp(par, "NO", 2) == 0)
						*(int *) cfg[i].tgt = NO;
					else if (strncasecmp(par, "YES", 3) == 0)
						*(int *) cfg[i].tgt = YES;
					else
						*(int *) cfg[i].tgt = UNSET;
				}				
				break;
			case NUM:
				if (*(int *) cfg[i].tgt == UNSET)
					*(int *) cfg[i].tgt = atoi(par);	
				break;
			default:
				break;				
			}
		
			return 1;
		}
	}

	return 0;
}

int
cfg_file_read(const char *file, struct config *cfg, int len)
{
	int ret;
	char line[CFG_LINELEN];
	char *par, *tmp;
	FILE *fp = NULL;

	

	/* open config file */
	fp = fopen(file, "r");

	/* try local configuration file */
	if (fp == NULL) {
		fprintf(stdout, "configuration file %s not found.\n", file);
		
		tmp = strrchr(file, '/');
		tmp++;
		
		/* open config file */
		fp = fopen(tmp, "r");
		err_ret_if(fp == NULL, -1);
		
		fprintf(stdout, "local configuration file %s used.\n", tmp);
	}

	/* read each line and set parameter */
	while (fgets(line, CFG_LINELEN, fp) != NULL ) {
		par = strtok(line, "\t =\n\r") ;
		
		if (par != NULL && par[0] != '#')
			ret = cfg_file_set_param(par, cfg, len);
	}

	/* close config file */
	ret = fclose(fp);
	err_ret_if(ret == EOF, -1);

	return 0;
}



int
pid_file_open(const char *file, int *fd)
{
	int ret;
	char pid[10];

	*fd = open(file, O_RDWR|O_CREAT, 0600);
	err_ret_if(*fd < 0, -1);

	ret = lockf(*fd, F_TLOCK, 0);
	err_ret_if(ret < 0, -1);

	sprintf(pid, "%d\n", getpid());
	ret = write(*fd, pid, strlen(pid));
	err_ret_if(ret < 0, -1);

	return 0;
}

int
pid_file_close(const char *file, int fd)
{
	int ret;

	ret = close(fd);
	err_ret_if(ret < 0, -1);

	ret = unlink(file);
	err_ret_if(ret < 0, -1);

	return 0;
}



int
sock_open(int *fd, int port, int localhost)
{
	int ret, opt;
	struct sockaddr_in sock;

	*fd = socket(PF_INET, SOCK_STREAM, 0);
	err_ret_if(fd < 0, -1);

	/* todo: verify if this realy work */
	/* prevent "Error Address already in use" error message */
	opt = 1;
	ret = setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	err_ret_if(ret < 0, -1);

	memset((char *) &sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;

	if (localhost == YES)
		sock.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	else
		sock.sin_addr.s_addr = htonl(INADDR_ANY);

	sock.sin_port = htons(port);

	ret = bind(*fd, (struct sockaddr *) &sock, sizeof(sock));
	err_ret_if(ret < 0, -1);

	ret = listen(*fd, 5);
	err_ret_if(ret < 0, -1);

	return 0;
}

int
sock_close(int fd)
{
	int ret;

	ret = close(fd);
	err_ret_if(ret < 0, -1);

	return 0;
}

int
sock_client_accept(int listenfd, int *datafd)
{
	struct sockaddr_in sock;
	socklen_t socklen;

	socklen = sizeof(sock);

	*datafd = accept(listenfd, (struct sockaddr *) &sock, &socklen);
	err_ret_if(*datafd < 0, -1);

	log_print(L_DBG, "client [%d] from %s connected.",
					*datafd, inet_ntoa(sock.sin_addr));

	return 0;
}

int
sock_client_read(int fd, char *buf, int *buflen)
{
	*buflen = read(fd, buf, *buflen);
	err_ret_if(*buflen < 0, -1);
	
	if (strncasecmp("quit", buf , 4) == 0 || *buflen <= 0) {
		/* close tcp connection */
		log_print(L_DBG, "client [%d] disconnected.", fd);
		sock_close(fd);
		return -1;
	}

	if (strchr(buf, '\n') != NULL) {
		buf[strcspn(buf, "\n")] = '\0';
		*buflen -= 1;
	}
	
	log_print(L_NET, ">>> client [%d] %s", fd, buf);

	return 0;
}

int
sock_client_write(int fd, char *buf, int buflen)
{
	int ret;

	/* add <cr> to each line */
	buf[buflen] = '\r';
	buflen++;
	
	ret = write(fd, buf, buflen);
	err_ret_if(ret < 0, -1);

	buf[strcspn(buf,"\n")] = '\0';
	log_print(L_NET, "<<< client [%d] %s", fd, buf);	

	return 0;
}

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

#ifndef UTILS_H_
#define UTILS_H_

#define NUMSTR2(s) #s
#define NUMSTR(s) NUMSTR2(s)

#define CFG_LINELEN         256

#define SOCKET_PORT         8888
#define SOCKET_BUFSIZE      1024

#define MSG_QUEUE_MSG_SIZE  256

enum enum_config {STR, BOL, NUM};

struct config {
	char *key;
	int type;
	void *tgt;
	char *info;
};

struct msg_queue {
	int id;
	char data[MSG_QUEUE_MSG_SIZE + 1];
	int clientfd;
	struct msg_queue *prev;
};

int msg_queue_entries(void);
int msg_queue_init(void);
void msg_queue_free(void);
void msg_queue_put(struct msg_queue *msg);
void msg_queue_get(void);
void msg_queue_msg_add(int id, char *data, int clientfd);
void msg_queue_msg_del(int *id, char *data, int *clientfd);

void cfg_print(struct config *cfg, int len);
int cfg_file_set_param(char *param, struct config *cfg, int len);
int cfg_file_read(const char *file, struct config *cfg, int len);

int pid_file_open(const char *file, int *fd);
int pid_file_close(const char *file, int fd);

int sock_open(int *fd, int port, int localhost);
int sock_close(int fd);
int sock_client_accept(int listenfd, int *datafd);
int sock_client_read(int fd, char *buf, int *buflen);
int sock_client_write(int fd, char *buf, int buflen);

#endif /* UTILS_H_ */

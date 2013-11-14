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
 * @file ebus-cmd.c
 * @brief ebus command file functions
 * @author roland.jax@liwest.at
 * @version 0.1
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>

#include "log.h"
#include "ebus-decode.h"
#include "ebus-cmd.h"

static struct cycbuf *cyc = NULL;
static int cyclen = 0;

static struct commands *com = NULL;
static int comlen = 0;

void
eb_cmd_uppercase(char *buf)
{
	while (*buf++ != '\0')
		*buf = toupper((unsigned char) *buf);
}


int
eb_cmd_check_type(int id, const char *type)
{
	if (strncasecmp(com[id].type, type, 3) == 0)
		return YES;
	else
		return NO;
}



int
eb_cmd_get_s_type(int id)
{
	return com[id].s_type;
}



void
eb_cmd_set_cyc_buf(int id, const unsigned char *msg, int msglen)
{
	int i;
	
	for (i = 0; i < cyclen; i++)
		if (cyc[i].id == com[id].id)
			break;	

	memcpy(cyc[i].buf, msg, msglen);
	cyc[i].buflen = msglen;
}

void
eb_cmd_get_cyc_buf(int id, unsigned char *msg, int *msglen)
{
	int i;
	
	for (i = 0; i < cyclen; i++)
		if (cyc[i].id == com[id].id)
			break;

	*msglen = cyc[i].buflen;
	memcpy(msg, cyc[i].buf, *msglen);
}



int
eb_cmd_search_com_cyc(const unsigned char *hex, int hexlen)
{
	unsigned char hlp[(CMD_SIZE_S_MSG * 2) + 1];
	int i;

	if (hexlen > (CMD_SIZE_S_MSG * 2)) {
		log_print(L_ERR, "hexlen: %d > hlp: %d ", hexlen, (CMD_SIZE_S_MSG * 2));
		return -2;
	}

	memset(hlp, '\0', sizeof(hlp));
	for (i = 0; i < hexlen; i++)
		sprintf((char *) &hlp[2 * i], "%02X", hex[i]);

	for (i = 0; i < cyclen; i++) {
		if (memcmp(hlp, cyc[i].msg, strlen((const char *) cyc[i].msg)) == 0) {
			log_print(L_NOT, " found: %s type: %d ==> id: %d",
				cyc[i].msg, com[cyc[i].id].s_type, cyc[i].id);
					
			return cyc[i].id;
		}
	}
	
	return -1;
}

int
eb_cmd_search_com_id(const char *type, const char *class, const char *cmd)
{
	int i;

	for (i = 0; i < comlen; i++) {
		if ((strncasecmp(type, com[i].type, strlen(com[i].type)) == 0)
		   && (strncasecmp(class, com[i].class, strlen(com[i].class)) == 0)
		   && strlen(class) == strlen(com[i].class)
		   && (strncasecmp(cmd, com[i].cmd, strlen(com[i].cmd)) == 0)			   
		   && strlen(cmd) == strlen(com[i].cmd)) {

			log_print(L_NOT, " found: %s%s%02X%s type: %d ==> id: %d",
				com[i].s_zz, com[i].s_cmd, com[i].s_len,
				com[i].s_msg, com[i].s_type, i);
			return i;
		}
	
	}
	
	return -1;
}

int
eb_cmd_search_com(char *buf, char *data)
{
	char *type, *class, *cmd, *tok;
	int id;

	type = strtok(buf, " ");
	class = strtok(NULL, " .");
	cmd = strtok(NULL, " .\n\r\t");
	
	if (class != NULL && cmd != NULL) {
	
		if (strncasecmp(type, "get", 3) == 0 ||
		    strncasecmp(type, "set", 3) == 0 ||
		    strncasecmp(type, "cyc", 3) == 0) {
			log_print(L_NOT, "search: %s %s.%s", type, class, cmd);
			
			id = eb_cmd_search_com_id(type, class, cmd);
			if (id < 0)
				return -1;
			
			tok = strtok(NULL, "\n\r");
			
			if (tok == NULL)
				strncpy(data, "-", 1);
			else
				strncpy(data, tok, strlen(tok));			
			
			log_print(L_NOT, "  data: %s", data);
			return id;

		}
	}	
	
	return -1;
}



int
eb_cmd_decode_value(int id, int elem, unsigned char *msg, char *buf)
{
	char *c1, *c2, *c3, *c4;
	char d_pos[CMD_SIZE_D_POS + 1];
	int ret, i, j, p1, p2, p3, p4;
	float f;
	unsigned long l;	

	memset(d_pos, '\0', sizeof(d_pos));
	strncpy(d_pos, com[id].elem[elem].d_pos, strlen(com[id].elem[elem].d_pos));

	c1 = strtok(d_pos, " ,\n");
	c2 = strtok(NULL, " ,\n");
	c3 = strtok(NULL, " ,\n");
	c4 = strtok(NULL, " ,\n");

	p1 = c1 ? atoi(c1) : 0;
	p2 = c2 ? atoi(c2) : 0;
	p3 = c3 ? atoi(c3) : 0;
	p4 = c4 ? atoi(c4) : 0;

	log_print(L_DBG, "id: %d elem: %d p1: %d p2: %d p3: %d p4: %d", id, elem, p1, p2, p3, p4);	

	if (strncasecmp(com[id].elem[elem].d_type, "asc", 3) == 0) {
		sprintf(buf, "%s", &msg[1]);

	} else if (strncasecmp(com[id].elem[elem].d_type, "bcd", 3) == 0) {
		if (p1 > 0) {
			ret = eb_bcd_to_int(msg[p1], &i);

			i *= com[id].elem[elem].d_fac;
			sprintf(buf, "%d", i);
		} else {
			goto on_error;
		}
				
	} else if (strncasecmp(com[id].elem[elem].d_type, "d1b", 3) == 0) {
		if (p1 > 0) {
			ret = eb_d1b_to_int(msg[p1], &i);

			f = i * com[id].elem[elem].d_fac;
			sprintf(buf, "%f", f);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d1c", 3) == 0) {
		if (p1 > 0) {
			ret = eb_d1c_to_float(msg[p1], &f);

			f *= com[id].elem[elem].d_fac;
			sprintf(buf, "%f", f);			
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d2b", 3) == 0) {
		if (p1 > 0 && p2 > 0) {
			if (p1 > p2)
				ret = eb_d2b_to_float(msg[p2], msg[p1], &f);
			else
				ret = eb_d2b_to_float(msg[p1], msg[p2], &f);
				
			f *= com[id].elem[elem].d_fac;
			sprintf(buf, "%f", f);		
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d2c", 3) == 0) {
		if (p1 > 0 && p2 > 0) {
			if (p1 > p2)
				ret = eb_d2c_to_float(msg[p2], msg[p1], &f);
			else	
				ret = eb_d2c_to_float(msg[p1], msg[p2], &f);
			
			f *= com[id].elem[elem].d_fac;
			sprintf(buf, "%f", f);
		} else {
			goto on_error;
		}
					
	} else if (strncasecmp(com[id].elem[elem].d_type, "bda", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			int dd, mm, yy;
			ret = eb_bcd_to_int(msg[p1], &dd);
			ret = eb_bcd_to_int(msg[p2], &mm);
			ret = eb_bcd_to_int(msg[p3], &yy);
			
			ret = eb_dat_to_str(dd, mm, yy, buf);
			if (ret < 0)
				sprintf(buf, "error %s ==> %02x %02x %02x",
				com[id].elem[elem].d_sub, msg[p1], msg[p2], msg[p3]);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "hda", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			ret = eb_dat_to_str(msg[p1], msg[p2], msg[p3], buf);
			if (ret < 0)
				sprintf(buf, "error %s ==> %02x %02x %02x",
				com[id].elem[elem].d_sub, msg[p1], msg[p2], msg[p3]);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "bti", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			int hh, mm, ss;

			ret = eb_bcd_to_int(msg[p1], &hh);
			ret = eb_bcd_to_int(msg[p2], &mm);
			ret = eb_bcd_to_int(msg[p3], &ss);
			
			ret = eb_tim_to_str(hh, mm, ss, buf);
			if (ret < 0)
				sprintf(buf, "error %s ==> %02x %02x %02x",
				com[id].elem[elem].d_sub, msg[p1], msg[p2], msg[p3]);
		} else {
			goto on_error;
		}
					
	} else if (strncasecmp(com[id].elem[elem].d_type, "hti", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			ret = eb_tim_to_str(msg[p1], msg[p2], msg[p3], buf);
			if (ret < 0)
				sprintf(buf, "error %s ==> %02x %02x %02x",
				com[id].elem[elem].d_sub, msg[p1], msg[p2], msg[p3]);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "bdy", 3) == 0) {
		if (p1 > 0)
			ret = eb_day_to_str(msg[p1], buf);
		else
			goto on_error;
			
	} else if (strncasecmp(com[id].elem[elem].d_type, "hdy", 3) == 0) {
		if (p1 > 0) {
			msg[p1] = msg[p1] - 0x01;
			ret = eb_day_to_str(msg[p1], buf);
		} else {
			goto on_error;
		}

	} else if (strncasecmp(com[id].elem[elem].d_type, "hex", 3) == 0) {
		if (p1 > 0) {
			if (p2 > msg[0])
				p2 = msg[0];

			if (p2 == 0 || p2 < p1)
				p2 = p1;
				
			for (i = 0, j = p1 - 1; i <= p2 - p1; i++, j++)
				sprintf(&buf[3 * i], "%02x ", msg[j + 1]);
			
			buf[i * 3 - 1] = '\0';
		} else {
			goto on_error;
		}

	} else if (strncasecmp(com[id].elem[elem].d_type, "ulg", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0) {
			l = msg[p1] + (msg[p2] << 8) + (msg[p3] << 16) + (msg[p4] << 24);
			sprintf(buf, "%lu", l);
		} else {
			goto on_error;
		}	
			
	}

	log_print(L_DBG, "buf: %s", buf);	
		
	return 0;

on_error:
	strcpy(buf, "error decode");
	return -1;

}

int
eb_cmd_decode(int id, char *part, char *data, unsigned char *msg, char *buf)
{
	char *tok;
	char tmp[CMD_DATA_SIZE], hlp[CMD_DATA_SIZE];
	int ret, i, found;

	/* walk through all elements */
	for (i = 0; i < com[id].d_elem; i++) {
		memset(tmp, '\0', sizeof(tmp));
		strncpy(tmp, data, strlen(data));

		/* "-" indicate no sub command, so we print all */
		tok = strtok(tmp, " \n\r\t");	
		if (tok == NULL || strncasecmp(tok, "-", 1) == 0)
			found = YES;
		else
			found = NO;

		/* search sub command */
		while (tok != NULL && found == NO) {
			if (strncasecmp(com[id].elem[i].d_sub, tok,
					strlen(com[id].elem[i].d_sub)) == 0 &&
			    strncasecmp(com[id].elem[i].d_part, part, 2) == 0) {
				
				found = YES;
				break;
			}			
			tok = strtok(NULL, " \n\r\t");
		}

		if (strncasecmp(com[id].elem[i].d_part, part, 2) == 0 &&
		    found == YES) {			
			memset(hlp, '\0', sizeof(hlp));
			
			ret = eb_cmd_decode_value(id, i, msg, hlp);
			if (ret < 0) {
				strncat(buf, "\n", 1);
				return -1;
			}

			if (i > 0)
				strncat(buf, " ", 1);
			
			strncat(buf, hlp, strlen(hlp));			
		}				
	}
		
	return 0;
}

int
eb_cmd_encode_value(int id, int elem, char *data, unsigned char *msg, char *buf)
{
	char *c1, *c2, *c3;
	char d_pos[CMD_SIZE_D_POS + 1];
	unsigned char bcd, d1b, d1c, d2b[2], d2c[2];
	int ret, i, j, p1, p2, p3;
	float f;

	memset(d_pos, '\0', sizeof(d_pos));
	strncpy(d_pos, com[id].elem[elem].d_pos, strlen(com[id].elem[elem].d_pos));

	c1 = strtok(d_pos, " ,\n");
	c2 = strtok(NULL, " ,\n");
	c3 = strtok(NULL, " ,\n");
	
	p1 = c1 ? atoi(c1) : 0;
	p2 = c2 ? atoi(c2) : 0;
	p3 = c3 ? atoi(c3) : 0;
	
	log_print(L_DBG, "id: %d elem: %d p1: %d p2: %d p3: %d data: %s",
						id, elem, p1, p2, p3, data);
					
	if (strncasecmp(com[id].elem[elem].d_type, "asc", 3) == 0) {
		for (i = 0; i < strlen(data); i++)
			sprintf((char *) &msg[i * 2], "%02x", data[i]);

	} else if (strncasecmp(com[id].elem[elem].d_type, "bcd", 3) == 0) {
		if (p1 > 0) {
			i = (int) (atof(data) / com[id].elem[elem].d_fac);

			ret = eb_int_to_bcd(i, &bcd);
			sprintf((char *) msg, "%02x", bcd);
			
		} else {
			goto on_error;
		}
				
	} else if (strncasecmp(com[id].elem[elem].d_type, "d1b", 3) == 0) {
		if (p1 > 0) {
			i = (int) (atof(data) / com[id].elem[elem].d_fac);
			
			ret = eb_int_to_d1b(i, &d1b);
			sprintf((char *) msg, "%02x", d1b);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d1c", 3) == 0) {
		if (p1 > 0) {
			f = (atof(data) / com[id].elem[elem].d_fac);
			
			ret = eb_float_to_d1c(f, &d1c);
			sprintf((char *) msg, "%02x", d1c);
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d2b", 3) == 0) {
		if (p1 > 0 && p2 > 0) {
			f = (atof(data) / com[id].elem[elem].d_fac);
			
			if (p1 > p2)
				ret = eb_float_to_d2b(f, &d2b[1], &d2b[0]);
			else
				ret = eb_float_to_d2b(f, &d2b[0], &d2b[1]);
				
			sprintf((char *) msg, "%02x%02x", d2b[0], d2b[1]);	
		} else {
			goto on_error;
		}
		
	} else if (strncasecmp(com[id].elem[elem].d_type, "d2c", 3) == 0) {
		if (p1 > 0 && p2 > 0) {
			f = (atof(data) / com[id].elem[elem].d_fac);
			
			if (p1 > p2)
				ret = eb_float_to_d2c(f, &d2c[1], &d2c[0]);
			else
				ret = eb_float_to_d2c(f, &d2c[0], &d2c[1]);
				
			sprintf((char *) msg, "%02x%02x", d2c[0], d2c[1]);
		} else {
			goto on_error;
		}
					
	} else if (strncasecmp(com[id].elem[elem].d_type, "hda", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			int dd, mm, yy;

			dd = atoi(strtok(data, " .\n"));
			mm = atoi(strtok(NULL, " .\n"));
			yy = atoi(strtok(NULL, " .\n"));
					
			ret = eb_str_to_dat(dd, mm, yy, msg);
			if (ret < 0)
				sprintf(buf, "error ==> %d.%d.%d", dd, mm, yy);
			
		} else {
			goto on_error;
		}
			
	} else if (strncasecmp(com[id].elem[elem].d_type, "hti", 3) == 0) {
		if (p1 > 0 && p2 > 0 && p3 > 0) {
			int hh, mm, ss;

			hh = atoi(strtok(data, " :\n"));
			mm = atoi(strtok(NULL, " :\n"));
			ss = atoi(strtok(NULL, " :\n"));
					
			ret = eb_str_to_tim(hh, mm, ss, msg);
			if (ret < 0)
				sprintf(buf, "error ==> %d:%d:%d", hh, mm, ss);

		} else {
			goto on_error;
		}

	} else if (strncasecmp(com[id].elem[elem].d_type, "hdy", 3) == 0) {
		if (p1 > 0) {
			int day;

			day = atoi(data);
			sprintf((char *) msg, "%02x", day);

		} else {
			goto on_error;
		}

	} else if (strncasecmp(com[id].elem[elem].d_type, "hex", 3) == 0) {
		for (i = 0, j = 0; data[i]; i++) {

			if (isxdigit(data[i])) {
				msg[j] = tolower(data[i]);
				j++;
			} else if (data[i] != ' ') {
				goto on_error;
			}
		}
		
	}	
		
	return 0;

on_error:
	strcpy(buf, "error encode");
	return -1;
	
}

int
eb_cmd_encode(int id, char *data, unsigned char *msg, char *buf)
{
	char *tok, *toksave;
	unsigned char hlp[CMD_SIZE_S_MSG + 1];
	int ret, i;
		
	tok = strtok_r(data, " \n\r\t", &toksave);

	/* walk through all elements */
	for (i = 0; i < com[id].d_elem; i++) {
		if (tok != NULL) {					
			memset(hlp, '\0', sizeof(hlp));		
			ret = eb_cmd_encode_value(id, i, tok, hlp, buf);
			if (ret < 0) {
				strncat(buf, "\n", 1);
				return -1;
			}

			strncat((char *) msg, (const char *) hlp, strlen((const char *) hlp));
	
		} else {
			return -1;
		}
		
		tok = strtok_r(NULL, " \n\r\t", &toksave);	
	}

	if (strlen(buf) > 0)
		strncat(buf, "\n", 1);
		
	return 0;
}

void
eb_cmd_prepare(int id, char *data, unsigned char *msg, int *msglen, char *buf)
{
	unsigned char tmp[CMD_SIZE_S_MSG + 1];
	char str[CMD_SIZE_S_ZZ + CMD_SIZE_S_CMD + 2 + CMD_SIZE_S_MSG + 1];
	char byte;
	int ret, i, j, k;
	int in[SERIAL_BUFSIZE];	

	/* encode msg */
	memset(tmp, '\0', sizeof(tmp));
	if (strncasecmp(com[id].type, "set", 3) == 0)
		eb_cmd_encode(id, data, tmp, buf);

	memset(str, '\0', sizeof(str));
	sprintf(str, "%s%s%02X%s%s",
		com[id].s_zz, com[id].s_cmd, com[id].s_len, com[id].s_msg, tmp);

	memset(in, '\0', sizeof(in));
	i = 0;
	j = 0;
	
	while (str[j] != '\0') {
		byte = str[j];
		if (i < sizeof(in)) {

			ret = eb_htoi(&byte);
			if (ret != -1) {
				in[i] = ret;
				i++;
			}
		}
		j++;
	}

	memset(msg, '\0', sizeof(msg));
	for (j = 0, k = 0; j < i; j += 2, k++)
		msg[k] = (unsigned char) (in[j]*16 + in[j+1]);

	*msglen = k;
	
}



void
eb_cmd_print(const char *type, int all, int detail)
{
	int i, j;

	for (i = 0; i < comlen; i++) {

		if (strncasecmp(com[i].type, type, 1) == 0 || all) {

			log_print(L_INF, "[%03d] %s : %5s.%-32s\t(type: %d)" \
					 " %s%s%-10s (len: %d) [%d] ==> %s",
				com[i].id,
				com[i].type,
				com[i].class,
				com[i].cmd,
				com[i].s_type,
				com[i].s_zz,
				com[i].s_cmd,
				com[i].s_msg,
				com[i].s_len,
				com[i].d_elem,		
				com[i].com			
				);

			if (detail) {
				for (j = 0; j < com[i].d_elem; j++) {
					log_print(L_INF, "\t\t  %-20s %-2s " \
					"pos: %-10s\t%s [%5.2f] [%s] \t%s\t%s",
						com[i].elem[j].d_sub,
						com[i].elem[j].d_part,
						com[i].elem[j].d_pos,
						com[i].elem[j].d_type,
						com[i].elem[j].d_fac,
						com[i].elem[j].d_unit,
						com[i].elem[j].d_valid,
						com[i].elem[j].d_com
				
						);
				}
				log_print(L_INF, "");
			}

			
		}
	}
}



int
eb_cmd_fill(const char *tok)
{
	int i;
	
	com = (struct commands *) realloc(com, (comlen + 1) * sizeof(struct commands));
	err_ret_if(com == NULL, -1);

	memset(com + comlen, '\0', sizeof(struct commands));
	
	/* id */
	com[comlen].id = comlen;

	/* type */
	strncpy(com[comlen].type, tok, strlen(tok));
	
	/* class */
	tok = strtok(NULL, ";");
	strncpy(com[comlen].class, tok, strlen(tok));

	
	/* cmd */
	tok = strtok(NULL, ";");
	strncpy(com[comlen].cmd, tok, strlen(tok));

	/* com */
	tok = strtok(NULL, ";");
	strncpy(com[comlen].com, tok, strlen(tok));
	
	/* s_type */
	tok = strtok(NULL, ";");
	com[comlen].s_type = atoi(tok);	
	
	/* s_zz */
	tok = strtok(NULL, ";");

	strncpy(com[comlen].s_zz, tok, strlen(tok));
	eb_cmd_uppercase(com[comlen].s_zz);
	
	/* s_cmd */
	tok = strtok(NULL, ";");
	strncpy(com[comlen].s_cmd, tok, strlen(tok));
	eb_cmd_uppercase(com[comlen].s_cmd);
	
	/* s_len */
	tok = strtok(NULL, ";");
	com[comlen].s_len = atoi(tok);

	/* s_msg */
	tok = strtok(NULL, ";");
	if (strncasecmp(tok, "-", 1) != 0) {
		strncpy(com[comlen].s_msg, tok, strlen(tok));
		eb_cmd_uppercase(com[comlen].s_msg);
	}
	
	/* d_elem */
	tok = strtok(NULL, ";");
	com[comlen].d_elem = atoi(tok);
	
	com[comlen].elem = (struct element *) malloc(com[comlen].d_elem * sizeof(struct element));
	err_ret_if(com[comlen].elem == NULL, -1);

	memset(com[comlen].elem, '\0', com[comlen].d_elem * sizeof(struct element));	

	for (i = 0; i < com[comlen].d_elem; i++) {
		
		/* d_sub */
		tok = strtok(NULL, ";");
		if (strncasecmp(tok, "-", 1) != 0)
			strncpy(com[comlen].elem[i].d_sub, tok, strlen(tok));

		/* d_part */
		tok = strtok(NULL, ";");	
		strncpy(com[comlen].elem[i].d_part, tok, strlen(tok));
		
		/* d_pos */
		tok = strtok(NULL, ";");	
		strncpy(com[comlen].elem[i].d_pos, tok, strlen(tok));
		
		/* d_type */
		tok = strtok(NULL, ";");
		strncpy(com[comlen].elem[i].d_type, tok, strlen(tok));				
		
		/* d_fac */
		tok = strtok(NULL, ";");	
		com[comlen].elem[i].d_fac = atof(tok);
		
		/* d_unit */
		tok = strtok(NULL, ";");	
		strncpy(com[comlen].elem[i].d_unit, tok, strlen(tok));

		/* d_valid */
		tok = strtok(NULL, ";");	
		strncpy(com[comlen].elem[i].d_valid, tok, strlen(tok));

		/* d_com */
		tok = strtok(NULL, ";");
		strncpy(com[comlen].elem[i].d_com, tok, strlen(tok));
		com[comlen].elem[i].d_com[strcspn(com[comlen].elem[i].d_com, "\n")] = '\0';
		
	}
	
	if (strncasecmp(com[comlen].type, "cyc", 3) == 0) {

		cyc = (struct cycbuf *) realloc(cyc, (cyclen + 1) * sizeof(struct cycbuf));
		err_ret_if(cyc == NULL, -1);

		memset(cyc + cyclen, '\0', sizeof(struct cycbuf));		

		/* id */
		cyc[cyclen].id = com[comlen].id;

		/* msg */
		sprintf((char *) cyc[cyclen].msg, "%s%s%02X%s",
					com[comlen].s_zz, com[comlen].s_cmd,
					com[comlen].s_len, com[comlen].s_msg);
			
		cyclen++;

	}

	comlen++;

	return 0;	
}

int
eb_cmd_num_c(const char *str, const char c)
{
	const char *p = str;
	int i = 0;

	do {
		if (*p == c)
			i++;
	} while (*(p++));

	return i;
}

int
eb_cmd_file_read(const char *file)
{
	int ret;
	char line[CMD_LINELEN];
	char *tok;
	FILE *fp = NULL;

	log_print(L_NOT, "%s", file);

	/* open config file */
	fp = fopen(file, "r");
	err_ret_if(fp == NULL, -1);			

	/* read each line and fill cmd array */
	while (fgets(line, CMD_LINELEN, fp) != NULL) {
		tok = strtok(line, ";\n");
			
		if (tok != NULL && tok[0] != '#' ) {

			ret = eb_cmd_fill(tok);
			if (ret < 0)
				return -2;
				
		}

	}
	
	/* close config file */
	ret = fclose(fp);
	err_ret_if(ret == EOF, -1);

	log_print(L_NOT, "%s success", file);

	return 0;
}

int
eb_cmd_dir_read(const char *cfgdir, const char *extension)
{
	struct dirent **dir;
	int ret, i, j, files, extlen;
	char file[CMD_FILELEN], extprep[11];
	char *ext;

	extlen = strlen(extension) + 1;

	memset(extprep, '\0', sizeof(extprep));
	extprep[0] = '.';
	strncpy(&extprep[1], extension, strlen(extension));

	files = scandir(cfgdir, &dir, 0, alphasort);
	if (files < 0) {
		log_print(L_WAR, "configuration directory %s not found.", cfgdir);
		return 1;
	}
	
	i = 0;
	j = 0;
	while (i < files) {
		ext = strrchr(dir[i]->d_name, '.');
			if (ext != NULL) {
				if (strlen(ext) == extlen
				    && dir[i]->d_type == DT_REG
				    && strncasecmp(ext, extprep, extlen) == 0 ) {
					memset(file, '\0', sizeof(file));
					sprintf(file, "%s/%s", cfgdir, dir[i]->d_name);
									
					ret = eb_cmd_file_read(file);
					if (ret < 0)
						return -1;

					j++;
				}
			}
			
		free(dir[i]);
		i++;
	}
	
	free(dir);

	if (j == 0) {
		log_print(L_WAR, "no command files found ==> decode disabled.");
		return 2;
	}
	
	return 0;
}

void
eb_cmd_dir_free(void)
{
	int i;

	if (comlen > 0) {
		for (i = 0; i < comlen; i++)
			free(&com[i].elem[0]);
	
		free(com);
	}

	if (cyclen > 0)
		free(cyc);
	
}


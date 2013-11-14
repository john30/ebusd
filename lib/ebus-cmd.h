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
 * @file ebus-cmd.h
 * @brief ebus command file functions
 * @author roland.jax@liwest.at
 * @version 0.1
 */

#ifndef EBUS_CMD_H_
#define EBUS_CMD_H_



#include <sys/time.h>

#include "ebus-common.h"



#define CMD_LINELEN        512
#define CMD_FILELEN        1024

#define CMD_SIZE_TYPE      3
#define CMD_SIZE_CLASS     20
#define CMD_SIZE_CMD       30
#define CMD_SIZE_COM       256
#define CMD_SIZE_S_ZZ      2
#define CMD_SIZE_S_CMD     4
#define CMD_SIZE_S_MSG     32

#define CMD_SIZE_D_SUB     20
#define CMD_SIZE_D_PART    2
#define CMD_SIZE_D_POS     10
#define CMD_SIZE_D_TYPE    3
#define CMD_SIZE_D_UNIT    6
#define CMD_SIZE_D_VALID   30
#define CMD_SIZE_D_COM     256

#define CMD_PART_MD        "MD"
#define CMD_PART_SA        "SA"
#define CMD_PART_SD        "SD"
#define CMD_PART_MA        "MA"



/**
 * @brief cycbuf structure
 */
struct cycbuf {
	int id; /**< command id from command buffer */
	unsigned char msg[CMD_SIZE_S_MSG + 1]; /**< zz + cmd + len + msg */
	unsigned char buf[CMD_DATA_SIZE + 1]; /**< whole message - unescaped */
	int buflen; /**< len of saved message */	
};

/**
 * @brief commands structure
 */
struct commands {
	int id; /**< command id */
	char type[CMD_SIZE_TYPE + 1]; /**< type of message */
	char class[CMD_SIZE_CLASS + 1]; /**< ci */
	char cmd[CMD_SIZE_CMD + 1]; /**< hydraulic */
	char com[CMD_SIZE_COM + 1]; /**< just a comment */	
	int s_type; /**< message type */
	char s_zz[CMD_SIZE_S_ZZ + 1]; /**< zz */ 
	char s_cmd[CMD_SIZE_S_CMD + 1]; /**< pb sb */
	int s_len; /**< number of send bytes */
	char s_msg[CMD_SIZE_S_MSG + 1]; /**< max 15 data bytes */
	int d_elem; /**< number of elements */
	struct element *elem; /**< pointer of array with elements */
};

/**
 * @brief element structure
 */
struct element {
	char d_sub[CMD_SIZE_D_SUB + 1]; /**< pin1 */
	char d_part[CMD_SIZE_D_PART + 1]; /**< part of message */
	char d_pos[CMD_SIZE_D_POS + 1]; /**< data position at bytes */
	char d_type[CMD_SIZE_D_TYPE + 1]; /**< data type */
	float d_fac; /**< facter */
	char d_unit[CMD_SIZE_D_UNIT + 1]; /**< unit of data like °C,...) */
	char d_valid[CMD_SIZE_D_VALID + 1]; /**< valid data */
	char d_com[CMD_SIZE_D_COM + 1]; /**< just a comment */
};



/**
 * @brief convert string to uppercase
 * @param [out] *buf pointer to buffer array
 */
void eb_cmd_uppercase(char *buf);



/**
 * @brief compare given type with type of given command id
 * @param [in] id is index in command array
 * @param [in] type for lookup
 * @return YES or NO
 */
int eb_cmd_check_type(int id, const char *type);



/**
 * @brief returns s_type of given id
 * @param [in] id is index in command array
 * @return s_type of ebus command
 */
int eb_cmd_get_s_type(int id);



/**
 * @brief set buf from cyc array
 * @param [in] id is index in command array
 * @param [in] *msg pointer to message array
 * @param [in] msglen pointer to message length
 * @return none
 */
void eb_cmd_set_cyc_buf(int id, const unsigned char *msg, int msglen);


/**
 * @brief get buf from cyc array 
 * @param [in] id is index in command array
 * @param [out] *msg pointer to message array
 * @param [out] *msglen pointer to message length
 * @return none
 */
void eb_cmd_get_cyc_buf(int id, unsigned char *msg, int *msglen);



/**
 * @brief search given strings in cycbuf array
 * @param [in] *hex pointer to hex string
 * @return 0-x id of found ebus command in array
 *         -1 command not found | -2 hexlen too long
 */
int eb_cmd_search_com_cyc(const unsigned char *hex, int hexlen);

/**
 * @brief search given strings in command array
 * @param [in] *type pointer to message type 
 * @param [in] *class pointer to a ebus class
 * @param [in] *cmd pointer to a ebus command
 * @return 0-x id of found ebus command in array | -1 command not found
 */
int eb_cmd_search_com_id(const char *type, const char *class, const char *cmd);

/**
 * @brief search given strings in command array
 * @param [in] *buf pointer to a byte array
 * @param [out] *data pointer to data array for passing to decode/encode
 * @return 0-x id of found ebus command in array | -1 command not found
 */
int eb_cmd_search_com(char *buf, char *data);



/**
 * @brief decode given element
 * @param [in] id is index in command array
 * @param [in] elem is index of data position
 * @param [out] *msg pointer to message array
 * @param [out] *buf pointer to decoded answer
 * @return 0 ok | -1 error at decode
 */
int eb_cmd_decode_value(int id, int elem, unsigned char *msg, char *buf);

/**
 * @brief decode msg
 * @param [in] id is index in command array
 * @param [in] *part point to part type for decode
 * @param [in] *data pointer to data bytes for decode
 * @param [out] *msg pointer to message array
 * @param [out] *buf pointer to decoded answer
 * @return 0 ok | -1 error at decode
 */
int eb_cmd_decode(int id, char *part, char *data, unsigned char *msg, char *buf);

/**
 * @brief decode given element
 * @param [in] id is index in command array
 * @param [in] elem is index of data position
 * @param [in] *data pointer to data bytes for encode
 * @param [out] *msg pointer to message array
 * @param [out] *buf pointer to decoded answer
 * @return 0 ok | -1 error at decode
 */
int eb_cmd_encode_value(int id, int elem, char *data, unsigned char *msg, char *buf);

/**
 * @brief encode msg
 * @param [in] id is index in command array
 * @param [in] *data pointer to data bytes for encode
 * @param [out] *msg pointer to message array
 * @param [out] *buf pointer to decoded answer
 * @return 0 ok | -1 error at encode
 */
int eb_cmd_encode(int id, char *data, unsigned char *msg, char *buf);

/**
 * @brief prepare message string for given ebus cmd from array
 * @param [in] id is index in command array
 * @param [in] *data pointer to data bytes for decode/encode
 * @param [out] *msg pointer to message array
 * @param [out] *msglen pointer to message length
 * @param [out] *buf pointer to decoded answer 
 * @return none
 */
void eb_cmd_prepare(int id, char *data, unsigned char *msg, int *msglen, char *buf);



/**
 * @brief print readed ebus commands
 * @param [in] *type (get/set/cyc)
 * @param [in] all print all
 * @param [in] detail show details of command
 * @return none
 */
void eb_cmd_print(const char *type, int all, int detail);



/**
 * @brief fill command structure
 * @param [in] *tok pointer to given token
 * @return 0 ok | -1 error
 */
int eb_cmd_fill(const char *tok);

/**
 * @brief prevent buffer overflow
 * 
 * number of tokens must be >= number of columns - 1 of file
 * 
 * @param [in] *buf pointer to a byte array
 * @param [in] delimeter
 * @return number of found delimeters
 */ 
int eb_cmd_num_c(const char *buf, const char c);

/**
 * @brief set cfgdir address
 * @param [in] *file pointer to configuration file with *.csv
 * @return 0 ok | -1 error | -2  read token error
 */
int eb_cmd_file_read(const char *file);

/**
 * @brief get all files with given extension from given configuration directory
 * @param [in] *cfgdir pointer to configuration directory
 * @param [in] *suffix pointer to given extension
 * @return 0 ok | -1 error | 1 read file error | 2 no command files found
 */
int eb_cmd_dir_read(const char *cfgdir, const char *extension);

/**
 * @brief free mem for ebus commands
 * @return none
 */
void eb_cmd_dir_free(void);



#endif /* EBUS_CMD_H_ */

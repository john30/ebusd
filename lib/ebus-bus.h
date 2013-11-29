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
 * @file ebus-bus.h
 * @brief ebus communication functions
 * @author roland.jax@liwest.at
 * @version 0.1
 */

#ifndef EBUS_BUS_H_
#define EBUS_BUS_H_



#include <sys/time.h>

#include "ebus-common.h"



#define TMP_BUFSIZE      1024



/**
 * @brief sending data structure
 */
struct send_data {
	unsigned char crc; /**< crc of escaped message */
	unsigned char msg[SERIAL_BUFSIZE + 1]; /**< original message */
	int len; /**< length of original message */
	unsigned char msg_esc[SERIAL_BUFSIZE + 1]; /**< esacaped message */
	int len_esc; /**< length of  esacaped message */
};

/**
 * @brief receiving data structure
 */
struct recv_data {
	unsigned char crc_recv; /**< received crc */
	unsigned char crc_calc;	/**< calculated crc */
	unsigned char msg[SERIAL_BUFSIZE + 1]; /**< unescaped message */
	int len; /**< length of unescaped message */
	unsigned char msg_esc[SERIAL_BUFSIZE + 1]; /**< received message */
	int len_esc; /**< length of received message */
};





/**
 * @brief set nodevicecheck
 * @param [in] YES | NO
 * @return none
 */
void eb_set_nodevicecheck(int check);

/**
 * @brief set rawdump
 * @param [in] YES | NO
 * @return none
 */
void eb_set_rawdump(int dump);

/**
 * @brief set showraw
 * @param [in] YES | NO
 * @return none
 */
void eb_set_showraw(int show);

/**
 * @brief set own ebus address
 * @param [in] src ebus address
 * @return none
 */
void eb_set_qq(unsigned char src);

/**
 * @brief set number of retry to get bus
 * @param [in] retry
 * @return none
 */
void eb_set_get_retry(int retry);

/**
 * @brief set number of skipped SYN if an error occurred while getting the bus
 * @param [in] skip is only the start value (skip = skip_ack + retry;)
 * @return none
 */
void eb_set_skip_ack(int skip);

/**
 * @brief set max wait time between send and receive of own address (QQ)
 * @param [in] usec wait time in usec
 * @return none
 */
void eb_set_max_wait(long usec);

/**
 * @brief set number of retry to send message
 * @param [in] retry
 * @return none
 */
void eb_set_send_retry(int retry);

/**
 * @brief set number of printed hex bytes
 * @param [in] size
 * @return none
 */
void eb_set_print_size(int size);



/**
 * @brief calculate delte time of given time stamps
 * @param [in] *tact newer time stamp
 * @param [in] *tlast older time stamp
 * @param [out] *tdiff calculated time difference
 * @return 0 positive | -1 negative ??? lol
 */
int eb_diff_time(const struct timeval *tact, const struct timeval *tlast, struct timeval *tdiff);



/**
 * @brief Open a file in write mode.
 * @param [in] *file device
 * @return 0 ok | -1 error
 */
int eb_raw_file_open(const char *file);

/**
 * @brief close a file.
 * @return 0 ok | -1 error
 */
int eb_raw_file_close(void);

/**
 * @brief write bytes into file
 * @param [in] *buf pointer to a byte array
 * @param [in] buflen length of byte array
 * @return 0 ok | -1 error
 */
int eb_raw_file_write(const unsigned char *buf,  int buflen);



/**
 * @brief Check if serial FD is vaild.
 * @return 0 ok | -1 error
 */
int eb_serial_valid();

/**
 * @brief Open serial device in raw mode. 1 Byte is then minimum input length.
 * @param [in] *dev serial device
 * @param [out] *fd file descriptor from opened serial device
 * @return 0 ok | -1 error
 */
int eb_serial_open(const char *dev, int *fd);

/**
 * @brief close serial device and set settings to default.
 * @return 0 ok | -1 error | -2 bad file descriptor
 */
int eb_serial_close(void);

/**
 * @brief send bytes to serial device
 * @param [in] *buf pointer to a byte array
 * @param [in] buflen length of byte array
 * @return 0 ok | -1 error | -2 bad file descriptor
 */
int eb_serial_send(const unsigned char *buf, int buflen);

/**
 * @brief receive bytes from serial device
 * @param [out] *buf pointer to a byte array received bytes
 * @param [out] *buflen length of received bytes
 * @return 0 ok | -1 error | -2 bad file descriptor | -3 received data > BUFFER
 */
int eb_serial_recv(unsigned char *buf, int *buflen);



/**
 * @brief print received results in a specific format
 * @return none
 */
void eb_print_result(void);

/**
 * @brief print received data with hex format
 * @param [in] *buf pointer to a byte array of received bytes
 * @param [in] buflen length of received bytes
 * @return none
 */
void eb_print_hex(const unsigned char *buf, int buflen);



/**
 * @brief get received data
 * @param [out] *buf pointer to a byte array of received bytes
 * @param [out] *buflen length of received bytes
 * @return none
 */
void eb_recv_data_get(unsigned char *buf, int *buflen);

/**
 * @brief prepare received data
 * @param [in] *buf pointer to a byte array of received bytes
 * @param [in] buflen length of received bytes
 * @return none
 */
void eb_recv_data_prepare(const unsigned char *buf, int buflen);

/**
 * @brief receive bytes from serial device
 * @param [out] *buf pointer to a byte array of received bytes
 * @param [out] buflen length of received bytes
 * @return 0 ok | -1 error | -2 syn no answer from slave 
 * | -3 SYN received after answer from slave
 */  
int eb_recv_data(unsigned char *buf, int *buflen);



/**
 * @brief receive bytes from serial device until SYN byte was received
 * @param [in] *skip number skipped SYN bytes
 * @return 0 ok | -1 error
 */ 
int eb_bus_wait_syn(int *skip);

/**
 * @brief try to get bus for sending
 * \li wait SYN byte, send our ebus address and wait for minimun of ~4200 usec
 * \li read at least one byte and compare it with sent byte.
 * @return 0 ok | -1 error | 1 max retry was reached
 */  
int eb_bus_wait(void);

/**
 * @brief free bus after syn was sent
 * @return 0 ok | -1 error
 */  
int eb_bus_free(void);



 /**
 * @brief receive ACK byte from serial device
 * @param [out] *buf pointer to a byte array of received bytes
 * @param [out] buflen length of received bytes
 * @return 0 ok | 1 negative ACK | -1 error | -2 send and recv msg are different 
 * | -3 syn received (no answer from slave) | -4 we should never reach this
 */  
int eb_send_data_get_ack(unsigned char *buf, int *buflen);

/**
 * @brief prepare send data
 * @param [in] *buf pointer to a byte array of send bytes
 * @param [in] buflen length of send bytes
 * @return none
 */
void eb_send_data_prepare(const unsigned char *buf, int buflen);

/**
 * @brief handle sending ebus data
 *
 * \li Given input data will be prepared (escape, crc) and send to ebus.
 * \li Answer will be received, prepared (unescape, crc) and return as result.
 * 
 * @param [in] *buf pointer to a byte array
 * @param [in] buflen length of byte array
 * @param [in] type is the type of message to send
 * @param [out] *bus pointer to answer array
 * @param [out] *buslen point to answer length
 * @return 0 ok | 1 neg. ACK from Slave | -1 error
 */ 
int eb_send_data(const unsigned char *buf, int buflen, int type, unsigned char *bus, int *buslen);



/**
 * @brief handle send ebus cmd 
 * @param [in] id is index in command array
 * @param [in] *data pointer to data bytes for decode/encode
 * @param [out] *buf pointer to answer array
 * @param [out] *buflen point to answer length
 * @return none
 */
void eb_execute(int id, char *data, char *buf, int *buflen);



/**
 * @brief handle one collected message
 * @param [in] *buf pointer to a byte array
 * @param [in] buflen length of byte array
 * @return 0-x msg number of collected msg | -1 msg not found | -2 error
 */
int eb_cyc_data_process(const unsigned char *buf, int buflen);

/**
 * @brief handle reading cycle ebus data
 * @return 0-x length of collected msg | -1 error
 */
int eb_cyc_data_recv();



#endif /* EBUS_BUS_H_ */

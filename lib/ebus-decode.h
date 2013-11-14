/*
 * Copyright (C) Roland Jax 2012-2013 <roland.jax@liwest.at>
 * crc calculations from http://www.mikrocontroller.net/topic/75698
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
 * @file ebus-decode.h
 * @brief ebus decode functions
 * @author roland.jax@liwest.at
 * @version 0.1
 */

/*
 * name     type             description              resolution   substitue
 * BCD      CHAR                 0    ... +   99      1              FFh
 * DATA1b   SIGNED CHAR      - 127    ... +  127      1              80h
 * DATA1c   CHAR                 0    ... +  100      0,5            FFh
 * DATA2b   SIGNED INTEGER   - 127,99 ... +  127,99   1/256        8000h
 * DATA2c   SIGNED INTEGER   -2047,9  ... + 2047,9    1/16         8000h
 */

#ifndef EBUS_DECODE_H_
#define EBUS_DECODE_H_


#include "ebus-common.h"



/**
 * @brief calculate integer value of given hex byte
 * @param [in] *buf hex byte
 * @return integer value | -1 if given byte is no hex value
 */ 
int eb_htoi(const char *buf);



/**
 * @brief print received results in a specific format
 * @param [out] *buf pointer to a byte array
 * @param [out] *buflen length of byte array
 * @return none
 */
void eb_esc(unsigned char *buf, int *buflen);

/**
 * @brief print received results in a specific format
 * @param [out] *buf pointer to a byte array
 * @param [out] *buflen length of byte array
 * @return none
 */
void eb_unesc(unsigned char *buf, int *buflen);



/**
 * @brief convert hex bytes to day
 * @param [in] day hex byte
 * @param [out] *tgt pointer to day string xxx
 * @return 0 OK | -1 values out of range
 */ 
int eb_day_to_str(unsigned char day, char *tgt);



/**
 * @brief convert hex bytes to date
 * @param [in] dd day hex byte
 * @param [in] mm month hex byte
 * @param [in] yy year hex byte 
 * @param [out] *tgt pointer to date string dd.mm.yyyy
 * @return 0 OK | -1 values out of range
 */ 
int eb_dat_to_str(unsigned char dd, unsigned char mm, unsigned char yy, char *tgt);

/**
 * @brief convert char date string to hex
 * @param [in] dd day int value
 * @param [in] mm month int value
 * @param [in] yy year int value 
 * @param [out] *tgt pointer to hex 
 * @return 0 OK | -1 values out of range
 */ 
int eb_str_to_dat(int dd, int mm, int yy, unsigned char *tgt);



/**
 * @brief convert hex bytes to date
 * @param [in] hh hour hex byte
 * @param [in] mm minute hex byte
 * @param [in] ss second hex byte 
 * @param [out] *tgt pointer to date string hh:mm:ss
 * @return 0 OK | -1 values out of range
 */ 
int eb_tim_to_str(unsigned char hh, unsigned char mm, unsigned char ss, char *tgt);

/**
 * @brief convert char time string to hex
 * @param [in] hh hour int value
 * @param [in] mm minute int value
 * @param [in] ss second int value 
 * @param [out] *tgt pointer to hex 
 * @return 0 OK | -1 values out of range
 */ 
int eb_str_to_tim(int hh, int mm, int ss, unsigned char *tgt);



/**
 * @brief convert bcd hex byte to int
 * @param [in] src bcd hex byte
 * @param [out] *tgt pointer to int value
 * @return 0 substitute value | 1 positive value
 */ 
int eb_bcd_to_int(unsigned char src, int *tgt);

/**
 * @brief convert int to bcd hex byte
 * @param [in] src int value
 * @param [out] *tgt pointer to hex byte
 * @return 0 substitute value | 1 positive value
 */ 
int eb_int_to_bcd(int src, unsigned char *tgt);



/**
 * @brief convert data1b hex byte to int
 * @param [in] src data1b hex byte
 * @param [out] *tgt pointer to int value
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_d1b_to_int(unsigned char src, int *tgt);

/**
 * @brief convert int to data1b hex byte
 * @param [in] src int value
 * @param [out] *tgt pointer to data1b hex byte
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_int_to_d1b(int src, unsigned char *tgt);



/**
 * @brief convert data1c hex byte to float
 * @param [in] src data1c hex byte
 * @param [out] *tgt pointer to float value
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_d1c_to_float(unsigned char src, float *tgt);

/**
 * @brief convert float to data1c hex byte
 * @param [in] src float value
 * @param [out] *tgt pointer to data1c hex byte
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_float_to_d1c(float src, unsigned char *tgt);



/**
 * @brief convert data2b hex bytes to float
 * @param [in] src_lsb least significant data2b hex byte
 * @param [in] src_msb most significant data2b hex byte
 * @param [out] *tgt pointer to float value
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_d2b_to_float(unsigned char src_lsb, unsigned char src_msb, float *tgt);

/**
 * @brief convert float to data2b hex bytes
 * @param [in] src float value
 * @param [out] *tgt_lsb pointer to least significant data2b hex byte
 * @param [out] *tgt_msb pointer to most significant data2b hex byte
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_float_to_d2b(float src, unsigned char *tgt_lsb, unsigned char *tgt_msb);



/**
 * @brief convert data2c hex bytes to float
 * @param [in] src_lsb least significant data2c hex byte
 * @param [in] src_msb most significant data2c hex byte
 * @param [out] *tgt pointer to float value
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_d2c_to_float(unsigned char src_lsb, unsigned char src_msb, float *tgt);

/**
 * @brief convert float to data2c hex bytes
 * @param [in] src float value
 * @param [out] *tgt_lsb pointer to least significant data2c hex byte
 * @param [out] *tgt_msb pointer to most significant data2c hex byte
 * @return 0 substitute value | 1 positive value | -1 negative value
 */ 
int eb_float_to_d2c(float src, unsigned char *tgt_lsb, unsigned char *tgt_msb);



/**
 * @brief calculate crc of hex byte
 * @param [in] byte byte to calculate
 * @param [in] init_crc start value for calculation
 * @return new calculated crc byte from byte and init crc byte
 */
unsigned char eb_calc_crc_byte(unsigned char byte, unsigned char init_crc);

/**
 * @brief calculate crc of given hex array
 *
 * \li crc calculation "CRC-8-WCDMA" with Polynom "x^8+x^7+x^4+x^3+x+1"
 * \li crc calculations by http://www.mikrocontroller.net/topic/75698
 *
 * @param [in] *bytes pointer to a byte array
 * @param [in] size length of given bytes
 * @return calculated crc byte
 */
unsigned char eb_calc_crc(const unsigned char *bytes, int size);



#endif /* EBUS_DECODE_H_ */

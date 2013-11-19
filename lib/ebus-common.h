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
 * @file ebus-common.h
 * @brief ebus common defines
 * @author roland.jax@liwest.at
 * @version 0.1
 */

#ifndef EBUS_COMMON_H_
#define EBUS_COMMON_H_



#define EBUS_SYN                   0xAA
#define EBUS_SYN_ESC_A9            0xA9
#define EBUS_SYN_ESC_01            0x01
#define EBUS_SYN_ESC_00            0x00

#define EBUS_ACK                   0x00
#define EBUS_NAK                   0xFF

#define EBUS_QQ                    0xFF

#define EBUS_GET_RETRY             3
#define EBUS_GET_RETRY_MAX         10
#define EBUS_SKIP_ACK              1
#define EBUS_MAX_WAIT              4000
#define EBUS_SEND_RETRY            2
#define EBUS_SEND_RETRY_MAX        10
#define EBUS_PRINT_SIZE            30

#define EBUS_MSG_BROADCAST_TXT     "BR"
#define EBUS_MSG_BROADCAST         1
#define EBUS_MSG_MASTER_MASTER_TXT "MM"
#define EBUS_MSG_MASTER_MASTER     2
#define EBUS_MSG_MASTER_SLAVE_TXT  "MS"
#define EBUS_MSG_MASTER_SLAVE      3

#define SERIAL_DEVICE              "/dev/ttyUSB0"
#define SERIAL_BAUDRATE            B2400
#define SERIAL_BUFSIZE             100

#define CMD_DATA_SIZE              50 /* 5+16+3+16+2 = 42 || 256 */

enum enum_bool {UNSET = -1, NO, YES};



#endif /* EBUS_COMMON_H_ */

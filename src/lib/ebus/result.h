/*
 * Copyright (C) John Baier 2014 <ebusd@johnm.de>
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

#ifndef LIBEBUS_RESULT_H_
#define LIBEBUS_RESULT_H_

static const int RESULT_OK = 0;

static const int RESULT_BUS_ACQUIRED = 1;      // bus successfully acquired
static const int RESULT_DATA = 2;              // some data received
static const int RESULT_SYN = 3;               // regular SYN after message received
static const int RESULT_BUS_LOCKED = 4;        // bus is locked for access
static const int RESULT_BUS_PRIOR_RETRY = 5;   // retry to access bus
static const int RESULT_IN_ESC = 6;            // start of escape sequence received

static const int RESULT_ERR_SEND = -1;         // send error
static const int RESULT_ERR_EXTRA_DATA = -2;   // received bytes > sent bytes
static const int RESULT_ERR_NAK = -3;          // NAK received
static const int RESULT_ERR_CRC = -4;          // CRC error
static const int RESULT_ERR_ACK = -5;          // ACK error
static const int RESULT_ERR_TIMEOUT = -6;      // read timeout
static const int RESULT_ERR_SYN = -7;          // SYN received
static const int RESULT_ERR_BUS_LOST = -8;     // arbitration lost
static const int RESULT_ERR_ESC = -9;          // invalid escape sequence received
static const int RESULT_ERR_INVALID_ARG = -10; // invalid argument
static const int RESULT_ERR_DEVICE = -11;      // generic device error (usually fatal)
static const int RESULT_ERR_EOF = -12;         // end of input reached
static const int RESULT_ERR_FILENOTFOUND = -13;// file not found or not readable
static const int RESULT_ERR_DUPLICATE = -14;   // duplicate entry

/** type for result code. */
typedef int result_t;

/**
 * @brief Return the string corresponding to the result code.
 * @param resultCode the result code (see RESULT_ constants).
 * @return the string corresponding to the result code.
 */
const char* getResultCode(result_t resultCode);

#endif // LIBEBUS_RESULT_H_

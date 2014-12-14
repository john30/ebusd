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

static const int RESULT_OK = 0;                 // success

static const int RESULT_IN_ESC = 1;             // start of escape sequence received
static const int RESULT_SYN = 2;                // regular SYN after message received
static const int RESULT_EMPTY = 3;              // empty result

static const int RESULT_ERR_GENERIC_IO = -1;    // generic I/O error (usually fatal)
static const int RESULT_ERR_DEVICE = -2;        // generic device error (usually fatal)
static const int RESULT_ERR_SEND = -3;          // send error
static const int RESULT_ERR_ESC = -4;           // invalid escape sequence
static const int RESULT_ERR_TIMEOUT = -5;       // read timeout

static const int RESULT_ERR_NOTFOUND = -6;      // file/element not found or not readable
static const int RESULT_ERR_EOF = -7;           // end of input reached
static const int RESULT_ERR_INVALID_ARG = -8;   // invalid argument
static const int RESULT_ERR_INVALID_NUM = -9;   // invalid numeric argument
static const int RESULT_ERR_INVALID_POS = -10;  // invalid position
static const int RESULT_ERR_OUT_OF_RANGE = -11; // argument value out of valid range
static const int RESULT_ERR_INVALID_PART = -12; // invalid part type value
static const int RESULT_ERR_MISSING_TYPE = -13; // missing data type
static const int RESULT_ERR_INVALID_LIST = -14; // invalid value list
static const int RESULT_ERR_DUPLICATE = -15;    // duplicate entry

static const int RESULT_ERR_BUS_LOST = -16;     // arbitration lost
static const int RESULT_ERR_CRC = -17;          // CRC error
static const int RESULT_ERR_ACK = -18;          // ACK error
static const int RESULT_ERR_NAK = -19;          // NAK received

/** type for result code. */
typedef int result_t;

/**
 * @brief Return the string corresponding to the result code.
 * @param resultCode the result code (see RESULT_ constants).
 * @return the string corresponding to the result code.
 */
const char* getResultCode(result_t resultCode);

#endif // LIBEBUS_RESULT_H_

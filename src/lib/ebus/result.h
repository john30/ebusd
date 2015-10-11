/*
 * Copyright (C) John Baier 2014-2015 <ebusd@ebusd.eu>
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

/** \file result.h */

/** type for result code. */
enum result_t {
	RESULT_OK = 0,                 //!< success

	RESULT_IN_ESC = 1,             //!< start of escape sequence received
	RESULT_EMPTY = 2,              //!< empty result

	RESULT_ERR_GENERIC_IO = -1,    //!< generic I/O error (usually fatal)
	RESULT_ERR_DEVICE = -2,        //!< generic device error (usually fatal)
	RESULT_ERR_SEND = -3,          //!< send error
	RESULT_ERR_ESC = -4,           //!< invalid escape sequence
	RESULT_ERR_TIMEOUT = -5,       //!< read timeout

	RESULT_ERR_NOTFOUND = -6,      //!< file/element not found or not readable
	RESULT_ERR_EOF = -7,           //!< end of input reached
	RESULT_ERR_INVALID_ARG = -8,   //!< invalid argument
	RESULT_ERR_INVALID_NUM = -9,   //!< invalid numeric argument
	RESULT_ERR_INVALID_ADDR = -10, //!< invalid address
	RESULT_ERR_INVALID_POS = -11,  //!< invalid position
	RESULT_ERR_OUT_OF_RANGE = -12, //!< argument value out of valid range
	RESULT_ERR_INVALID_PART = -13, //!< invalid part type value
	RESULT_ERR_MISSING_TYPE = -14, //!< missing data type
	RESULT_ERR_INVALID_LIST = -15, //!< invalid value list
	RESULT_ERR_DUPLICATE = -16,    //!< duplicate entry
	RESULT_ERR_DUPLICATE_NAME = -17, //!< duplicate entry (name)

	RESULT_ERR_BUS_LOST = -18,     //!< arbitration lost
	RESULT_ERR_CRC = -19,          //!< CRC error
	RESULT_ERR_ACK = -20,          //!< ACK error
	RESULT_ERR_NAK = -21,          //!< NAK received

	RESULT_ERR_NO_SIGNAL = -22,    //!< no signal found on the bus
	RESULT_ERR_SYN = -23,          //!< SYN received instead of answer
};


/**
 * Return the string corresponding to the result code.
 * @param resultCode the result code (see RESULT_ constants).
 * @return the string corresponding to the result code.
 */
const char* getResultCode(result_t resultCode);

#endif // LIBEBUS_RESULT_H_

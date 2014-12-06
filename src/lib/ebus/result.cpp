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

#include "result.h"
#include <iostream>

using namespace std;

const char* getResultCode(result_t resultCode) {
	switch (resultCode) {
		case RESULT_IN_ESC:           return "success: escape sequence received";
		case RESULT_SYN:              return "success: SYN received";
		case RESULT_ERR_GENERIC_IO:   return "ERR: generic I/O error";
		case RESULT_ERR_DEVICE:       return "ERR: generic device error";
		case RESULT_ERR_SEND:         return "ERR: send error";
		case RESULT_ERR_ESC:          return "ERR: invalid escape sequence";
		case RESULT_ERR_TIMEOUT:      return "ERR: read timeout";
		case RESULT_ERR_NOTFOUND:     return "ERR: file/element not found or not readable";
		case RESULT_ERR_EOF:          return "ERR: end of input reached";
		case RESULT_ERR_INVALID_ARG:  return "ERR: invalid argument";
		case RESULT_ERR_INVALID_NUM:  return "ERR: invalid numeric argument";
		case RESULT_ERR_INVALID_POS:  return "ERR: invalid position";
		case RESULT_ERR_OUT_OF_RANGE: return "ERR: argument value out of valid range";
		case RESULT_ERR_INVALID_PART: return "ERR: invalid part type value";
		case RESULT_ERR_MISSING_TYPE: return "ERR: missing data type";
		case RESULT_ERR_INVALID_LIST: return "ERR: invalid value list";
		case RESULT_ERR_DUPLICATE:    return "ERR: duplicate entry";
		case RESULT_ERR_BUS_LOST:     return "ERR: arbitration lost";
		case RESULT_ERR_CRC:          return "ERR: CRC error";
		case RESULT_ERR_ACK:          return "ERR: ACK error";
		case RESULT_ERR_NAK:          return "ERR: NAK received";
		default:
			if (resultCode >= 0)
				return "success: unknown result code";
			return "ERR: unknown result code";
	}
}


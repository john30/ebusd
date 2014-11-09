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

namespace libebus
{

const char* getResultCodeCStr(result_t resultCode) {
	switch (resultCode) {
		case RESULT_ERR_SEND: return "ERR_SEND: send error";
		case RESULT_ERR_EXTRA_DATA: return "ERR_EXTRA_DATA: received bytes > sent bytes";
		case RESULT_ERR_NAK: return "ERR_NAK: NAK received";
		case RESULT_ERR_CRC: return "ERR_CRC: CRC error";
		case RESULT_ERR_ACK: return "ERR_ACK: ACK error";
		case RESULT_ERR_TIMEOUT: return "ERR_TIMEOUT: read timeout";
		case RESULT_ERR_SYN: return "ERR_SYN: SYN received";
		case RESULT_ERR_BUS_LOST: return "ERR_BUS_LOST: lost bus arbitration";
		case RESULT_ERR_ESC: return "ERR_ESC: invalid escape sequence received";
		case RESULT_ERR_INVALID_ARG: return "ERR_INVALID_ARG: invalid argument specified";
		case RESULT_ERR_DEVICE: return "ERR_DEVICE: generic device error";
		case RESULT_ERR_EOF: return "ERR_EOF: end of input reached";
		default:
			if (resultCode >= 0)
				return "success";
			return "ERR: unknown error code";
	}
}


} //namespace

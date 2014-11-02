/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "buscommand.h"

namespace libebus
{


BusCommand::BusCommand(const std::string command, const bool isPoll)
	: m_isPoll(isPoll), m_command(command), m_resultCode(RESULT_OK)
{
	unsigned char dstAddress = m_command[1];

	if (dstAddress == BROADCAST)
		m_type = broadcast;
	else if (isMaster(dstAddress) == true)
		m_type = masterMaster;
	else
		m_type = masterSlave;

	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);
}

BusCommand::~BusCommand()
{
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

const std::string BusCommand::getMessageStr()
{
	std::string result;

	if (m_resultCode >= 0) {
		if (m_type == masterSlave) {
			result = m_command.getDataStr(true);
			result += "00";
			result += m_result.getDataStr();
			result += "00";
		} else {
			result = "success";
		}
	} else {
		result = "error";
	}

	return result;
}


} //namespace


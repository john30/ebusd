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

BusCommand::BusCommand(const std::string commandStr, const bool poll, const bool scan)
	: m_poll(poll), m_scan(scan), m_command(commandStr), m_result(), m_resultCode(RESULT_OK)
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
			result = m_command.getDataStr();
			result += "00";
			result += m_result.getDataStr();
			result += "00";
		}
		else
			result = "success";
	}
	else
		result = "error: "+std::string(getResultCodeCStr());

	return result;
}


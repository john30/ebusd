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

#ifndef LIBEBUS_BUSCOMMAND_H_
#define LIBEBUS_BUSCOMMAND_H_

#include "symbol.h"
#include "result.h"


namespace libebus
{


enum CommandType { invalid, broadcast, masterMaster, masterSlave };

class BusCommand
{

public:
	BusCommand(const std::string commandStr, const bool isPoll);
	~BusCommand();

	CommandType getType() const { return m_type; }
	bool isPoll() const { return m_isPoll; }
	SymbolString getCommand() const { return m_command; }
	bool isErrorResult() const { return m_resultCode < 0; }
	const char* getResultCodeCStr();
	SymbolString getResult() const { return m_result; }
	void setResult(const SymbolString result, const int resultCode) { m_result = result; m_resultCode = resultCode; }
	const std::string getMessageStr();
	void waitSignal() { pthread_cond_wait(&m_cond, &m_mutex); } // TODO timeout
	void sendSignal() { pthread_cond_signal(&m_cond); }

private:
	CommandType m_type;
	bool m_isPoll;
	SymbolString m_command;
	SymbolString m_result;
	int m_resultCode;

	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
};


} //namespace

#endif // LIBEBUS_BUSCOMMAND_H_

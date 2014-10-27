/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of libebus.
 *
 * libebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libebus. If not, see http://www.gnu.org/licenses/.
 */

#include "bus.h"
#include <iostream>
#include <iomanip>

namespace libebus
{


BusCommand::BusCommand(const std::string commandStr, const bool isPoll)
	: m_isPoll(isPoll), m_command(commandStr), m_resultCode(RESULT_OK)
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

const char* BusCommand::getResultCodeCStr()
{
	return libebus::getResultCodeCStr(m_resultCode);
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
	}
	else
		result = "error";

	return result;
}


Bus::Bus(const std::string deviceName, const bool noDeviceCheck, const long recvTimeout,
	const std::string dumpFile, const long dumpSize, const bool dumpState)
	: m_previousEscape(false), m_recvTimeout(recvTimeout), m_dumpState(dumpState),
	  m_busLocked(false), m_busPriorRetry(false)
{
	m_port = new Port(deviceName, noDeviceCheck);
	m_dump = new Dump(dumpFile, dumpSize);
}

Bus::~Bus()
{
	if (isConnected() == true)
		disconnect();

	delete m_port;
	delete m_dump;
}

void Bus::printBytes() const
{
	unsigned char byte;
	ssize_t bytes_read;

	bytes_read = m_port->recv(0);

	for (int i = 0; i < bytes_read; i++) {
		byte = m_port->byte();
		std::cout << std::nouppercase << std::hex << std::setw(2)
		          << std::setfill('0') << static_cast<unsigned>(byte);
		if (byte == SYN)
			std::cout << std::endl;
	}
}

int Bus::proceed()
{
	unsigned char byte_recv;
	ssize_t bytes_recv;

	// fetch new message and get bus
	if (m_sendBuffer.size() != 0 && m_sstr.size() == 0) {
		BusCommand* busCommand = m_sendBuffer.front();
		return getBus(busCommand->getCommand()[0]);
	}

	// wait for new data
	bytes_recv = m_port->recv(0);

	if (bytes_recv < 0)
		return RESULT_ERR_DEVICE;

	for (int i = 0; i < bytes_recv; i++) {

		// fetch next byte
		byte_recv = recvByte();

		// store byte
		return proceedCycData(byte_recv); // TODO what if more than one byte was received?
	}

	return RESULT_SYN;
}

int Bus::proceedCycData(const unsigned char byte)
{
	if (byte != SYN) {
		m_sstr.push_back_unescape(byte, m_previousEscape, false);
		if (m_busLocked == true)
			m_busLocked = false;

		return RESULT_DATA;
	}

	m_previousEscape = false;
	if (byte == SYN && m_sstr.size() != 0) {
		// lock bus after SYN-BYTE-SYN Sequence
		if (m_sstr.size() == 1 && m_busPriorRetry == false)
			m_busLocked = true;

		m_cycBuffer.push(m_sstr);
		m_sstr.clear();

		if (m_busLocked == true)
			return RESULT_BUS_LOCKED;
	}

	return RESULT_SYN;
}

SymbolString Bus::getCycData()
{
	SymbolString data;

	if (m_cycBuffer.empty() == false) {
		data = m_cycBuffer.front();
		m_cycBuffer.pop();
	}

	return data;
}

int Bus::getBus(const unsigned char byte_sent)
{
	unsigned char byte_recv;
	ssize_t bytes_sent, bytes_recv;

	// send QQ
	bytes_sent = m_port->send(&byte_sent, 1);
	if (bytes_sent <= 0)
		return RESULT_ERR_SEND;

	// receive 1 byte - must be QQ
	bytes_recv = m_port->recv(0, 1);

	if (bytes_recv < 0)
		return RESULT_ERR_DEVICE;

	// fetch next byte
	byte_recv = recvByte();

	// compare sent and received byte
	if (bytes_recv == 1 && byte_sent == byte_recv) {
		m_busPriorRetry = false;
		return RESULT_BUS_ACQUIRED;
  	}

	// store byte
	int ret = proceedCycData(byte_recv);
	if (ret >= 0)
		return ret;
// TODO this needs to be re-designed with above proceedCycData()
	// compare prior nibble for retry
	if (bytes_recv == 1 && (byte_sent & 0x0F) == (byte_recv & 0x0F)) {
		m_busPriorRetry = true;
		return RESULT_BUS_PRIOR_RETRY;
	}

	m_busLocked = true;
	return RESULT_ERR_BUS_LOST;
}

BusCommand* Bus::sendCommand()
{
	unsigned char byte_recv;
	ssize_t bytes_recv;
	std::string result;
	SymbolString slaveData;
	int retval = RESULT_OK;

	BusCommand* busCommand = m_sendBuffer.front();
	m_sendBuffer.pop();

	// send ZZ PB SB NN Dx CRC
	SymbolString command = busCommand->getCommand();
	for (size_t i = 1; i < command.size(); i++) {
		retval = sendByte(command[i]);
		if (retval < 0)
			goto on_exit;
	}

	// BC -> send SYN
	if (busCommand->getType() == broadcast) {
		sendByte(SYN);
		goto on_exit;
	}

	// receive ACK
	bytes_recv = m_port->recv(m_recvTimeout);
	if (bytes_recv > 1) {
		retval = RESULT_ERR_EXTRA_DATA;
		goto on_exit;
	} else if (bytes_recv < 0) {
		retval = RESULT_ERR_TIMEOUT;
		goto on_exit;
	}

	byte_recv = recvByte();

	// is received byte SYN?
	if (byte_recv == SYN) {
		retval = RESULT_ERR_SYN;
		goto on_exit;
	}

	// is slave ACK negative?
	if (byte_recv == NAK) {

		// send QQ ZZ PB SB NN Dx CRC again
		for (size_t i = 0; i < command.size(); i++) {
			retval = sendByte(command[i]);
			if (retval < 0)
				goto on_exit;
		}

		// receive ACK
		bytes_recv = m_port->recv(m_recvTimeout);
		if (bytes_recv > 1) {
			retval = RESULT_ERR_EXTRA_DATA;
			goto on_exit;
		} else if (bytes_recv < 0) {
			retval = RESULT_ERR_TIMEOUT;
			goto on_exit;
		}

		byte_recv = recvByte();

		// is received byte SYN?
		if (byte_recv == SYN) {
			retval = RESULT_ERR_SYN;
			goto on_exit;
		}

		// is slave ACK negative?
		if (byte_recv == NAK) {
			retval = sendByte(SYN);
			if (retval == 0)
				retval = RESULT_ERR_NAK;

			goto on_exit;
		}
	}

	// MM -> send SYN
	if (busCommand->getType() == masterMaster) {
		sendByte(SYN);
		goto on_exit;
	}

	// receive NN, Dx, CRC
	retval = recvSlaveDataAndCRC(slaveData);

	// are calculated and received CRC equal?
	if (retval == RESULT_ERR_CRC) {

		// send NAK
		retval = sendByte(NAK);
		if (retval < 0)
			goto on_exit;

		// receive NN, Dx, CRC
		slaveData = SymbolString();
		retval = recvSlaveDataAndCRC(slaveData);

		// are calculated and received CRC equal?
		if (retval == RESULT_ERR_CRC) {

			// send NAK
			retval = sendByte(NAK);
			if (retval >= 0)
				retval = RESULT_ERR_CRC;
		}
	}

	if (retval < 0)
		goto on_exit;

	// send ACK
	retval = sendByte(ACK);
	if (retval == -1) {
		retval = RESULT_ERR_ACK;
		goto on_exit;
	}

	// MS -> send SYN
	sendByte(SYN);

on_exit:

	// empty receive buffer
	while (m_port->size() != 0)
		byte_recv = recvByte();

	busCommand->setResult(slaveData, retval);
	return busCommand;

}

BusCommand* Bus::delCommand()
{
	BusCommand* busCommand = m_sendBuffer.front();
	m_sendBuffer.pop();

	busCommand->setResult(SymbolString(), RESULT_ERR_BUS_LOST);
	return busCommand;
}

int Bus::sendByte(const unsigned char byte_sent)
{
	unsigned char byte_recv;
	ssize_t bytes_sent, bytes_recv;

	bytes_sent = m_port->send(&byte_sent, 1);

	// receive 1 byte - must be equal
	bytes_recv = m_port->recv(RECV_TIMEOUT);
	if (bytes_sent != bytes_recv)
		return RESULT_ERR_EXTRA_DATA;

	byte_recv = recvByte();

	if (byte_sent != byte_recv)
		return RESULT_ERR_SEND;

	return RESULT_OK;
}

unsigned char Bus::recvByte()
{
	unsigned char byte_recv;

	// fetch byte
	byte_recv = m_port->byte();

	if (m_dumpState == true)
		m_dump->write((const char*) &byte_recv);

	return byte_recv;
}

int Bus::recvSlaveDataAndCRC(SymbolString& result)
{
	unsigned char byte_recv;
	ssize_t bytes_recv;
	bool previousEscape = false;

	// receive NN
	bytes_recv = m_port->recv(RECV_TIMEOUT, 1);
	if (bytes_recv < 0)
		return RESULT_ERR_TIMEOUT;

	byte_recv = recvByte();
	byte_recv = result.push_back_unescape(byte_recv, previousEscape);
	if (previousEscape == true && byte_recv == 0)
		return RESULT_ERR_ESC;

	// escape sequence: get another symbol to find NN
	if (previousEscape == true) {
		bytes_recv = m_port->recv(RECV_TIMEOUT, 1);
		if (bytes_recv < 0)
			return RESULT_ERR_TIMEOUT;

		byte_recv = recvByte();
		byte_recv = result.push_back_unescape(byte_recv, previousEscape);
		if (previousEscape == true)
			return RESULT_ERR_ESC;
	}

	int NN = byte_recv;

	// receive Dx
	for (int i = 0; i < NN; i++) {
		bytes_recv = m_port->recv(RECV_TIMEOUT, 1);
		if (bytes_recv < 0)
			return RESULT_ERR_TIMEOUT;

		byte_recv = recvByte();
		byte_recv = result.push_back_unescape(byte_recv, previousEscape);
		if (previousEscape == true && byte_recv == 0)
			return RESULT_ERR_ESC;

		// escape sequence: increase NN
		if (previousEscape == true)
			NN++;
	}
	if (previousEscape == true)
		return RESULT_ERR_ESC;

	unsigned char crc_calc = result.getCRC();
	// receive CRC
	bytes_recv = m_port->recv(RECV_TIMEOUT, 1);
	if (bytes_recv < 0)
		return RESULT_ERR_TIMEOUT;

	byte_recv = recvByte();
	byte_recv = result.push_back_unescape(byte_recv, previousEscape, false);
	if (previousEscape == true && byte_recv == 0)
		return RESULT_ERR_ESC;

	// escape sequence: get another symbol to find CRC
	if (previousEscape == true) {
		bytes_recv = m_port->recv(RECV_TIMEOUT, 1);
		if (bytes_recv < 0)
			return RESULT_ERR_TIMEOUT;

		byte_recv = recvByte();
		byte_recv = result.push_back_unescape(byte_recv, previousEscape);
		if (previousEscape == true)
			return RESULT_ERR_ESC;
	}

	if (crc_calc != byte_recv)
		return RESULT_ERR_CRC;

	return RESULT_OK;
}


} //namespace


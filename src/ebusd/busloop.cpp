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

#include "busloop.h"
#include "logger.h"
#include "appl.h"
#include <fstream>
#include <iomanip>

using namespace std;

extern Logger& L;
extern Appl& A;

BusMessage::BusMessage(const string command, const bool poll, const bool scan)
	: m_poll(poll), m_scan(scan), m_command(command), m_result(), m_resultCode(RESULT_OK)
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

const string BusMessage::getMessageStr()
{
	string result;

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
		result = "error: "+string(getResultCodeCStr());

	return result;
}


BusLoop::BusLoop(Commands* commands)
	: m_commands(commands), m_running(true), m_lockCounter(0),
	  m_priorRetry(false), m_scan(false), m_scanFull(false), m_scanIndex(0)
{
	m_port = new Port(A.getOptVal<const char*>("device"), A.getOptVal<bool>("nodevicecheck"));
	m_port->open();

	if (m_port->isOpen() == false)
		L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

	m_dumpFile = A.getOptVal<const char*>("dumpfile");
	m_dumpSize = A.getOptVal<long>("dumpsize");
	m_dumping = A.getOptVal<bool>("dump");

	m_logRawData = A.getOptVal<bool>("lograwdata");

	m_pollInterval = A.getOptVal<int>("pollinterval");

	m_recvTimeout = A.getOptVal<long>("recvtimeout");

	m_sendRetries = A.getOptVal<int>("sendretries");

	m_lockRetries = A.getOptVal<int>("lockretries");

	m_acquireTime = A.getOptVal<long>("acquiretime");
}

BusLoop::~BusLoop()
{
	if (m_port->isOpen() == true)
		m_port->close();

	delete m_port;
}

void* BusLoop::run()
{
	int sendRetries = 0;
	int lockRetries = 0;

	// polling
	time_t pollStart, pollEnd;
	time(&pollStart);
	double pollDelta;

	for (;;) {
		if (m_port->isOpen() == true) {
			ssize_t numBytes;

			// add poll or scan command
			if (m_commands->sizePollDB() > 0 || m_scan == true) {
				// check polling delta
				time(&pollEnd);
				pollDelta = difftime(pollEnd, pollStart);

				// add new polling command to send
				if (pollDelta >= m_pollInterval) {
					if (m_scan == true)
						addScanMessage();
					else
						addPollMessage();

					time(&pollStart);
				}
			}

			// read device - no timeout needed (AUTO-SYN)
			numBytes = m_port->recv(0);

			if (numBytes < 0) {
				L.log(bus, error, " ERR_DEVICE: generic device error");
				continue;
			}

			// cycle bytes
			collectCycData(numBytes);

			// send command
			if (m_sstr.size() == 0 && m_lockCounter == 0 && m_busQueue.size() > 0) {
				// acquire Bus
				int busResult = acquireBus();

				// send bus command
				if (busResult == RESULT_BUS_ACQUIRED) {
					BusMessage* message = sendCommand();
					L.log(bus, trace, " %s", message->getMessageStr().c_str());

					if (message->isErrorResult() == true) {
						if (sendRetries < m_sendRetries) {
							sendRetries++;
							L.log(bus, trace, " send retry %d", sendRetries);
							message->setResult(string(), RESULT_OK);
						}
						else {
							sendRetries = 0;
							L.log(bus, event, " send retry failed", sendRetries);

							if (message->isPoll() == true)
								delete m_busQueue.remove();
							else
								message->sendSignal();
						}
					}
					else {
						sendRetries = 0;

						if (message->isPoll() == true) {
							if (message->isScan() == true)
								m_commands->storeScanData(message->getMessageStr().c_str());
							else
								m_commands->storePollData(message->getMessageStr().c_str()); // TODO use getResult()
							delete message;
						}
						else
							message->sendSignal();
					}

					lockRetries = 0;
					m_lockCounter = A.getOptVal<int>("lockcounter");
				}
				else if (busResult == RESULT_ERR_BUS_LOST) {
					L.log(bus, trace, " acquire bus failed");

					if (lockRetries >= m_lockRetries) {
						lockRetries = 0;
						L.log(bus, event, " lock bus failed");

						BusMessage* message = m_busQueue.remove();
						if (message->isPoll() == true)
							delete message;
						else
							message->sendSignal();
					}
					else {
						lockRetries++;
						L.log(bus, trace, " lock retry %d", lockRetries);
					}

					m_lockCounter = A.getOptVal<int>("lockcounter");
				}

			}

		}
		else {
			// TODO: define max reopen
			sleep(10);
			m_port->open();

			if (m_port->isOpen() == false)
				L.log(bus, error, "can't open %s", A.getOptVal<const char*>("device"));

		}

		if (m_running == false) {
			if (m_port->isOpen() == true)
				m_port->close();

			return NULL;
		}

	}

	return NULL;
}

int BusLoop::writeDumpFile(const char* byte)
{
	int ret = 0;

	ofstream fs(m_dumpFile.c_str(), ios::out | ios::binary | ios::app);

	if (fs == 0)
		return -1;

	fs.write(byte, 1);

	if (fs.tellp() >= m_dumpSize * 1024) {
		string oldfile;
		oldfile += m_dumpFile;
		oldfile += ".old";
		ret = rename(m_dumpFile.c_str(), oldfile.c_str());
	}

	fs.close();

	return ret;
}

unsigned char BusLoop::fetchByte()
{
	unsigned char byte;

	// fetch byte
	byte = m_port->byte();

	if (m_dumping == true)
		writeDumpFile((const char*) &byte);

	if (m_logRawData == true)
		L.log(bus, event, "%02x", byte);

	return byte;
}

void BusLoop::collectCycData(const int numRecv)
{
	// cycle bytes
	for (int i = 0; i < numRecv; i++) {

		// fetch byte
		unsigned char byte = fetchByte();

		if (byte == SYN) {

			// analyse cycle data
			if (m_sstr.size() > 0) {

				analyseCycData();

				if (m_sstr.size() == 1 && m_lockCounter == 0 && m_priorRetry == false)
					m_lockCounter++;

				else if (m_lockCounter > 0)
					m_lockCounter--;

				m_sstr.clear();
			}

			else if (m_lockCounter > 0)
				m_lockCounter--;

		}

		// collect cycle data
		else
			m_sstr.push_back(byte, true, false);
	}
}

void BusLoop::analyseCycData()
{
	//TODO check the input data for validity

	static bool skipfirst = false;

	if (skipfirst == true) {
		L.log(cyc, trace, "%s", m_sstr.getDataStr().c_str());

		int index = m_commands->storeCycData(m_sstr.getDataStr());

		if (index == -1) {
			L.log(cyc, debug, " command not found");
		}
		else if (index == -2) {
			L.log(cyc, debug, " no commands defined");
		}
		else if (index == -3) {
			L.log(cyc, debug, " search skipped - string too short");
		}
		else {
			string tmp;
			tmp += (*m_commands)[index][1];
			tmp += " ";
			tmp += (*m_commands)[index][2];
			L.log(cyc, event, " cycle   [%4d] %s", index, tmp.c_str());
		}

		// collect Slave address
		if (index != -3)
			collectSlave();
	}
	else
		skipfirst = true;
}

void BusLoop::collectSlave()
{
	vector<unsigned char>::iterator it;

	for (int i = 0; i < 2; i++) {
		bool found = false;
		unsigned char mm = m_sstr[i];

		if (i == 0) {
			if (mm == 0xFF)
				mm = 0x04;
			else
				mm += 0x05;
		}

		for (it = m_slave.begin(); it != m_slave.end(); it++)
			if ((*it) == mm)
				found = true;

		if (found == false && isMaster(mm) == false && mm != BROADCAST) {
			m_slave.push_back(mm);
			L.log(bus, event, " new slave: %d %02x", m_slave.size(), m_slave.back());
		}
	}
}

int BusLoop::acquireBus()
{
	unsigned char recvByte, sendByte;
	ssize_t numRecv, numSend;

	sendByte = m_busQueue.next()->getCommand()[0];

	// send QQ
	numSend = m_port->send(&sendByte);
	if (numSend <= 0) {
		L.log(bus, error, " ERR_SEND: send error");
		return RESULT_ERR_SEND;
	}

	// wait ~4200 usec for receive
	usleep(m_acquireTime);

	// receive 1 byte - must be QQ
	numRecv = m_port->recv(0);

	if (numRecv < 0) {
		L.log(bus, error, " ERR_DEVICE: generic device error");
		return RESULT_ERR_DEVICE;
	}

	if (numRecv == 1) {
		// fetch byte
		recvByte = fetchByte();

		// compare sent and received byte
		if (sendByte == recvByte) {
			L.log(bus, trace, " bus acquired");
			return RESULT_BUS_ACQUIRED;
		}

		// collect cycle data
		if (recvByte != SYN)
			m_sstr.push_back(recvByte, true, false);

		// compare prior nibble for retry
		if ((sendByte & 0x0F) == (recvByte & 0x0F)) {
			m_priorRetry = true;
			L.log(bus, trace, " bus prior retry");
			return RESULT_BUS_PRIOR_RETRY;
		}

		L.log(bus, error, " ERR_BUS_LOST: lost bus arbitration");
		return RESULT_ERR_BUS_LOST;
  	}

  	// cycle bytes
	collectCycData(numRecv);

	L.log(bus, error, " ERR_EXTRA_DATA: received bytes > sent bytes");
	return RESULT_ERR_EXTRA_DATA;
}

BusMessage* BusLoop::sendCommand()
{
	unsigned char recvByte;
	string result;
	SymbolString slaveData;
	int retval = RESULT_OK;

	BusMessage* message = m_busQueue.next();

	// send ZZ PB SB NN Dx CRC
	SymbolString command = message->getCommand();
	for (size_t i = 1; i < command.size(); i++) {
		retval = sendByte(command[i]);
		if (retval < 0)
			goto on_exit;
	}

	// BC -> send SYN
	if (message->getType() == broadcast) {
		sendByte(SYN);
		goto on_exit;
	}

	// receive ACK
	retval = recvSlaveAck(recvByte);
	if (retval < 0)
		goto on_exit;

	// is slave ACK negative?
	if (recvByte == NAK) {

		// send QQ ZZ PB SB NN Dx CRC again
		for (size_t i = 0; i < command.size(); i++) {
			retval = sendByte(command[i]);
			if (retval < 0)
				goto on_exit;
		}

		// receive ACK
		retval = recvSlaveAck(recvByte);
		if (retval < 0)
			goto on_exit;

		// is slave ACK negative?
		if (recvByte == NAK) {
			sendByte(SYN);
			L.log(bus, error, " ERR_NAK: NAK received");
			retval = RESULT_ERR_NAK;
			goto on_exit;
		}
	}

	// MM -> send SYN
	if (message->getType() == masterMaster) {
		sendByte(SYN);
		goto on_exit;
	}

	// receive NN, Dx, CRC
	retval = recvSlaveData(slaveData);

	// are calculated and received CRC equal?
	if (retval == RESULT_ERR_CRC) {

		// send NAK
		retval = sendByte(NAK);
		if (retval < 0)
			goto on_exit;

		// receive NN, Dx, CRC
		slaveData.clear();
		retval = recvSlaveData(slaveData);

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
		L.log(bus, error, " ERR_ACK: ACK error");
		retval = RESULT_ERR_ACK;
		goto on_exit;
	}

	// MS -> send SYN
	sendByte(SYN);

on_exit:

	// empty receive buffer
	while (m_port->size() != 0)
		recvByte = fetchByte();

	message->setResult(slaveData, retval);

	if (retval == RESULT_OK)
		return m_busQueue.remove();
	else
		return message;

}

int BusLoop::sendByte(const unsigned char sendByte)
{
	unsigned char recvByte;
	ssize_t numRecv, numSend;

	numSend = m_port->send(&sendByte);

	// receive 1 byte - must be equal
	numRecv = m_port->recv(RECV_TIMEOUT);

	if (numSend != numRecv) {
		L.log(bus, error, " ERR_EXTRA_DATA: received bytes > sent bytes");
		return RESULT_ERR_EXTRA_DATA;
	}

	recvByte = fetchByte();

	if (sendByte != recvByte) {
		L.log(bus, error, " ERR_SEND: send error");
		return RESULT_ERR_SEND;
	}

	return RESULT_OK;
}

int BusLoop::recvSlaveAck(unsigned char& recvByte)
{
	ssize_t numRecv;

	// receive ACK
	numRecv = m_port->recv(m_recvTimeout);

	if (numRecv > 1) {
		L.log(bus, error, " ERR_EXTRA_DATA: received bytes > sent bytes");
		return RESULT_ERR_EXTRA_DATA;
	}
	else if (numRecv < 0) {
		L.log(bus, error, " ERR_TIMEOUT: read timeout");
		return RESULT_ERR_TIMEOUT;
	}

	recvByte = fetchByte();

	// is received byte SYN?
	if (recvByte == SYN) {
		L.log(bus, error, " ERR_SYN: SYN received");
		return RESULT_ERR_SYN;
	}

	return RESULT_OK;
}

int BusLoop::recvSlaveData(SymbolString& result)
{
	unsigned char recvByte, calcCrc = 0;
	ssize_t numRecv;
	size_t NN = 0;
	bool updateCrc = true;
	int retval = 0;

	for (size_t i = 0, needed = 1; i < needed; i++) {
		numRecv = m_port->recv(RECV_TIMEOUT);
		if (numRecv < 0) {
			L.log(bus, error, " ERR_TIMEOUT: read timeout");
			return RESULT_ERR_TIMEOUT;
		}

		recvByte = fetchByte();
		retval = result.push_back(recvByte, true, updateCrc);
		if (retval < 0)
			return retval;

		if (retval == RESULT_IN_ESC)
			needed++;
		else if (result.size() == 1) { // NN received
			NN = result[0];
			needed += NN;
		}
		else if (NN > 0 && result.size() == 1+NN) {// all data received
			updateCrc = false;
			calcCrc = result.getCRC();
			needed++;
		}
	}

	if (retval == RESULT_IN_ESC) {
		L.log(bus, error, " ERR_ESC: invalid escape sequence received");
		return RESULT_ERR_ESC;
	}

	if (updateCrc == true || calcCrc != result[result.size()-1]) {
		L.log(bus, error, " ERR_CRC: CRC error");
		return RESULT_ERR_CRC;
	}

	return RESULT_OK;
}

void BusLoop::addPollMessage()
{
	int index = m_commands->nextPollCommand();
	if (index < 0) {
		L.log(bus, error, "polling index out of range");
	}
	else {
		// TODO: implement as methode from class commands?
		string tmp;
		tmp += (*m_commands)[index][1];
		tmp += " ";
		tmp += (*m_commands)[index][2];
		L.log(bus, event, " polling [%4d] %s", index, tmp.c_str());

		string busCommand(A.getOptVal<const char*>("address"));
		busCommand += m_commands->getBusCommand(index);
		transform(busCommand.begin(), busCommand.end(), busCommand.begin(), ::tolower);

		BusMessage* message = new BusMessage(busCommand, true, false);
		L.log(bus, trace, " msg: %s", busCommand.c_str());

		addMessage(message);
	}
}

void BusLoop::addScanMessage()
{
	string busCommand(A.getOptVal<const char*>("address"));
	stringstream sstr;

	if (m_scanFull == true) {
		for (; m_scanIndex <= 0xFF; m_scanIndex++) {
			if (isMaster(m_scanIndex) == false && m_scanIndex != SYN
			&& m_scanIndex != ESC && m_scanIndex != BROADCAST) {
				sstr << nouppercase << setw(2) << setfill('0')
				     << hex << m_scanIndex;
				break;
			}
		}
	}
	else {
		sstr << nouppercase << setw(2) << setfill('0')
		     << hex << static_cast<unsigned>(m_slave[m_scanIndex]);

		if (m_scanIndex+1 >= m_slave.size())
			m_scan = false;
	}

	if (m_scanIndex > 0xFF)
		m_scan = false;
	else {
		m_scanIndex++;

		busCommand += sstr.str();
		busCommand += "070400";
		transform(busCommand.begin(), busCommand.end(), busCommand.begin(), ::tolower);

		L.log(bus, event, " scanning address %s", sstr.str().c_str());


		BusMessage* message = new BusMessage(busCommand, true, true);
		L.log(bus, trace, " msg: %s", busCommand.c_str());

		addMessage(message);
	}
}

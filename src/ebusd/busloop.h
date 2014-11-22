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

#ifndef BUSLOOP_H_
#define BUSLOOP_H_

#include "commands.h"
#include "port.h"
#include "wqueue.h"
#include "thread.h"
#include "symbol.h"
#include "result.h"

/** the maximum time [us] allowed for retrieving a byte from an addressed slave */
#define RECV_TIMEOUT 10000

/** possible bus command types */
enum BusCommandType {
	invalid,      // invalid command type
	broadcast,    // broadcast
	masterMaster, // master - master
	masterSlave,  // master - slave
};

/**
 * @brief class for data/message transfer between baseloop and busloop.
 */
class BusMessage
{

public:
	/**
	 * @brief construct a new bus message instance and determine command type.
	 * @param command the command data to write on bus.
	 * @param poll true if message type is polling.
	 * @param scan true if message type is scanning.
	 */
	BusMessage(const std::string command, const bool poll, const bool scan);

	/**
	 * @brief destructor.
	 */
	~BusMessage()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	/**
	 * @brief get the bus command type.
	 * @return the bus command type.
	 */
	BusCommandType getType() const { return m_type; }

	/**
	 * @brief get the command string.
	 * @return the command string.
	 */
	SymbolString getCommand() const { return m_command; }

	/**
	 * @brief get the result string.
	 * @return the result string.
	 */
	SymbolString getResult() const { return m_result; }

	/**
	 * @brief set the result string and result code.
	 * @param result the result string.
	 * @param resultCode the result code.
	 */
	void setResult(const SymbolString result, const int resultCode)
		{ m_result = result; m_resultCode = resultCode; }

	/**
	 * @brief return status of result code.
	 * @return true if result code is negativ.
	 */
	bool isErrorResult() const { return m_resultCode < 0; }

	/**
	 * @brief return output string of result code.
	 * @return the output string of result code.
	 */
	const char* getResultCodeCStr() const { return getResultCode(m_resultCode); }

	/**
	 * @brief return the message string or error result string.
	 * @return the message string or error result string.
	 */
	const std::string getMessageStr();

	/**
	 * @brief return polling flag of message type.
	 * @return true if message type is polling.
	 */
	bool isPoll() const { return m_poll; }

	/**
	 * @brief return scanning flag of message type.
	 * @return true if message type is scanning.
	 */
	bool isScan() const { return m_scan; }

	/**
	 * @brief wait on notification.
	 */
	void waitSignal() { pthread_cond_wait(&m_cond, &m_mutex); } // TODO timeout

	/**
	 * @brief send notification.
	 */
	void sendSignal() { pthread_cond_signal(&m_cond); }

private:
	/** the bus command type */
	BusCommandType m_type;

	/** true if message is of type polling */
	bool m_poll;

	/** true if message is of type scanning */
	bool m_scan;

	/** the command string (master data) */
	SymbolString m_command;

	/** the result string (slave data) */
	SymbolString m_result;

	/** the result code of result string */
	int m_resultCode;

	/** mutex variable for exclusive lock */
	pthread_mutex_t m_mutex;

	/** condition variable for exclusive lock */
	pthread_cond_t m_cond;

};

/**
 * @brief class busloop which handle all bus activities.
 */
class BusLoop : public Thread
{

public:
	/**
	 * @brief create a busloop instance and set the commands instance.
	 * @param commands the commands instance.
	 */
	BusLoop(Commands* commands);

	/**
	 * @brief destructor.
	 */
	~BusLoop();

	/**
	 * @brief endless loop for busloop instance.
	 * @return void pointer.
	 */
	void* run();

	/**
	 * @brief shut down busloop.
	 */
	void stop() { m_running = false; }

	/**
	 * @brief add a new bus message to internal message queue.
	 * @param message the bus message.
	 */
	void addMessage(BusMessage* message) { m_busQueue.add(message); }

	/**
	 * @brief switch to new commands instance.
	 * @param commands reference of new loaded commands instance.
	 */
	void reload(Commands* commands) { m_commands = commands; }

	/**
	 * @brief scanning ebus do determine bus members.
	 * @param full if true a scan of all slave addresses will be done.
	 */
	void scan(const bool full=false) { m_scan = true; m_scanFull = full; m_scanIndex = 0; }

	/**
	 * @brief toggle (on/off) logging of raw data to logging system.
	 */
	void raw() { m_logRawData == true ? m_logRawData = false :  m_logRawData = true ; }

	/**
	 * @brief set the name of dump file.
	 * @param dumpFile the file name of dump file.
	 */
	void setDumpFile(const std::string& dumpFile) { m_dumpFile = dumpFile; }

	/**
	 * @brief set the max size of dump file.
	 * @param dumpSize the max. size of the dump file, before switching.
	 */
	void setDumpSize(const long dumpSize) { m_dumpSize = dumpSize; }

	/**
	 * @brief toggle (on/off) dumping of raw bytes to a dump file.
	 */
	void dump() { m_dumping == true ? m_dumping = false :  m_dumping = true ; }

private:
	/** the commands instance */
	Commands* m_commands;

	/** the port instance which control the ebus device */
	Port* m_port;

	/** the name of dump file*/
	std::string m_dumpFile;

	/** max. size of dump file */
	long m_dumpSize;

	/** true if dumping of raw bytes to file is enabled */
	bool m_dumping;

	/** true if logging of raw bytes is enabled */
	bool m_logRawData;

	/** true if this instance is running */
	bool m_running;

	/** bus access is not allowed if counter is greater than 0 */
	int m_lockCounter;

	/** if true, we lost bus acquire but same priority class.
	 *  after next SYN sign we are allowed to try again to aquire bus.
	 */
	bool m_priorRetry;

	/** queue for bus messages */
	WQueue<BusMessage*> m_busQueue;

	/** string for cycle bus data */
	SymbolString m_sstr;

	/** number of send retries for one bus command */
	int m_sendRetries;

	/** number of lock retries (acquire bus) for one bus command */
	int m_lockRetries;

	/** time for receiving answer from slave [us] */
	long m_recvTimeout;

	/** waiting time for bus acquire [us] */
	long m_acquireTime;

	/** time between to polling commands [s] */
	double m_pollInterval;

	/** vector with collected slave addresses */
	std::vector<unsigned char> m_slave;

	/** true if bus scanning for collected slave addresses is active */
	bool m_scan;

	/** true if bus scanning for all slave addresses is active */
	bool m_scanFull;

	/** internal index do get next scan command */
	size_t m_scanIndex;

	/**
	 * @brief write byte to dump file.
	 * @param byte to write
	 * @return -1 if dump file cannot opened or renaming of dump file failed.
	 */
	int writeDumpFile(const char* byte);

	/**
	 * @brief fetch next byte of device input buffer (dumping and raw logging).
	 * @return next byte of device.
	 */
	unsigned char fetchByte();

	/**
	 * @brief collect cycle bytes. the analysis of collected bytes will be triggered after next SYN sign.
	 * @param numRecv the number of bytes to analyze.
	 */
	void collectCycData(const int numRecv);

	/**
	 * @brief the analyzing of collected bytes. collecting of slave address will be triggered.
	 */
	void analyseCycData();

	/**
	 * @brief determine and collect slave addresses.
	 */
	void collectSlave();

	/**
	 * @brief try to acquire bus for sending purpose.
	 * @return result code of bus acquiring.
	 */
	int acquireBus();

	/**
	 * @brief handle sending of a bus command.
	 * @return a reference to sent bus message.
	 */
	BusMessage* sendCommand();

	/**
	 * @brief send 1 byte to bus device.
	 * @param sendByte the byte to send.
	 * @return result code of byte sending.
	 */
	int sendByte(const unsigned char sendByte);

	/**
	 * @brief receive ACK from slave.
	 * @param reference for receive byte.
	 * @return result code of receiving byte.
	 */
	int recvSlaveAck(unsigned char& recvByte);

	/**
	 * @brief receive slave data block.
	 * @param reference for result string.
	 * @return result code of receiving slave data.
	 */
	int recvSlaveData(SymbolString& result);

	/**
	 * @brief add a polling bus message to internal message queue.
	 * @param message the bus message.
	 */
	void addPollMessage();

	/**
	 * @brief add a scanning bus message to internal message queue.
	 * @param message the bus message.
	 */
	void addScanMessage();

};

#endif // BUSLOOP_H_

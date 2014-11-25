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

#ifndef LIBUTILS_DAEMON_H_
#define LIBUTILS_DAEMON_H_

/**
 * @brief class to daemonize a process.
 */
class Daemon
{

public:
	/**
	 * @brief create an instance and return the reference.
	 * @return the reference to instance.
	 */
	static Daemon& Instance();

	/**
	 * @brief daemonize act process.
	 * @param pidfile the name of the pid file.
	 */
	void run(const char* pidfile);

	/**
	 * @brief stop daemon and delete the pid file.
	 */
	void stop() { pidfile_close(); }

	/**
	 * @brief show actual status if daemonize.
	 * @return true if process is a daemon.
	 */
	bool status() { return m_status; }

private:
	/**
	 * @brief private construtor.
	 */
	Daemon() {}

	/**
	 * @brief private copy construtor.
	 * @param reference to an instance.
	 */
	Daemon(const Daemon&);

	/**
	 * @brief private = operator.
	 * @param reference to an instance.
	 * @return reference to instance.
	 */
	Daemon& operator=(const Daemon&);

	/** status of process; true if we are a daemon */
	bool m_status;

	/** name of the pid file*/
	const char* m_pidfile;

	/** file descriptor of the pid file */
	int m_pidfd;

	/**
	 * @brief creates a pid file for process.
	 * @return true if success.
	 */
	bool pidfile_open();

	/**
	 * @brief close and delete the pid file.
	 * @return true if success.
	 */
	bool pidfile_close();

};

#endif // LIBUTILS_DAEMON_H_

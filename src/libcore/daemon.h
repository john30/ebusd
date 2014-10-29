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

#ifndef LIBCORE_DAEMON_H_
#define LIBCORE_DAEMON_H_

class Daemon
{

public:
	static Daemon& Instance();
	~Daemon() {}

	void run(const char* file);
	void stop() { pidfile_close(); }
	bool status() { return m_status; }

private:
	bool m_status;
	const char* m_pidfile;
	int m_pidfd;

	Daemon() {}
	Daemon(const Daemon&);
	Daemon& operator= (const Daemon&);

	bool pidfile_open();
	bool pidfile_close();

};

#endif // LIBCORE_DAEMON_H_

/*
 * Copyright (C) Roland Jax 2014 <roland.jax@liwest.at>
 *
 * This file is part of ebus-daemon.
 *
 * ebus-daemon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus-daemon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus-daemon. If not, see http://www.gnu.org/licenses/.
 */

#ifndef DAEMON_H_
#define DAEMON_H_

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

#endif // DAEMON_H_

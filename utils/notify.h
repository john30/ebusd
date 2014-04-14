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
 
#ifndef NOTIFY_H_
#define NOTIFY_H_

#include <unistd.h>
#include <fcntl.h>

class Notify
{

public:
	Notify()
	{
		int pipefd[2];
		int ret = pipe(pipefd);

		if (ret == 0) {
			m_recvfd = pipefd[0];
			m_sendfd = pipefd[1];
				
			fcntl(m_sendfd, F_SETFL, O_NONBLOCK);
		}

	}
	virtual ~Notify() { close(m_sendfd); close(m_recvfd); }

	int notifyFD() const { return m_recvfd; }
	int notify() const { return write(m_sendfd,"1",1); }

private:
        int m_recvfd;
        int m_sendfd;
        
};

#endif // NOTIFY_H_



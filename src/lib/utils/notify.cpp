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

#include "notify.h"

Notify::Notify()
{
	int pipefd[2];
	int ret = pipe(pipefd);

	if (ret == 0) {
		m_recvfd = pipefd[0];
		m_sendfd = pipefd[1];

		fcntl(m_sendfd, F_SETFL, O_NONBLOCK);
	}

}

Notify::~Notify()
{
	close(m_sendfd);
	close(m_recvfd);
}



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

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "tcpsocket.h"
#include "wqueue.h"
#include "notify.h"
#include "thread.h"
#include "message.h"

class Connection : public Thread
{

public:
	Connection(TCPSocket* socket, WQueue<Message*>* data)
		: m_socket(socket), m_data(data), m_running(false) { m_sum++; m_id = m_sum;}

	void addResult(Message message);

	void* run();
	void stop() const { m_notify.notify(); }
	bool isRunning() const { return m_running; }

	int getID() { return m_id; }

private:
	TCPSocket* m_socket;
	WQueue<Message*>* m_data;
	WQueue<Message*> m_result;
	Notify m_notify;
	bool m_running;
	int m_id;

	static int m_sum;

};

#endif // CONNECTION_H_

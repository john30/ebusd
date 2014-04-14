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

#include "daemon.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

Daemon& Daemon::Instance()
{
	static Daemon instance;
	return instance;
}

void Daemon::run(const char* file)
{
	m_status = false;
	m_pidfile = file;
	m_pidfd = 0;
	
	pid_t pid;

	// fork off the parent process
	pid = fork();
	
	if (pid < 0) {
		std::cerr << "ebusd fork() failed." << std::endl;
		exit(EXIT_FAILURE);
	}

	// If we got a good PID, then we can exit the parent process
	if (pid > 0) {
		// printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}

	// At this point we are executing as the child process

	// Set file permissions 750
	umask(027);

	// Create a new SID for the child process and
	// detach the process from the parent (normally a shell)
	if (setsid() < 0) {
		std::cerr << "ebusd setsid() failed." << std::endl;
		exit(EXIT_FAILURE);
	}

	// Change the current working directory. This prevents the current
	// directory from being locked; hence not being able to remove it.
	if (chdir("/tmp") < 0) {  //DAEMON_WORKDIR
		std::cerr << "ebusd chdir() failed." << std::endl;
		exit(EXIT_FAILURE);
	}

	// Close stdin, stdout and stderr
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// write pidfile and try to lock it
	if (pidfile_open() == false) {
		std::cerr << "can't open pidfile: %s" << m_pidfile << std::endl;
		exit(EXIT_FAILURE);
	}

	m_status = true;
}

bool Daemon::pidfile_open()
{
	char pid[10];

	m_pidfd = open(m_pidfile, O_RDWR|O_CREAT, 0600);
	if (m_pidfd < 0)
		return false;

	if (lockf(m_pidfd, F_TLOCK, 0) < 0)
		return false;

	sprintf(pid, "%d\n", getpid());
	if (write(m_pidfd, pid, strlen(pid)) < 0)
		return false;

	return true;
}

bool Daemon::pidfile_close()
{
	if (close(m_pidfd) < 0)
		return false;

	if (remove(m_pidfile) < 0)
		return false;

	return true;
}

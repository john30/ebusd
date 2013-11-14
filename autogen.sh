#!/bin/sh
#
# Copyright (C) Roland Jax 2012-2013 <roland.jax@liwest.at>
#
# This file is part of ebusd.
#
# ebusd is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ebusd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ebusd. If not, see http://www.gnu.org/licenses/.
#

exists()
{
        if command $1 
        then
                return 1
        else
		printf " command not found\n";
		exit
        fi
}

run()
{
	$1
	if [ $? -eq 0 ]
	then
		printf " done\n";
	else
		printf " command failed\n";
		exit
	fi
}

printf ">>> aclocal";
exists aclocal
run aclocal

printf ">>> autoconf";
exists autoconf;
run autoconf

printf ">>> autoheader";
exists autoheader
run autoheader

printf ">>> automake";
exists automake
run automake

printf ">>> configure\n";
./configure

printf ">>> make\n";
run make

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBEBUS_CONTRIB_H_
#define LIBEBUS_CONTRIB_H_

/** @file contrib.h
 * Contributed sources that may be excluded from regular builds.
 * configure switch: --without-contrib
 */

using namespace std;

/**
 * Registration function that is called once during initialization.
 * @return true if registration was successful.
 */
bool libebus_contrib_register();

#endif // LIBEBUS_CONTRIB_H_

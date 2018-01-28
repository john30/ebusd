/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2018 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/contrib/contrib.h"
#include "lib/ebus/contrib/tem.h"

namespace ebusd {

bool libebus_contrib_register() {
  contrib_tem_register();
  return true;
}

}  // namespace ebusd

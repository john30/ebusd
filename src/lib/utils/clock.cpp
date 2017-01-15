/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2017 John Baier <ebusd@ebusd.eu>
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

#include "clock.h"
#ifdef __MACH__
#  include <mach/clock.h>
#  include <mach/mach.h>
#endif

#ifdef __MACH__
static bool clockInitialized = false;
static clock_serv_t clockServ;
#endif

void clockGettime(struct timespec* t) {
#ifdef __MACH__
  if (!clockInitialized) {
    clockInitialized = true;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clockServ);
  }
  mach_timespec_t mts;
  clock_get_time(clockServ, &mts);
  t->tv_sec = mts.tv_sec;
  t->tv_nsec = mts.tv_nsec;
#else
  clock_gettime(CLOCK_REALTIME, t);
#endif
}

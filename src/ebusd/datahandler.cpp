/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2021 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ebusd/datahandler.h"
#ifdef HAVE_MQTT
#  include "ebusd/mqtthandler.h"
#endif

namespace ebusd {

/** the final @a argp_child structure. */
static const struct argp_child g_last_argp_child = {nullptr, 0, nullptr, 0};

/** the list of @a argp_child structures. */
static struct argp_child g_argp_children[
            1
#ifdef HAVE_MQTT
            +1
#endif
];

const struct argp_child* datahandler_getargs() {
  size_t count = 0;
#ifdef HAVE_MQTT
  g_argp_children[count++] = *mqtthandler_getargs();
#endif
  if (count > 0) {
    g_argp_children[count] = g_last_argp_child;
    return g_argp_children;
  }
  return nullptr;
}

bool datahandler_register(UserInfo* userInfo, BusHandler* busHandler, MessageMap* messages,
    list<DataHandler*>* handlers) {
  bool success = true;
#ifdef HAVE_MQTT
  if (!mqtthandler_register(userInfo, busHandler, messages, handlers)) {
    success = false;
  }
#endif
  return success;
}

void DataSink::notifyUpdate(Message* message) {
  if (message && message->hasLevel(m_levels)) {
    m_updatedMessages[message->getKey()]++;
  }
}

}  // namespace ebusd

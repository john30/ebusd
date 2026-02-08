/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2026 John Baier <ebusd@ebusd.eu>
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
#ifdef HAVE_KNX
#  include "ebusd/knxhandler.h"
#endif

namespace ebusd {

/** the final @a argParseChildOpt structure. */
static const argParseChildOpt g_last_arg_child = {nullptr, nullptr};

/** the list of @a argParseChildOpt structures. */
static argParseChildOpt g_arg_children[
            1
#ifdef HAVE_MQTT
            +1
#endif
#ifdef HAVE_KNX
            +1
#endif
];

const argParseChildOpt* datahandler_getargs() {
  size_t count = 0;
#ifdef HAVE_MQTT
  g_arg_children[count++] = *mqtthandler_getargs();
#endif
#ifdef HAVE_KNX
  g_arg_children[count++] = *knxhandler_getargs();
#endif
  if (count > 0) {
    g_arg_children[count] = g_last_arg_child;
    return g_arg_children;
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
#ifdef HAVE_KNX
  if (!knxhandler_register(userInfo, busHandler, messages, handlers)) {
    success = false;
  }
#endif
  return success;
}

void DataSink::notifyUpdate(Message* message, bool changed) {
  if (message && message->hasLevel(m_levels)) {
    if (m_changedOnly && !changed) {
      return;
    }
    m_updatedMessages[message->getKey()]++;
  }
}

}  // namespace ebusd

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022 John Baier <ebusd@ebusd.eu>
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

#include <string.h>

#include "lib/knx/knx.h"

#ifdef HAVE_KNXD
#include "lib/knx/knxd.h"
#endif
#include "lib/knx/knxnet.h"

namespace ebusd {

unsigned int parseInt(const char* str, int base, unsigned int minValue, unsigned int maxValue,
                      bool* error) {
  char* strEnd = nullptr;

  unsigned long ret = strtoul(str, &strEnd, base);

  if (strEnd == nullptr || strEnd == str || *strEnd != 0) {
    *error = true;  // invalid value
    return 0;
  }

  if (minValue > ret || ret > maxValue) {
    *error = true;  // invalid value
    return 0;
  }
  return (unsigned int)ret;
}

knx_addr_t parseAddress(const string &str, bool isGroup, bool* error) {
  auto sep = isGroup ? '/' : '.';
  auto pos = str.find(sep);
  if (pos != string::npos) {
    auto pos2 = str.find(sep, pos+1);
    bool err = false;
    unsigned int v = 0;
    v = parseInt(str.substr(0, pos).c_str(), 10, 0, isGroup ? 0x1f : 0x0f, &err);
    if (!err) {
      auto dest = static_cast<knx_addr_t>(v << (isGroup ? 11 : 12));
      if (pos2 == string::npos) {
        // 2 level
        if (isGroup) {
          v = parseInt(str.substr(pos+1).c_str(), 10, 0, 0x7ff, &err);
          if (!err) {
            dest |= static_cast<knx_addr_t>(v);
            return dest;
          }
        }
      } else {
        // 3 level
        v = parseInt(str.substr(pos+1, pos2-pos-1).c_str(), 10, 0, isGroup ? 0x07 : 0x0f, &err);
        if (!err) {
          dest |= static_cast<knx_addr_t>(v << 8);
          v = parseInt(str.substr(pos2+1).c_str(), 10, 0, 0xff, &err);
          if (!err) {
            dest |= static_cast<knx_addr_t>(v);
            return dest;
          }
        }
      }
    }
  }
  if (error) {
    *error = true;
  }
  return 0;
}

// copydoc
KnxConnection *KnxConnection::create(const char *url) {
#ifdef HAVE_KNXD
  if (strchr(url, ':')) {
    return new KnxdConnection(url);
  }
#endif
  return new KnxNetConnection(url);
}

}  // namespace ebusd

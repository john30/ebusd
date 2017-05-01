/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
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

#include "lib/ebus/symbol.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "lib/ebus/result.h"

namespace ebusd {

using std::ostringstream;
using std::nouppercase;
using std::setw;
using std::hex;
using std::setfill;

/**
 * CRC8 lookup table for the polynom 0x9b = x^8 + x^7 + x^4 + x^3 + x^1 + 1.
 */
static const symbol_t CRC_LOOKUP_TABLE[] = {
  0x00, 0x9b, 0xad, 0x36, 0xc1, 0x5a, 0x6c, 0xf7, 0x19, 0x82, 0xb4, 0x2f, 0xd8, 0x43, 0x75, 0xee,
  0x32, 0xa9, 0x9f, 0x04, 0xf3, 0x68, 0x5e, 0xc5, 0x2b, 0xb0, 0x86, 0x1d, 0xea, 0x71, 0x47, 0xdc,
  0x64, 0xff, 0xc9, 0x52, 0xa5, 0x3e, 0x08, 0x93, 0x7d, 0xe6, 0xd0, 0x4b, 0xbc, 0x27, 0x11, 0x8a,
  0x56, 0xcd, 0xfb, 0x60, 0x97, 0x0c, 0x3a, 0xa1, 0x4f, 0xd4, 0xe2, 0x79, 0x8e, 0x15, 0x23, 0xb8,
  0xc8, 0x53, 0x65, 0xfe, 0x09, 0x92, 0xa4, 0x3f, 0xd1, 0x4a, 0x7c, 0xe7, 0x10, 0x8b, 0xbd, 0x26,
  0xfa, 0x61, 0x57, 0xcc, 0x3b, 0xa0, 0x96, 0x0d, 0xe3, 0x78, 0x4e, 0xd5, 0x22, 0xb9, 0x8f, 0x14,
  0xac, 0x37, 0x01, 0x9a, 0x6d, 0xf6, 0xc0, 0x5b, 0xb5, 0x2e, 0x18, 0x83, 0x74, 0xef, 0xd9, 0x42,
  0x9e, 0x05, 0x33, 0xa8, 0x5f, 0xc4, 0xf2, 0x69, 0x87, 0x1c, 0x2a, 0xb1, 0x46, 0xdd, 0xeb, 0x70,
  0x0b, 0x90, 0xa6, 0x3d, 0xca, 0x51, 0x67, 0xfc, 0x12, 0x89, 0xbf, 0x24, 0xd3, 0x48, 0x7e, 0xe5,
  0x39, 0xa2, 0x94, 0x0f, 0xf8, 0x63, 0x55, 0xce, 0x20, 0xbb, 0x8d, 0x16, 0xe1, 0x7a, 0x4c, 0xd7,
  0x6f, 0xf4, 0xc2, 0x59, 0xae, 0x35, 0x03, 0x98, 0x76, 0xed, 0xdb, 0x40, 0xb7, 0x2c, 0x1a, 0x81,
  0x5d, 0xc6, 0xf0, 0x6b, 0x9c, 0x07, 0x31, 0xaa, 0x44, 0xdf, 0xe9, 0x72, 0x85, 0x1e, 0x28, 0xb3,
  0xc3, 0x58, 0x6e, 0xf5, 0x02, 0x99, 0xaf, 0x34, 0xda, 0x41, 0x77, 0xec, 0x1b, 0x80, 0xb6, 0x2d,
  0xf1, 0x6a, 0x5c, 0xc7, 0x30, 0xab, 0x9d, 0x06, 0xe8, 0x73, 0x45, 0xde, 0x29, 0xb2, 0x84, 0x1f,
  0xa7, 0x3c, 0x0a, 0x91, 0x66, 0xfd, 0xcb, 0x50, 0xbe, 0x25, 0x13, 0x88, 0x7f, 0xe4, 0xd2, 0x49,
  0x95, 0x0e, 0x38, 0xa3, 0x54, 0xcf, 0xf9, 0x62, 0x8c, 0x17, 0x21, 0xba, 0x4d, 0xd6, 0xe0, 0x7b,
};


unsigned int parseInt(const char* str, int base, unsigned int minValue, unsigned int maxValue,
    result_t* result, size_t* length) {
  char* strEnd = NULL;

  unsigned long ret = strtoul(str, &strEnd, base);

  if (strEnd == NULL || strEnd == str || *strEnd != 0) {
    *result = RESULT_ERR_INVALID_NUM;  // invalid value
    return 0;
  }

  if (minValue > ret || ret > maxValue) {
    *result = RESULT_ERR_OUT_OF_RANGE;  // invalid value
    return 0;
  }
  if (length != NULL) {
    *length = (unsigned int)(strEnd - str);
  }
  *result = RESULT_OK;
  return (unsigned int)ret;
}

int parseSignedInt(const char* str, int base, int minValue, int maxValue,
    result_t* result, size_t* length) {
  char* strEnd = NULL;

  long ret = strtol(str, &strEnd, base);

  if (strEnd == NULL || *strEnd != 0) {
    *result = RESULT_ERR_INVALID_NUM;  // invalid value
    return 0;
  }

  if (minValue > ret || ret > maxValue) {
    *result = RESULT_ERR_OUT_OF_RANGE;  // invalid value
    return 0;
  }
  if (length != NULL) {
    *length = (unsigned int)(strEnd - str);
  }
  *result = RESULT_OK;
  return static_cast<int>(ret);
}


void SymbolString::updateCrc(symbol_t value, symbol_t* crc) {
  *crc = CRC_LOOKUP_TABLE[*crc]^value;
}

result_t SymbolString::parseHex(const string& str) {
  result_t result;
  for (size_t i = 0; i < str.size(); i += 2) {
    symbol_t value = (symbol_t)parseInt(str.substr(i, 2).c_str(), 16, 0, 0xff, &result);
    if (result != RESULT_OK) {
      return result;
    }
    m_data.push_back(value);
  }
  return RESULT_OK;
}

result_t SymbolString::parseHexEscaped(const string& str) {
  result_t result;
  bool inEscape = false;
  for (size_t i = 0; i < str.size(); i += 2) {
    symbol_t value = (symbol_t)parseInt(str.substr(i, 2).c_str(), 16, 0, 0xff, &result);
    if (result != RESULT_OK) {
      return result;
    }
    if (inEscape) {
      if (value == 0x00) {
        m_data.push_back(ESC);
        inEscape = false;
      } else if (value == 0x01) {
        m_data.push_back(SYN);
        inEscape = false;
      } else {
        return RESULT_ERR_ESC;  // invalid escape sequence
      }
    } else if (value == ESC) {
      inEscape = true;
    } else if (value == SYN) {
      return RESULT_ERR_ESC;  // invalid escape sequence
    } else {
      m_data.push_back(value);
    }
  }
  return inEscape ? RESULT_ERR_ESC : RESULT_OK;
}

const string SymbolString::getStr(size_t skipFirstSymbols) const {
  ostringstream sstr;
  for (size_t i = 0; i < m_data.size(); i++) {
    if (skipFirstSymbols > 0) {
      skipFirstSymbols--;
    } else {
      sstr << nouppercase << setw(2) << hex
          << setfill('0') << static_cast<unsigned>(m_data[i]);
    }
  }
  return sstr.str();
}

symbol_t SymbolString::calcCrc() const {
  symbol_t crc = 0;
  for (size_t i = 0; i < m_data.size(); i++) {
    symbol_t value = m_data[i];
    if (value == ESC) {
      updateCrc(ESC, &crc);
      updateCrc(0x00, &crc);
    } else if (value == SYN) {
      updateCrc(ESC, &crc);
      updateCrc(0x01, &crc);
    } else {
      updateCrc(value, &crc);
    }
  }
  return crc;
}


/**
 * Return the index of the upper or lower 4 bits of a master address.
 * @param bits the upper or lower 4 bits of the address.
 * @return the 1-based index of the upper or lower 4 bits of a master address (1 to 5), or 0.
 */
unsigned int getMasterPartIndex(symbol_t bits) {
  switch (bits) {
  case 0x0:
    return 1;
  case 0x1:
    return 2;
  case 0x3:
    return 3;
  case 0x7:
    return 4;
  case 0xF:
    return 5;
  default:
    return 0;
  }
}

bool isMaster(symbol_t addr) {
  return getMasterPartIndex(addr & 0x0F) > 0
    && getMasterPartIndex((addr & 0xF0)>>4) > 0;
}

bool isSlaveMaster(symbol_t addr) {
  return isMaster((symbol_t)(addr+256-5));
}

symbol_t getSlaveAddress(symbol_t addr) {
  if (isMaster(addr)) {
    return (symbol_t)(addr+5);
  }
  if (isValidAddress(addr, false)) {
    return addr;
  }
  return SYN;
}

symbol_t getMasterAddress(symbol_t addr) {
  if (isMaster(addr)) {
    return addr;
  }
  addr = (symbol_t)(addr+256-5);
  if (isMaster(addr)) {
    return addr;
  }
  return SYN;
}

unsigned int getMasterNumber(symbol_t addr) {
  unsigned int priority = getMasterPartIndex(addr & 0x0F);
  if (priority == 0) {
    return 0;
  }
  unsigned int index = getMasterPartIndex((addr & 0xF0) >> 4);
  if (index == 0) {
    return 0;
  }
  return 5*(priority-1) + index;
}

bool isValidAddress(symbol_t addr, bool allowBroadcast) {
  return addr != SYN && addr != ESC && (allowBroadcast || addr != BROADCAST);
}

}  // namespace ebusd

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/ebus/contrib/tem.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include "lib/ebus/datatype.h"

namespace ebusd {

using std::setfill;
using std::setw;
using std::dec;

void contrib_tem_register() {
  DataTypeList::getInstance()->add(new TemParamDataType("TEM_P"));
}

result_t TemParamDataType::derive(int divisor, size_t bitCount, const NumberDataType** derived) const {
  if (divisor == 0) {
    divisor = 1;
  }
  if (bitCount == 0) {
    bitCount = m_bitCount;
  }
  if (divisor == 1 && bitCount == 16) {
    *derived = this;
    return RESULT_OK;
  }
  return RESULT_ERR_INVALID_ARG;
}

result_t TemParamDataType::readSymbols(size_t offset, size_t length, const SymbolString& input,
    OutputFormat outputFormat, ostream* output) const {
  unsigned int value = 0;

  result_t result = readRawValue(offset, length, input, &value);
  if (result != RESULT_OK) {
    return result;
  }

  if (value == m_replacement) {
    if (outputFormat & OF_JSON) {
      *output << "null";
    } else {
      *output << NULL_VALUE;
    }
    return RESULT_OK;
  }
  int grp = 0, num = 0;
  if (input.isMaster()) {
    grp = (value & 0x1f);  // grp in bits 0...5
    num = ((value >> 8) & 0x7f);  // num in bits 8...13
  } else {
    grp = ((value >> 7) & 0x1f);  // grp in bits 7...11
    num = (value & 0x7f);  // num in bits 0...6
  }
  if (outputFormat & OF_JSON) {
    *output << '"';
  }
  *output << setfill('0') << setw(2) << dec << grp << '-' << setw(3) << num;
  if (outputFormat & OF_JSON) {
    *output << '"';
  }
  *output << setfill(' ') << setw(0);  // reset
  return RESULT_OK;
}

result_t TemParamDataType::writeSymbols(const size_t offset, const size_t length, istringstream* input,
    SymbolString* output, size_t* usedLength) const {
  unsigned int value;
  unsigned int grp, num;

  if (input->str() == NULL_VALUE) {
    value = m_replacement;  // replacement value
  } else {
    string token;
    if (input->eof() || !getline(*input, token, '-')) {
      return RESULT_ERR_EOF;  // incomplete
    }
    const char* str = token.c_str();
    if (str == NULL || *str == 0) {
      return RESULT_ERR_EOF;  // input too short
    }
    char* strEnd = NULL;
    grp = (unsigned int)strtoul(str, &strEnd, 10);
    if (strEnd == NULL || strEnd == str || *strEnd != 0) {
      return RESULT_ERR_INVALID_NUM;  // invalid value
    }
    if (input->eof() || !getline(*input, token, '-')) {
      return RESULT_ERR_EOF;  // incomplete
    }
    str = token.c_str();
    if (str == NULL || *str == 0) {
      return RESULT_ERR_EOF;  // input too short
    }
    strEnd = NULL;
    num = (unsigned int)strtoul(str, &strEnd, 10);
    if (strEnd == NULL || strEnd == str || *strEnd != 0) {
      return RESULT_ERR_INVALID_NUM;  // invalid value
    }
    if (grp > 0x1f || num > 0x7f) {
      return RESULT_ERR_OUT_OF_RANGE;  // value out of range
    }
    if (output->isMaster()) {
      value = grp | (num << 8);  // grp in bits 0...5, num in bits 8...13
    } else {
      value = (grp << 7) | num;  // grp in bits 7...11, num in bits 0...6
    }
  }
  if (value < getMinValue() || value > getMaxValue()) {
    return RESULT_ERR_OUT_OF_RANGE;  // value out of range
  }
  return writeRawValue(value, offset, length, output, usedLength);
}

}  // namespace ebusd

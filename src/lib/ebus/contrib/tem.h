/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2017 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_CONTRIB_TEM_H_
#define LIB_EBUS_CONTRIB_TEM_H_

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/ebus/datatype.h"

namespace ebusd {

/** @file lib/ebus/contrib/tem.h
 * Contributed data types for TEM devices not necessarily part of regular releases.
 */

/**
 * A special variant of @a NumberDataType for TEM/Dungs ParamID in master/slave
 * data.
 */
class TemParamDataType : public NumberDataType {
 public:
  /**
   * Constructs a new instance.
   * @param id the type identifier.
   */
  explicit TemParamDataType(const string& id)
    : NumberDataType(id, 16, 0, 0xffff, 0, 0xffff, 0, NULL) {}

  // @copydoc
  result_t derive(int divisor, size_t bitCount, const NumberDataType** derived) const override;

  // @copydoc
  result_t readSymbols(size_t offset, size_t length, const SymbolString& input,
      const OutputFormat outputFormat, ostream* output) const override;

  // @copydoc
  result_t writeSymbols(size_t offset, size_t length, istringstream* input,
      SymbolString* output, size_t* usedLength) const override;
};

/**
 * Registration function to be called once during initialization.
 */
void contrib_tem_register();

}  // namespace ebusd

#endif  // LIB_EBUS_CONTRIB_TEM_H_

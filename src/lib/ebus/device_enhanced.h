/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2015-2026 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_DEVICE_ENHANCED_H_
#define LIB_EBUS_DEVICE_ENHANCED_H_

#include <cstdint>
#include <string>
#include "lib/ebus/result.h"
#include "lib/ebus/symbol.h"

namespace ebusd {

/** @file lib/ebus/device_enhanced.h
 * Enhanced protocol definitions for @a Device instances.
 */

// ebusd enhanced protocol IDs:
#define ENH_REQ_INIT ((uint8_t)0x0)
#define ENH_RES_RESETTED ((uint8_t)0x0)
#define ENH_REQ_SEND ((uint8_t)0x1)
#define ENH_RES_RECEIVED ((uint8_t)0x1)
#define ENH_REQ_START ((uint8_t)0x2)
#define ENH_RES_STARTED ((uint8_t)0x2)
#define ENH_REQ_INFO ((uint8_t)0x3)
#define ENH_RES_INFO ((uint8_t)0x3)
#define ENH_RES_FAILED ((uint8_t)0xa)
#define ENH_RES_ERROR_EBUS ((uint8_t)0xb)
#define ENH_RES_ERROR_HOST ((uint8_t)0xc)

// ebusd enhanced error codes for the ENH_RES_ERROR_* responses
#define ENH_ERR_FRAMING ((uint8_t)0x00)
#define ENH_ERR_OVERRUN ((uint8_t)0x01)

#define ENH_BYTE_FLAG ((uint8_t)0x80)
#define ENH_BYTE_MASK ((uint8_t)0xc0)
#define ENH_BYTE1 ((uint8_t)0xc0)
#define ENH_BYTE2 ((uint8_t)0x80)
#define makeEnhancedByte1(cmd, data) (uint8_t)(ENH_BYTE1 | ((cmd) << 2) | (((data)&0xc0) >> 6))
#define makeEnhancedByte2(cmd, data) (uint8_t)(ENH_BYTE2 | ((data)&0x3f))
#define makeEnhancedSequence(cmd, data) {makeEnhancedByte1(cmd, data), makeEnhancedByte2(cmd, data)}


/**
 * Interface for an enhanced @a Device.
 */
class EnhancedDeviceInterface {
 public:
  /**
   * Destructor.
   */
  virtual ~EnhancedDeviceInterface() {}

  /**
   * Check for a running extra infos request, wait for it to complete,
   * and then send a new request for extra infos to enhanced device.
   * @param infoId the ID of the info to request.
   * @param wait true to wait for a running request to complete, false to send right away.
   * @return @a RESULT_OK on success, or an error code otherwise.
   */
  virtual result_t requestEnhancedInfo(symbol_t infoId, bool wait = true) = 0;  // abstract

  /**
   * Get the enhanced device version.
   * @return @a a string with the version infos, or empty.
   */
  virtual string getEnhancedVersion() const = 0;  // abstract

  /**
   * Retrieve/update all extra infos from an enhanced device.
   * @return @a a string with the extra infos, or empty.
   */
  virtual string getEnhancedInfos() = 0;  // abstract
};

}  // namespace ebusd

#endif  // LIB_EBUS_DEVICE_ENHANCED_H_

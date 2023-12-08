/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2023 John Baier <ebusd@ebusd.eu>
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

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cassert>
#include <cmath>
#include "ebusd/knxhandler.h"

using namespace std;
using namespace ebusd;

/**
 * Translates a float value into KNX data type 9 (2-byte float). Algiruthm adapted from Calimero 2, 
 * see method io.calimero.dptxlator.DPTXlator2ByteFloat::toDPT(double, short[], int).
*/
uint32_t floatToInt16(float val) {
  // encoding: val = (0.01*M)*2^E
  double v = val * 100.0f;
  int e = 0;
  for (; v < -2048.0f; v /= 2)
    e++;
  for (; v > 2047.0f; v /= 2)
    e++;
  int m = (int) round(v) & 0x7FF;
  short msb = (short) (e << 3 | m >> 8);
  if (val < 0.0)
    msb |= 0x80;
  return msb << 8 | m;
}

/**
 * Translates KNX data type 9 (2-byte float) into a float value. Algiruthm adapted from Calimero 2, 
 * see method io.calimero.dptxlator.DPTXlator2ByteFloat::fromDPT(int).
*/
float int16ToFloat(uint16_t val) {
  // DPT bits high byte: MEEEEMMM, low byte: MMMMMMMM
  // left align all mantissa bits
  int v = (((val >> 8) & 0x80) << 24) | (((val >> 8) & 0x7) << 28) | ((val & 0x00FF) << 20);
  // normalize
  v >>= 20;
  int exp = ((val >> 8) & 0x78) >> 3;
  return static_cast<float>((1 << exp) * v) * 0.01f;
}

int testIntToFloat(uint16_t test, float expect) {
  float val = int16ToFloat(test);
  cout << std::setprecision(2) << std::fixed;
  if (val == expect) {
    cout << "Correct: 0x" << std::hex << test << " == " << val << endl;
    return 0;
  } else {
    cout << "Incorrect: 0x" << std::hex << test << " != " << val << ", should be " << std::dec << expect << endl;
    return -1;
  }
}

int testFloatToInt(float test, uint16_t expect) {
  uint16_t val = floatToInt16(test);
  cout << std::setprecision(2) << std::fixed;
  if (val == expect) {
    cout << "Correct: " << test << " == 0x" << std::hex << val << endl;
    return 0;
  } else {
    cout << "Incorrect: " << test << " != 0x" << std::hex << val << ", should be " << "0x" << expect << endl;
    return -1;
  }
}

int main() {
  int res = 0;

  // decode
  res += testIntToFloat((uint16_t) 0x0000, (float) 0.0);
  res += testIntToFloat((uint16_t) 0x07FF, (float) 20.47);
  res += testIntToFloat((uint16_t) 0x6464, (float) 46039.04);
  res += testIntToFloat((uint16_t) 0x7FFF, (float) 670760.96); // maximum positive value

  res += testIntToFloat((uint16_t) 0x87FF, (float) -0.01);
  res += testIntToFloat((uint16_t) 0x8000, (float) -20.48);
  res += testIntToFloat((uint16_t) 0x8A24, (float) -30.0);
  res += testIntToFloat((uint16_t) 0xAC00, (float) -327.68);
  res += testIntToFloat((uint16_t) 0xC8C8, (float) -9461.76);
  res += testIntToFloat((uint16_t) 0xF800, (float) -671088.64); // maximum negative value

  // encode
  res += testFloatToInt((float) 0.0, (uint16_t) 0x0000);
  res += testFloatToInt((float) 20.47, (uint16_t) 0x07FF);
  res += testFloatToInt((float) 46039.04, (uint16_t) 0x6464);
  res += testFloatToInt((float) 670760.96, (uint16_t) 0x7FFF); // maximum positive value

  res += testFloatToInt((float) -0.01, (uint16_t) 0x87FF);
  res += testFloatToInt((float) -20.48, (uint16_t) 0x8000);
  res += testFloatToInt((float) -30.0, (uint16_t) 0x8A24);
  res += testFloatToInt((float) -9461.76, (uint16_t) 0xC8C8);
  res += testFloatToInt((float) -671088.64, (uint16_t) 0xF800); // maximum negative value

  return res;
}

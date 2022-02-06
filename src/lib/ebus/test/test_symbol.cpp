/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2022 John Baier <ebusd@ebusd.eu>
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
#include <string>
#include <unordered_map>
#include "lib/ebus/symbol.h"

using namespace std;
using namespace ebusd;

static bool error = false;

void verify(bool expectFailMatch, string type, string input,
    bool match, string expectStr, string gotStr) {
  match = match && expectStr == gotStr;
  if (expectFailMatch) {
    if (match) {
      cout << "  failed " << type << " match >" << input
              << "< error: unexpectedly succeeded" << endl;
      error = true;
    } else {
      cout << "  failed " << type << " match >" << input << "< OK" << endl;
    }
  } else if (match) {
    cout << "  " << type << " match >" << input << "< OK" << endl;
  } else {
    cout << "  " << type << " match >" << input << "< error: got >"
            << gotStr << "<, expected >" << expectStr << "<" << endl;
      error = true;
  }
}

int main(int argc, char** argv) {
  MasterSymbolString mstr;
  if (argc > 1) {
    result_t result;
    if (argc > 2 && strcmp("escaped", argv[1]) == 0) {
      result = mstr.parseHexEscaped(argv[2]);
    } else {
      result = mstr.parseHex(argv[1]);
    }
    if (result != RESULT_OK) {
      cout << "parse escaped error: " << getResultCode(result) << endl;
    } else {
      symbol_t gotCrc = mstr.calcCrc();
      cout << "calculated CRC: 0x"
          << nouppercase << setw(2) << hex << setfill('0')
              << static_cast<unsigned>(gotCrc) << endl;
    }
    return 0;
  }

  string gotStr, expectStr;
  result_t result = mstr.parseHex("10feb5050427a915aa");
  if (result != RESULT_OK) {
    cout << "parse unescaped error: " << getResultCode(result) << endl;
    error = true;
  } else {
    gotStr = mstr.getStr(), expectStr = "10feb5050427a915aa";
    verify(false, "parse unescaped", "10feb5050427a915aa", true, expectStr, gotStr);
    symbol_t gotCrc = mstr.calcCrc(), expectCrc = 0x77;
    ostringstream ostr;
    ostr << nouppercase << setw(2) << hex << setfill('0') << static_cast<unsigned>(expectCrc);
    expectStr = ostr.str();
    ostr.str("");
    ostr << nouppercase << setw(2) << hex << setfill('0') << static_cast<unsigned>(gotCrc);
    gotStr = ostr.str();
    verify(false, "CRC", "10feb5050427a915aa", gotCrc == expectCrc, expectStr, gotStr);
  }

  mstr.clear();
  result = mstr.parseHexEscaped("10feb5050427a90015a901");
  if (result != RESULT_OK) {
    cout << "parse escaped error: " << getResultCode(result) << endl;
    error = true;
  } else {
    gotStr = mstr.getStr(), expectStr = "10feb5050427a915aa";
    verify(false, "parse escaped", "10feb5050427a90015a901", true, expectStr, gotStr);
    ostringstream ostr;
    ostr << dec << static_cast<unsigned>(4);
    expectStr = ostr.str();
    ostr.str("");
    ostr << dec << static_cast<unsigned>(mstr.getDataSize());
    gotStr = ostr.str();
    verify(false, "data size", "10feb5050427a90015a901", mstr.getDataSize() == 4, expectStr, gotStr);
  }

  SlaveSymbolString sstr;
  result = sstr.parseHexEscaped("0427a90015a901");
  if (result != RESULT_OK) {
    cout << "parse escaped error: " << getResultCode(result) << endl;
    error = true;
  } else {
    gotStr = sstr.getStr(), expectStr = "0427a915aa";
    verify(false, "parse escaped", "0427a90015a901", true, expectStr, gotStr);
    ostringstream ostr;
    ostr << dec << static_cast<unsigned>(4);
    expectStr = ostr.str();
    ostr.str("");
    ostr << dec << static_cast<unsigned>(sstr.getDataSize());
    gotStr = ostr.str();
    verify(false, "data size", "0427a90015a901", sstr.getDataSize() == 4, expectStr, gotStr);
  }

  int masterCnt = 0, slaveCnt = 0;
  for (int i=0; i<256; i++) {
    symbol_t address = static_cast<symbol_t>(i);
    if (isMaster(address)) {
      masterCnt++;
    } else if (isValidAddress(address, false)) {
      slaveCnt++;
    }
  }
  if (masterCnt == 25) {
    cout << "count master addresses OK" << endl;
  } else {
    cout << "count master addresses error: found " << dec << masterCnt << " instead of 25" << endl;
  }
  if (slaveCnt == 228) {
    cout << "count slave addresses OK" << endl;
  } else {
    cout << "count slave addresses error: found " << dec << slaveCnt << " instead of 228" << endl;
  }
  return error ? 1 : 0;
}

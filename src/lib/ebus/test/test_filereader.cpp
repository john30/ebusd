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

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "lib/ebus/filereader.h"

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

string resultlines[][3] = {
  {"col 1", "col 2", "col 3"},
  {"line 2 col 1", "line 2 col 2", "line 2 \"col 3\";default of col 3"},
  {"", "", ""},
  {"line 4 col 1", "line 4 col 2 part 1;line 4 col 2 part 2", "line 4 col 3;default of col 3"},
  {"", "", ""},
  {"line 6 col 1", "", "line 6 col 3;default of col 3"},
  {"", "", ""},
  {"line 8 col 1", "line 8 col 2 part 1;line 8 col 2 part 2", "line 8 col 3;default of col 3"},
};

string resultsublines[][2][4] = {
  {},
  {{"subcol 1", "line 2 subcol 1", "subcol 2", "line 2 subcol 2;default of sub 0 subcol 2"},{"subcol 2", "line 2 subcol 2", "subcol 3", "line 2 subcol 3"}},
  {},
  {{"subcol 1", "line 4 subcol 1", "subcol 2", "line 4 subcol 2;default of sub 0 subcol 2"},{"subcol 2", "line 4 subcol 2", "subcol 3", "line 4 subcol 3"}},
  {},
  {{"subcol 1", "line 6 subcol 1", "subcol 2", "line 6 subcol 2;default of sub 0 subcol 2"},{"subcol 2", "line 6 subcol 2", "subcol 3", "line 6 subcol 3"}},
  {},
  {{"subcol 1", "line 8 subcol 1", "subcol 2", "line 8 subcol 2;default of sub 0 subcol 2"},{"subcol 2", "line 8 subcol 2", "subcol 3", "line 8 subcol 3"}},
};

static unsigned int baseLine = 0;

class NoopReader : public FileReader {
 public:
  result_t addFromFile(vector<string>& row, string& errorDescription,
      const string filename, unsigned int lineNo) override {
    return RESULT_OK;
  }
};

class TestReader : public MappedFileReader {
 public:
  TestReader(size_t expectedCols) : MappedFileReader::MappedFileReader(false), m_expectedCols(expectedCols) {}
  result_t getFieldMap(vector<string>& row, string& errorDescription) override {
    if (row.size() == m_expectedCols) {
      cout << "get field map: split OK" << endl;
      return RESULT_OK;
    }
    cout << "get field map: error got " << static_cast<unsigned>(row.size()) << " columns, expected " <<
        static_cast<unsigned>(m_expectedCols) << endl;
    return RESULT_ERR_EOF;
  }
  result_t addFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
      string& errorDescription, const string filename, unsigned int lineNo) override {
    if (row.empty() || (m_expectedCols == 3) != subRows.empty()) {
      cout << "read line " << static_cast<unsigned>(baseLine + lineNo) << ": read error: got "
          << static_cast<unsigned>(row.size()) << "/3 main, " << static_cast<unsigned>(subRows.size())
          << (m_expectedCols == 3 ? "/0 sub" : "/>0 sub") << endl;
      return RESULT_ERR_EOF;
    }
    if (lineNo < 2 || lineNo >= 1+sizeof(resultlines)/sizeof(string[3])) {
      cout << "read line " << static_cast<unsigned>(baseLine + lineNo) << ": error invalid line" << endl;
      return RESULT_ERR_INVALID_ARG;
    }
    cout << "read line " << static_cast<unsigned>(baseLine + lineNo) << ": split OK" << endl;
    string resultline[3] = resultlines[lineNo - 1];
    if (row.empty()) {
      cout << "  result empty";
      if (resultline[0] == "") {
        cout << ": OK" << endl;
      } else {
        cout << ": error" << endl;
        return RESULT_ERR_INVALID_ARG;
      }
      return RESULT_EMPTY;
    }

    bool error = false;
    string colnames[3] = resultlines[0];
    map<string, string>& defaults = getDefaults()[""];
    for (size_t colIdx = 0; colIdx < 3; colIdx++) {
      string col = colnames[colIdx];
      string got = row[col] + defaults[col];
      string expect = resultline[colIdx];
      ostringstream type;
      type << "line " << static_cast<unsigned>(baseLine + lineNo) << " column \"" << col << "\"";
      bool match = got == expect;
      verify(false, type.str(), expect, match, expect, got);
      if (!match) {
        error = true;
      }
    }
    if (row.size() > 3) {
      ostringstream type;
      type << "line " << static_cast<unsigned>(baseLine + lineNo);
      verify(false, type.str(), "", false, "", "extra column");
      error = true;
    }

    for (size_t subIdx = 0; subIdx < subRows.size(); subIdx++) {
      string resultsubline[4] = resultsublines[lineNo - 1][subIdx];
      row = subRows[subIdx];
      if (row.empty()) {
        cout << "  sub " << subIdx << " result empty";
        if (resultline[0] == "") {
          cout << ": OK" << endl;
        } else {
          cout << ": error" << endl;
          return RESULT_ERR_INVALID_ARG;
        }
        return RESULT_EMPTY;
      }

      vector< map<string, string> >& subDefaults = getSubDefaults()[""];
      for (size_t colIdx = 0; colIdx < 2; colIdx++) {
        string col = resultsubline[colIdx*2];
        string got = row[col];
        if (subIdx < subDefaults.size()) {
          got += subDefaults[subIdx][col];
        }
        string expect = resultsubline[colIdx*2+1];
        ostringstream type;
        type << "line " << static_cast<unsigned>(baseLine + lineNo) << " sub " << subIdx << " column \"" << col << "\"";
        bool match = got == expect;
        verify(false, type.str(), expect, match, expect, got);
        if (!match) {
          error = true;
        }
      }
      if (row.size() > 2) {
        ostringstream type;
        type << "line " << static_cast<unsigned>(baseLine + lineNo) << " sub " << subIdx;
        verify(false, type.str(), "", false, "", "extra sub column");
        error = true;
      }
    }
    return error ? RESULT_ERR_INVALID_ARG : RESULT_OK;
  }
 private:
  size_t m_expectedCols;
};


int main(int argc, char** argv) {
  if (argc > 1) {
    NoopReader reader;
    for (int argpos = 1; argpos < argc; argpos++) {
      size_t hash = 0, size = 0;
      time_t time = 0;
      string errorDescription;
      result_t result = reader.readFromFile(argv[argpos], errorDescription, false, NULL, &hash, &size, &time);
      cout << argv[argpos] << " ";
      if (result != RESULT_OK) {
        cout << getResultCode(result) << ", " << errorDescription << endl;
        error = true;
        continue;
      }
      FileReader::formatHash(hash, cout);
      cout << " " << size << " " << time << endl;
    }
    return error ? 1 : 0;
  }
  baseLine = __LINE__+1;
  istringstream ifs(
    "col 1,col 2,col 3\n"
    "line 2 col 1,\"line 2 col 2\",\"line 2 \"\"col 3\"\";default of col 3\"\n"
    "line 4 col 1,\"line 4 col 2 part 1\n"
    "line 4 col 2 part 2\",line 4 col 3;default of col 3\n"
    ",,,\n"
    "line 6 col 1,,line 6 col 3;default of col 3\n"
    "line 8 col 1,\"line 8 col 2 part 1;\n"
    "line 8 col 2 part 2\",line 8 col 3;default of col 3\n"
  );
  size_t hash = 0, size = 0, expectHash = 0xfd58724c2984595d, expectSize = 301;
  TestReader reader{3};
  unsigned int lineNo = 0;
  vector<string> row;
  string errorDescription;
  while (ifs.peek() != EOF) {
    istringstream str;
    result_t result = reader.readLineFromStream(ifs, errorDescription, "", lineNo, row, true, &hash, &size);
    if (result != RESULT_OK) {
      cout << "  error " << getResultCode(result) << endl;
      error = true;
    }
  }
  if (hash == expectHash) {
    cout << "hash OK" << endl;
  } else {
    cout << "hash error: got 0x" << hex << hash << ", expected 0x" << expectHash << dec << endl;
    error = true;
  }
  if (size == expectSize) {
    cout << "size OK" << endl;
  } else {
    cout << "size error: got " << size << ", expected " << expectSize << endl;
    error = true;
  }

  ifs.clear();
  baseLine = __LINE__+1;
  ifs.str(
    "col 1,col 2,col 3,*subcol 1,subcol 2,*subcol 2,subcol 3\n"
    "line 2 col 1,\"line 2 col 2\",\"line 2 \"\"col 3\"\"\",line 2 subcol 1,line 2 subcol 2,line 2 subcol 2,line 2 subcol 3\n"
    "line 4 col 1,\"line 4 col 2 part 1\n"
    "line 4 col 2 part 2\",line 4 col 3,line 4 subcol 1,line 4 subcol 2,line 4 subcol 2,line 4 subcol 3\n"
    ",,,\n"
    "line 6 col 1,,line 6 col 3,line 6 subcol 1,line 6 subcol 2,line 6 subcol 2,line 6 subcol 3\n"
    "line 8 col 1,\"line 8 col 2 part 1;\n"
    "line 8 col 2 part 2\",line 8 col 3,line 8 subcol 1,line 8 subcol 2,line 8 subcol 2,line 8 subcol 3\n"
  );
  hash = 0, size = 0, expectHash = 0xec31914e32c6fb6f, expectSize = 527;
  TestReader reader2{7};
  lineNo = 0;
  map<string, string> defaults;
  reader2.getDefaults()[""]["col 3"] = ";default of col 3";
  vector< map<string, string> >& subDefaults = reader2.getSubDefaults()[""];
  subDefaults.resize(1);
  subDefaults[0]["subcol 2"] = ";default of sub 0 subcol 2";
  while (ifs.peek() != EOF) {
    istringstream str;
    result_t result = reader2.readLineFromStream(ifs, errorDescription, "", lineNo, row, true, &hash, &size);
    if (result != RESULT_OK) {
      cout << "  error " << getResultCode(result) << endl;
      error = true;
    }
  }
  if (hash == expectHash) {
    cout << "hash OK" << endl;
  } else {
    cout << "hash error: got 0x" << hex << hash << ", expected 0x" << expectHash << dec << endl;
    error = true;
  }
  if (size == expectSize) {
    cout << "size OK" << endl;
  } else {
    cout << "size error: got " << size << ", expected " << expectSize << endl;
    error = true;
  }

  return error ? 1 : 0;
}

/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2016-2025 John Baier <ebusd@ebusd.eu>
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
#include "lib/ebus/contrib/tem.h"
#include "lib/ebus/data.h"

using namespace ebusd;
using std::cout;
using std::endl;

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

class TestReader : public MappedFileReader {
 public:
  TestReader(DataFieldTemplates* templates, bool isSet, bool isMasterDest)
  : MappedFileReader::MappedFileReader(true), m_templates(templates), m_isSet(isSet), m_isMasterDest(isMasterDest),
    m_fields(nullptr) {}
  result_t getFieldMap(const string& preferLanguage, vector<string>* row, string* errorDescription) const override {
    if (row->empty()) {
      row->push_back("*name");
      row->push_back("part");
      row->push_back("type");
      row->push_back("divisor/values");
      row->push_back("unit");
      row->push_back("comment");
      return RESULT_OK;
    }
    if ((*row)[0][0] != '*') {
      return RESULT_ERR_INVALID_ARG;
    }
    return RESULT_OK;  // leave it to DataField::create
  }
  result_t addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
      vector< map<string, string> >* subRows, string* errorDescription, bool replace) override {
    if (!row->empty() || subRows->empty()) {
      cout << "read line " << static_cast<unsigned>(lineNo) << ": read error: got "
          << static_cast<unsigned>(row->size()) << "/0 main, " << static_cast<unsigned>(subRows->size())
          << "/>=3 sub" << endl;
      return RESULT_ERR_EOF;
    }
    cout << "read line " << static_cast<unsigned>(lineNo) << ": read OK" << endl;
    return DataField::create(m_isSet, false, m_isMasterDest, MAX_POS, m_templates, subRows, errorDescription, &m_fields);
  }
 private:
  DataFieldTemplates* m_templates;
  const bool m_isSet;
  const bool m_isMasterDest;
 public:
  const DataField* m_fields;
};

int main() {
  const DataType* type = DataTypeList::getInstance()->get("TEM_P");
  if (type == nullptr) {
    cout << "datatype not registered" << endl;
    return 1;
  }

  // entry: definition, decoded value, master data, slave data, flags
  // definition: name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
  unsigned int baseLine = __LINE__+1;
  string checks[][5] = {
    {"x,,TEM_P", "04-033", "10fe0700020421", "00", ""},
    {"x,,TEM_P", "00-000", "10fe0700020000", "00", ""},
    {"x,,TEM_P", "31-127", "10fe0700021f7f", "00", ""},
    {"x,,TEM_P", "-",      "10fe070002ffff", "00", ""},
    {"x,,TEM_P", "32-000", "10fe0700022000", "00", "Rw"},
    {"x,,TEM_P", "00-128", "10fe0700020080", "00", "Rw"},

    {"x,,TEM_P", "04-033", "1015070000", "022102", ""},
    {"x,,TEM_P", "00-000", "1015070000", "020000", ""},
    {"x,,TEM_P", "31-127", "1015070000", "02ff0f", ""},
    {"x,,TEM_P", "-",      "1015070000", "02ffff", ""},
    {"x,,TEM_P", "32-000", "1015070000", "022000", "Rw"},
    {"x,,TEM_P", "00-128", "1015070000", "020080", "Rw"},
  };
  DataFieldTemplates* templates = new DataFieldTemplates();
  unsigned int lineNo = 0;
  istringstream dummystr("#");
  string errorDescription;
  vector<string> row;
  templates->readLineFromStream(&dummystr, "inline", false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
  const DataField* fields = nullptr;
  for (unsigned int i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    string* check = checks[i];
    istringstream isstr(check[0]);
    string expectStr = check[1];
    MasterSymbolString mstr;
    result_t result = mstr.parseHex(check[2]);
    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": parse \"" << check[2] << "\" error: " << getResultCode(result) << endl;
      error = true;
      continue;
    }
    SlaveSymbolString sstr;
    result = sstr.parseHex(check[3]);
    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": parse \"" << check[3] << "\" error: " << getResultCode(result) << endl;
      error = true;
      continue;
    }
    string flags = check[4];
    bool isSet = flags.find('s') != string::npos;
    bool failedRead = flags.find('r') != string::npos;
    bool failedReadMatch = flags.find('R') != string::npos;
    bool failedWrite = flags.find('w') != string::npos;
    bool failedWriteMatch = flags.find('W') != string::npos;
    string item;

    if (fields != nullptr) {
      delete fields;
      fields = nullptr;
    }

    string errorDescription;
    TestReader reader{templates, isSet, mstr[1] == BROADCAST || isMaster(mstr[1])};
    lineNo = 0;
    dummystr.clear();
    dummystr.str("#");
    result = reader.readLineFromStream(&dummystr, "inline", false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": reader header error: " << getResultCode(result) << ", " << errorDescription
          << endl;
      error = true;
      continue;
    }
    lineNo = baseLine + i;
    result = reader.readLineFromStream(&isstr, __FILE__, false, &lineNo, &row, &errorDescription, false, nullptr, nullptr);
    fields = reader.m_fields;

    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": create error: " << getResultCode(result) << ", " << errorDescription << endl;
      error = true;
      continue;
    }
    if (fields == nullptr) {
      cout << "\"" << check[0] << "\": create error: nullptr" << endl;
      error = true;
      continue;
    }
    cout << "\"" << check[0] << "\"=\"";
    fields->dump(false, OF_NONE, &cout);
    cout << "\": create OK" << endl;

    ostringstream output;
    MasterSymbolString writeMstr;
    result = writeMstr.parseHex(mstr.getStr().substr(0, 10));
    if (result != RESULT_OK) {
      cout << "  parse \"" << mstr.getStr().substr(0, 10) << "\" error: " << getResultCode(result) << endl;
      error = true;
    }
    SlaveSymbolString writeSstr;
    result = writeSstr.parseHex(sstr.getStr().substr(0, 2));
    if (result != RESULT_OK) {
      cout << "  parse \"" << sstr.getStr().substr(0, 2) << "\" error: " << getResultCode(result) << endl;
      error = true;
    }
    result = fields->read(mstr, 0, false, nullptr, -1, OF_NONE, -1, &output);
    if (result >= RESULT_OK) {
      result = fields->read(sstr, 0, !output.str().empty(), nullptr, -1, OF_NONE, -1, &output);
    }
    if (failedRead) {
      if (result >= RESULT_OK) {
        cout << "  failed read " << fields->getName(-1) << " >" << check[2] << " " << check[3]
             << "< error: unexpectedly succeeded" << endl;
        error = true;
      } else {
        cout << "  failed read " << fields->getName(-1) << " >" << check[2] << " " << check[3]
             << "< OK" << endl;
      }
    } else if (result < RESULT_OK) {
      cout << "  read " << fields->getName(-1) << " >" << check[2] << " " << check[3]
           << "< error: " << getResultCode(result) << endl;
      error = true;
    } else {
      bool match = strcasecmp(output.str().c_str(), expectStr.c_str()) == 0;
      verify(failedReadMatch, "read", check[2], match, expectStr, output.str());
    }

    istringstream input(expectStr);
    result = fields->write(UI_FIELD_SEPARATOR, 0, &input, &writeMstr, nullptr);
    if (result >= RESULT_OK) {
      result = fields->write(UI_FIELD_SEPARATOR, 0, &input, &writeSstr, nullptr);
    }
    if (failedWrite) {
      if (result >= RESULT_OK) {
        cout << "  failed write " << fields->getName(-1) << " >"
            << expectStr << "< error: unexpectedly succeeded" << endl;
        error = true;
      } else {
        cout << "  failed write " << fields->getName(-1) << " >"
            << expectStr << "< OK" << endl;
      }
    } else if (result < RESULT_OK) {
      cout << "  write " << fields->getName(-1) << " >" << expectStr
          << "< error: " << getResultCode(result) << endl;
      error = true;
    } else {
      bool match = mstr == writeMstr && sstr == writeSstr;
      verify(failedWriteMatch, "write", expectStr, match, mstr.getStr() + " " + sstr.getStr(),
          writeMstr.getStr() + " " + writeSstr.getStr());
    }
    delete fields;
    fields = nullptr;
  }

  delete templates;
  return error ? 1 : 0;
}

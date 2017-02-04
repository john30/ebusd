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

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "lib/ebus/contrib/tem.h"
#include "lib/ebus/data.h"

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

int main() {
  DataType* type = DataTypeList::getInstance()->get("TEM_P");
  if (type == NULL) {
    cout << "datatype not registered" << endl;
    return 1;
  }

  string checks[][5] = {
    // entry: definition, decoded value, master data, slave data, flags
    // definition: name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
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
  DataField* fields = NULL;
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    string check[5] = checks[i];
    istringstream isstr(check[0]);
    string expectStr = check[1];
    SymbolString mstr(false);
    result_t result = mstr.parseHex(check[2]);
    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": parse \"" << check[2] << "\" error: " << getResultCode(result) << endl;
      error = true;
      continue;
    }
    SymbolString sstr(false);
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
    vector<string> entries;

    while (getline(isstr, item, FIELD_SEPARATOR))
      entries.push_back(item);

    if (fields != NULL) {
      delete fields;
      fields = NULL;
    }
    vector<string>::iterator it = entries.begin();
    result = DataField::create(it, entries.end(), templates, fields, isSet, false,
        (mstr[1] == BROADCAST || isMaster(mstr[1])));
    if (result != RESULT_OK) {
      cout << "\"" << check[0] << "\": create error: " << getResultCode(result) << endl;
      error = true;
      continue;
    }
    if (fields == NULL) {
      cout << "\"" << check[0] << "\": create error: NULL" << endl;
      error = true;
      continue;
    }
    if (it != entries.end()) {
      cout << "\"" << check[0] << "\": create error: trailing input" << endl;
      error = true;
      continue;
    }
    cout << "\"" << check[0] << "\"=\"";
    fields->dump(cout);
    cout << "\": create OK" << endl;

    ostringstream output;
    SymbolString writeMstr(false);
    result = writeMstr.parseHex(mstr.getDataStr(true, false).substr(0, 10));
    if (result != RESULT_OK) {
      cout << "  parse \"" << mstr.getDataStr(true, false).substr(0, 10) << "\" error: " << getResultCode(result)
          << endl;
      error = true;
    }
    SymbolString writeSstr(false);
    result = writeSstr.parseHex(sstr.getDataStr(true, false).substr(0, 2));
    if (result != RESULT_OK) {
      cout << "  parse \"" << sstr.getDataStr(true, false).substr(0, 2) << "\" error: " << getResultCode(result)
          << endl;
      error = true;
    }
    result = fields->read(pt_masterData, mstr, 0, output, 0, -1, false);
    if (result >= RESULT_OK) {
      result = fields->read(pt_slaveData, sstr, 0, output, 0, -1, !output.str().empty());
    }
    if (failedRead) {
      if (result >= RESULT_OK) {
        cout << "  failed read " << fields->getName() << " >" << check[2] << " " << check[3]
             << "< error: unexpectedly succeeded" << endl;
        error = true;
      } else {
        cout << "  failed read " << fields->getName() << " >" << check[2] << " " << check[3]
             << "< OK" << endl;
      }
    } else if (result < RESULT_OK) {
      cout << "  read " << fields->getName() << " >" << check[2] << " " << check[3]
           << "< error: " << getResultCode(result) << endl;
      error = true;
    } else {
      bool match = strcasecmp(output.str().c_str(), expectStr.c_str()) == 0;
      verify(failedReadMatch, "read", check[2], match, expectStr, output.str());
    }

    istringstream input(expectStr);
    result = fields->write(input, pt_masterData, writeMstr, 0);
    if (result >= RESULT_OK) {
      result = fields->write(input, pt_slaveData, writeSstr, 0);
    }
    if (failedWrite) {
      if (result >= RESULT_OK) {
        cout << "  failed write " << fields->getName() << " >"
            << expectStr << "< error: unexpectedly succeeded" << endl;
        error = true;
      } else {
        cout << "  failed write " << fields->getName() << " >"
            << expectStr << "< OK" << endl;
      }
    } else if (result < RESULT_OK) {
      cout << "  write " << fields->getName() << " >" << expectStr
          << "< error: " << getResultCode(result) << endl;
      error = true;
    } else {
      bool match = mstr == writeMstr && sstr == writeSstr;
      verify(failedWriteMatch, "write", expectStr, match, mstr.getDataStr(true, false) + " "
          + sstr.getDataStr(true, false), writeMstr.getDataStr(true, false) + " " + writeSstr.getDataStr(true, false));
    }
    delete fields;
    fields = NULL;
  }

  delete templates;
  return error ? 1 : 0;
}

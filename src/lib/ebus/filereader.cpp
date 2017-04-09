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

#include "lib/ebus/filereader.h"
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <climits>
#include <fstream>
#include <functional>

namespace ebusd {

using std::ifstream;
using std::ostringstream;
using std::cout;
using std::endl;
using std::setw;
using std::dec;


result_t FileReader::readFromFile(const string filename, string& errorDescription, bool verbose,
    map<string, string>* defaults, size_t* hash, size_t* size, time_t* time) {
  struct stat st;
  if (stat(filename.c_str(), &st) != 0) {
    errorDescription = filename;
    return RESULT_ERR_NOTFOUND;
  }
  if (S_ISDIR(st.st_mode)) {
    errorDescription = filename+" is a directory";
    return RESULT_ERR_NOTFOUND;
  }
  ifstream ifs;
  ifs.open(filename.c_str(), ifstream::in);
  if (!ifs.is_open()) {
    errorDescription = filename;
    return RESULT_ERR_NOTFOUND;
  }
  if (hash) {
    *hash = 0;
  }
  if (size) {
    *size = 0;
  }
  if (time) {
    *time = st.st_mtime;
  }
  unsigned int lineNo = 0;
  vector<string> row;
  result_t result = RESULT_OK;
  while (ifs.peek() != EOF && result == RESULT_OK) {
    result = readLineFromStream(ifs, errorDescription, filename, lineNo, row, verbose, hash, size);
  }
  ifs.close();
  return result;
}

result_t FileReader::readLineFromStream(istream& stream, string& errorDescription,
    const string filename, unsigned int& lineNo, vector<string>& row, bool verbose,
    size_t* hash, size_t* size) {
  result_t result;
  if (!splitFields(stream, row, lineNo, hash, size)) {
    errorDescription = "blank line";
    result = RESULT_ERR_EOF;
  } else {
    errorDescription = "";
    result = addFromFile(row, errorDescription, filename, lineNo);
  }
  if (result != RESULT_OK) {
    if (!verbose) {
      ostringstream error;
      error << filename << ":" << lineNo;
      if (errorDescription.length() > 0) {
        error << ": " << errorDescription;
      }
      errorDescription = error.str();
      return result;
    }
    if (!errorDescription.empty()) {
      cout << "error reading " << filename << ":" << lineNo << ": " << getResultCode(result) << ", "
          << errorDescription << endl;
    }
  } else if (!verbose) {
    errorDescription = "";
  }
  return result;
}

void FileReader::trim(string& str) {
  size_t pos = str.find_first_not_of(" \t");
  if (pos != string::npos) {
    str.erase(0, pos);
  }
  pos = str.find_last_not_of(" \t");
  if (pos != string::npos) {
    str.erase(pos+1);
  }
}

void FileReader::tolower(string& str) {
  transform(str.begin(), str.end(), str.begin(), ::tolower);
}

static std::hash<string> hashFunction;

bool FileReader::splitFields(istream& ifs, vector<string>& row, unsigned int& lineNo,
    size_t* hash, size_t* size) {
  row.clear();
  string line;
  bool quotedText = false, wasQuoted = false;
  ostringstream field;
  char prev = FIELD_SEPARATOR;
  bool empty = true, read = false;
  while (getline(ifs, line)) {
    read = true;
    lineNo++;
    trim(line);
    size_t length = line.size();
    if (size) {
      *size += length + 1;  // normalized with trailing endl
    }
    if (hash) {
      // TODO ensure 32 bit machine produces same result
      *hash ^= (hashFunction(line) << 1) ^ (length << (7 * (lineNo % 5)));
    }
    if (!quotedText && (length == 0 || line[0] == '#' || (line.length() > 1 && line[0] == '/' && line[1] == '/'))) {
      if (lineNo == 1) {
        break;  // keep empty first line for applying default header
      }
      continue;  // skip empty lines and comments
    }
    for (size_t pos = 0; pos < length; pos++) {
      char ch = line[pos];
      switch (ch) {
      case FIELD_SEPARATOR:
        if (quotedText) {
          field << ch;
        } else {
          string str = field.str();
          trim(str);
          empty &= str.empty();
          row.push_back(str);
          field.str("");
          wasQuoted = false;
        }
        break;
      case TEXT_SEPARATOR:
        if (prev == TEXT_SEPARATOR && !quotedText) {  // double dquote
          field << ch;
          quotedText = true;
        } else if (quotedText) {
          quotedText = false;
        } else if (prev == FIELD_SEPARATOR) {
          quotedText = wasQuoted = true;
        } else {
          field << ch;
        }
        break;
      case '\r':
        break;
      default:
        if (prev == TEXT_SEPARATOR && !quotedText && wasQuoted) {
          field << TEXT_SEPARATOR;  // single dquote in the middle of formerly quoted text
          quotedText = true;
        } else if (quotedText && pos == 0 && field.tellp() > 0 && *(field.str().end()-1) != VALUE_SEPARATOR) {
          field << VALUE_SEPARATOR;  // add separator in between multiline field parts
        }
        field << ch;
        break;
      }
      prev = ch;
    }
    if (!quotedText) {
      break;
    }
  }
  string str = field.str();
  trim(str);
  if (empty && str.empty()) {
    row.clear();
    return read;
  }
  row.push_back(str);
  return true;
}


result_t MappedFileReader::readFromFile(const string filename, string& errorDescription, bool verbose,
    map<string, string>* defaults, size_t* hash, size_t* size, time_t* time) {
  m_mutex.lock();
  m_columnNames.clear();
  m_lastDefaults.clear();
  m_lastSubDefaults.clear();
  if (defaults) {
    m_lastDefaults[""] = *defaults;
  }
  size_t lastSep = filename.find_last_of('/');
  string defaultsPart = lastSep == string::npos ? filename : filename.substr(lastSep+1);
  extractDefaultsFromFilename(defaultsPart, m_lastDefaults[""]);
  result_t result = FileReader::readFromFile(filename, errorDescription, verbose, defaults, hash, size, time);
  m_mutex.unlock();
  return result;
}

result_t MappedFileReader::addFromFile(vector<string>& row, string& errorDescription,
    const string filename, unsigned int lineNo) {
  result_t result;
  if (lineNo == 1) {  // first line defines column names
    result = getFieldMap(row, errorDescription);
    if (result != RESULT_OK) {
      return result;
    }
    if (row.empty()) {
      errorDescription = "missing field map";
      return RESULT_ERR_EOF;
    }
    m_columnNames = row;
    return RESULT_OK;
  }
  if (row.empty()) {
    return RESULT_OK;
  }
  if (m_columnNames.empty()) {
    errorDescription = "missing field map";
    return RESULT_ERR_INVALID_ARG;
  }
  map<string, string> rowMapped;
  vector< map<string, string> > subRowsMapped;
  vector<string>::iterator it = row.begin();
  bool isDefault = m_supportsDefaults && !row[0].empty() && row[0][0] == '*';
  if (isDefault) {
    row[0] = row[0].substr(1);
  }
  size_t lastRepeatStart = UINT_MAX;
  map<string, string>* lastMappedRow = &rowMapped;
  bool empty = true;
  for (size_t colIdx = 0, colNameIdx = 0; colIdx < row.size(); colIdx++, colNameIdx++) {
    if (colNameIdx >= m_columnNames.size()) {
      if (lastRepeatStart == UINT_MAX) {
        errorDescription = "named columns exceeded";
        return RESULT_ERR_INVALID_ARG;
      }
      colNameIdx = lastRepeatStart;
    }
    string columnName = m_columnNames[colNameIdx];
    if (!columnName.empty() && columnName[0] == '*') {  // marker for next entry
      if (empty) {
        lastMappedRow->clear();
      }
      if (!empty || lastMappedRow == &rowMapped) {
        subRowsMapped.resize(subRowsMapped.size() + 1);
        lastMappedRow = &subRowsMapped[subRowsMapped.size() - 1];
      }
      columnName = columnName.substr(1);
      lastRepeatStart = colNameIdx;
      empty = true;
    }
    string value = row[colIdx];
    empty &= value.empty();
    (*lastMappedRow)[columnName] = value;
  }
  if (empty) {
    lastMappedRow->clear();
    if (lastMappedRow != &rowMapped) {
      subRowsMapped.resize(subRowsMapped.size() - 1);
    }
  }
  if (isDefault) {
    return addDefaultFromFile(rowMapped, subRowsMapped, errorDescription, filename, lineNo);
  }
  return addFromFile(rowMapped, subRowsMapped, errorDescription, filename, lineNo);
}

string MappedFileReader::combineRow(const map<string, string>& row) {
  ostringstream ostream;
  bool first = true;
  for (auto entry : row) {
    if (first) {
      first = false;
    } else {
      ostream << ", ";
    }
    ostream << entry.first << ": \"" << entry.second << "\"";
  }
  return ostream.str();
}

}  // namespace ebusd

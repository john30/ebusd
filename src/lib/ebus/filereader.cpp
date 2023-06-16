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


istream* FileReader::openFile(const string& filename, string* errorDescription, time_t* time) {
  struct stat st;
  if (stat(filename.c_str(), &st) != 0) {
    *errorDescription = filename;
    return nullptr;
  }
  if (S_ISDIR(st.st_mode)) {
    *errorDescription = filename+" is a directory";
    return nullptr;
  }
  ifstream* stream = new ifstream();
  stream->open(filename.c_str(), ifstream::in);
  if (!stream->is_open()) {
    *errorDescription = filename;
    delete(stream);
    return nullptr;
  }
  if (time) {
    *time = st.st_mtime;
  }
  return stream;
}

result_t FileReader::readFromStream(istream* stream, const string& filename, const time_t& mtime, bool verbose,
    map<string, string>* defaults, string* errorDescription, bool replace, size_t* hash, size_t* size) {
  if (hash) {
    *hash = 0;
  }
  if (size) {
    *size = 0;
  }
  unsigned int lineNo = 0;
  vector<string> row;
  result_t result = RESULT_OK;
  while (stream->peek() != EOF && result == RESULT_OK) {
    result = readLineFromStream(stream, filename, verbose, &lineNo, &row, errorDescription, replace, hash, size);
  }
  return result;
}

result_t FileReader::readLineFromStream(istream* stream, const string& filename, bool verbose,
    unsigned int* lineNo, vector<string>* row, string* errorDescription, bool replace, size_t* hash, size_t* size) {
  result_t result;
  if (!splitFields(stream, row, lineNo, hash, size)) {
    *errorDescription = "blank line";
    result = RESULT_ERR_EOF;
  } else {
    *errorDescription = "";
    result = addFromFile(filename, *lineNo, row, errorDescription, replace);
  }
  if (result != RESULT_OK) {
    if (!errorDescription->empty()) {
      string error;
      formatError(filename, *lineNo, result, *errorDescription, &error);
      *errorDescription = error;
      if (verbose) {
        cout << error << endl;
      }
    } else if (!verbose) {
      return formatError(filename, *lineNo, result, "", errorDescription);
    }
  } else if (!verbose) {
    *errorDescription = "";
  }
  return result;
}

void FileReader::trim(string* str) {
  size_t pos = str->find_first_not_of(" \t");
  if (pos != string::npos) {
    str->erase(0, pos);
  }
  pos = str->find_last_not_of(" \t");
  if (pos != string::npos) {
    str->erase(pos+1);
  }
}

void FileReader::tolower(string* str) {
  transform(str->begin(), str->end(), str->begin(), ::tolower);
}

bool FileReader::matches(const string& input, const string& search, bool ignoreCase, bool searchIsLower) {
  if (search.empty()) {
    return true;  // empty pattern matches everything
  }
  if (ignoreCase) {
    string inputSub = input;
    tolower(&inputSub);
    if (searchIsLower) {
      return matches(inputSub, search, false, true);
    }
    string searchSub = search;
    tolower(&searchSub);
    return matches(inputSub, searchSub, false, true);
  }
  // walk through alternatives
  size_t from = 0;
  bool found = false;
  do {
    size_t to = search.find('|', from);
    if (to == string::npos) {
      to = search.length();
    } else {
      found = true;
    }
    if (from == to) {
      return true;  // empty pattern matches everything
    }
    size_t nextStart = to+1;
    bool matchStart = search[from] == '^';
    if (matchStart) {
      from++;
    }
    bool matchEnd = from < to && search[to-1] == '$';
    if (matchEnd) {
      to--;
    }
    if (matchEnd && matchStart && from == to) {  // pattern is "^$"
      if (input.empty()) {
        return true;
      }
    } else {
      string prefix = search.substr(from, to-from);
      size_t star = prefix.find('*');
      size_t checkEnd = input.length();
      if (star != string::npos) {
        string suffix = prefix.substr(star + 1);
        prefix = prefix.substr(0, star);
        if (suffix.empty()) {
          // empty suffix matches everything
        } else {
          if (checkEnd < suffix.length()) {
            checkEnd = string::npos;  // no-match
          } else {
            checkEnd -= suffix.length();
            if (matchEnd) {
              if (input.find(suffix, checkEnd) == string::npos) {
                checkEnd = string::npos;  // no-match
              }
            } else {
              checkEnd = input.rfind(suffix, checkEnd);
            }
          }
        }
        matchEnd = false;  // prefix is no longer required to match at the end
      }  // else: no star, check prefix only
      if (checkEnd != string::npos) {
        if (prefix.empty()) {
          return true;  // empty prefix matches everything
        }
        if (prefix.length() <= checkEnd) {
          if (matchStart) {
            if (input.substr(0, prefix.length()) == prefix && (!matchEnd || prefix.length() == checkEnd)) {
              return true;
            }
          } else {
            string remain = input.substr(0, checkEnd);
            if (matchEnd) {
              if (remain.find(prefix, checkEnd-prefix.length()) != string::npos) {
                return true;
              }
            } else if (remain.find(prefix) != string::npos) {
              return true;
            }
          }
        }  // else: prefix is longer than remainder
      }
    }
    from = nextStart;
  } while (from < search.length()+(found?1:0));
  return false;
}

static size_t hashFunction(const string& str) {
  size_t hash = 0;
  for (unsigned char c : str) {
    hash = (31 * hash) ^ c;
  }
  return hash;
}

bool FileReader::splitFields(istream* stream, vector<string>* row, unsigned int* lineNo,
    size_t* hash, size_t* size, bool clear) {
  if (clear) {
    row->clear();
  }
  string line;
  bool quotedText = false, wasQuoted = false;
  ostringstream field;
  char prev = FIELD_SEPARATOR;
  bool empty = true, read = false;
  while (getline(*stream, line)) {
    read = true;
    ++(*lineNo);
    trim(&line);
    size_t length = line.size();
    if (size) {
      *size += length + 1;  // normalized with trailing endl
    }
    if (hash) {
      *hash ^= (hashFunction(line) ^ (length << (7 * (*lineNo % 5)))) & 0xffffffff;
    }
    if (!quotedText && (length == 0 || line[0] == '#' || (line.length() > 1 && line[0] == '/' && line[1] == '/'))) {
      if (*lineNo == 1) {
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
          trim(&str);
          empty &= str.empty();
          row->push_back(str);
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
  trim(&str);
  if (empty && str.empty()) {
    row->clear();
    return read;
  }
  row->push_back(str);
  return true;
}

result_t FileReader::formatError(const string& filename, unsigned int lineNo, result_t result,
    const string& error, string* errorDescription) {
  ostringstream str;
  if (!errorDescription->empty()) {
    str << *errorDescription << ", ";
  }
  str << filename << ":" << lineNo << ": " << getResultCode(result);
  if (!error.empty()) {
    str << ", " << error;
  }
  *errorDescription = str.str();
  return result;
}


const string MappedFileReader::normalizeLanguage(const string& lang) {
  string normLang = lang;
  tolower(&normLang);
  if (normLang.size() > 2) {
    size_t pos = normLang.find('.');
    if (pos == string::npos) {
      pos = normLang.size();
    }
    size_t strip = normLang.find('_');
    if (strip == string::npos || strip > pos) {
      strip = pos;
    }
    if (strip > 2) {
      strip = 2;
    }
    return normLang.substr(0, strip);
  }
  return normLang;
}

result_t MappedFileReader::readFromStream(istream* stream, const string& filename, const time_t& mtime, bool verbose,
    map<string, string>* defaults, string* errorDescription, bool replace, size_t* hash, size_t* size) {
  m_mutex.lock();
  m_columnNames.clear();
  m_lastDefaults.clear();
  m_lastSubDefaults.clear();
  if (defaults) {
    m_lastDefaults[""] = *defaults;
  }
  size_t lastSep = filename.find_last_of('/');
  string defaultsPart = lastSep == string::npos ? filename : filename.substr(lastSep+1);
  extractDefaultsFromFilename(defaultsPart, &m_lastDefaults[""]);
  result_t result
  = FileReader::readFromStream(stream, filename, mtime, verbose, defaults, errorDescription, replace, hash, size);
  m_mutex.unlock();
  return result;
}

result_t MappedFileReader::addFromFile(const string& filename, unsigned int lineNo, vector<string>* row,
    string* errorDescription, bool replace) {
  result_t result;
  if (lineNo == 1) {  // first line defines column names
    result = getFieldMap(m_preferLanguage, row, errorDescription);
    if (result != RESULT_OK) {
      return result;
    }
    if (row->empty()) {
      *errorDescription = "missing field map";
      return RESULT_ERR_EOF;
    }
    m_columnNames = *row;
    return RESULT_OK;
  }
  if (row->empty()) {
    return RESULT_OK;
  }
  if (m_columnNames.empty()) {
    *errorDescription = "missing field map";
    return RESULT_ERR_INVALID_ARG;
  }
  map<string, string> rowMapped;
  vector< map<string, string> > subRowsMapped;
  bool isDefault = m_supportsDefaults && !(*row)[0].empty() && (*row)[0][0] == '*';
  if (isDefault) {
    (*row)[0].erase(0, 1);
  }
  size_t lastRepeatStart = UINT_MAX;
  map<string, string>* lastMappedRow = &rowMapped;
  bool empty = true;
  for (size_t colIdx = 0, colNameIdx = 0; colIdx < row->size(); colIdx++, colNameIdx++) {
    if (colNameIdx >= m_columnNames.size()) {
      if (lastRepeatStart == UINT_MAX) {
        *errorDescription = "named columns exceeded";
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
    } else if (columnName == SKIP_COLUMN) {
      continue;
    }
    string value = (*row)[colIdx];
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
    return addDefaultFromFile(filename, lineNo, &rowMapped, &subRowsMapped, errorDescription);
  }
  return addFromFile(filename, lineNo, &rowMapped, &subRowsMapped, errorDescription, replace);
}

const string MappedFileReader::combineRow(const map<string, string>& row) {
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

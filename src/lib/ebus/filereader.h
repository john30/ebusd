/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2016 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_FILEREADER_H_
#define LIB_EBUS_FILEREADER_H_

#include <climits>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include "symbol.h"
#include "result.h"

/** @file filereader.h
 * Helper class and constants for reading configuration files.
 *
 * The @a FileReader template class allows to read CSV compliant text files
 * while splitting each read line into fields.
 * It also supports special treatment of comment lines starting with a "#", as
 * well as so called "default values" indicated by the first field starting
 * with a "*" symbol.
 */

namespace ebusd {

using std::string;
using std::ostream;
using std::ostringstream;
using std::istream;
using std::istringstream;
using std::ifstream;
using std::cout;
using std::endl;

/** the separator character used between fields. */
#define FIELD_SEPARATOR ','

/** the separator character used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR '"'

/** the separator character as string used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR_STR "\""

/** the separator character used between multiple values (in CSV only). */
#define VALUE_SEPARATOR ';'

extern void printErrorPos(ostream& out, vector<string>::iterator begin, const vector<string>::iterator end,
    vector<string>::iterator pos, string filename, size_t lineNo, result_t result);

extern unsigned int parseInt(const char* str, int base, const unsigned int minValue, const unsigned int maxValue,
    result_t& result, unsigned int* length);

/**
 * An abstract class that support reading definitions from a file.
 */
class FileReader {
 public:
  /**
   * Construct a new instance.
   */
  explicit FileReader(bool supportsDefaults)
      : m_supportsDefaults(supportsDefaults) {}

  /**
   * Destructor.
   */
  virtual ~FileReader() {}

  /**
   * Read the definitions from a file.
   * @param filename the name of the file being read.
   * @param verbose whether to verbosely log problems.
   * @param defaultDest the default destination address (may be overwritten by file name), or empty.
   * @param defaultCircuit the default circuit name (may be overwritten by file name), or empty.
   * @param defaultSuffix the default circuit name suffix (starting with a ".", may be overwritten by file name, or empty.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readFromFile(const string filename, bool verbose = false,
    string defaultDest = "", string defaultCircuit = "", string defaultSuffix = "") {
    ifstream ifs;
    ifs.open(filename.c_str(), ifstream::in);
    if (!ifs.is_open()) {
      m_lastError = filename;
      return RESULT_ERR_NOTFOUND;
    }
    size_t lastSep = filename.find_last_of('/');
    if (lastSep != string::npos) {  // potential destination address, matches "^ZZ."
      // extract defaultDest, defaultCircuit, defaultSuffix from filename:
      // ZZ.IDENT[.CIRCUIT][.SUFFIX].*csv
      unsigned char checkDest;
      string checkIdent, useCircuit, useSuffix;
      unsigned int checkSw, checkHw;
      if (extractDefaultsFromFilename(filename.substr(lastSep+1), checkDest, checkIdent, useCircuit, useSuffix,
          checkSw, checkHw)) {
        defaultDest = filename.substr(lastSep+1, 2);
        if (!useCircuit.empty()) {
          defaultCircuit = useCircuit;
        }
        if (!useSuffix.empty()) {
          defaultSuffix = useSuffix;
        }
      }
    }
    unsigned int lineNo = 0;
    vector<string> row;
    vector< vector<string> > defaults;
    while (splitFields(ifs, row, lineNo)) {
      if (row.empty()) {
        continue;
      }
      result_t result;
      vector<string>::iterator it = row.begin();
      const vector<string>::iterator end = row.end();
      if (m_supportsDefaults) {
        if (row[0][0] == '*') {
          row[0] = row[0].substr(1);
          result = addDefaultFromFile(defaults, row, it, defaultDest, defaultCircuit, defaultSuffix, filename, lineNo);
          if (result == RESULT_OK) {
            continue;
          }
        } else {
          result = addFromFile(it, end, &defaults, defaultDest, defaultCircuit, defaultSuffix, filename, lineNo);
        }
      } else {
        result = addFromFile(it, end, NULL, defaultDest, defaultCircuit, defaultSuffix, filename, lineNo);
      }
      if (result != RESULT_OK) {
        if (!verbose) {
          ifs.close();
          ostringstream error;
          error << filename << ":" << static_cast<unsigned>(lineNo);
          if (m_lastError.length() > 0) {
            error << ": " << m_lastError;
          }
          m_lastError = error.str();
          return result;
        }
        if (m_lastError.length() > 0) {
          cout << m_lastError << endl;
        }
        printErrorPos(cout, row.begin(), end, it, filename, lineNo, result);
      } else if (!verbose) {
        m_lastError = "";
      }
    }

    ifs.close();
    return RESULT_OK;
  }

  /**
   * Return a @a string describing the last error position.
   * @return a @a string describing the last error position.
   */
  virtual string getLastError() { return m_lastError; }

  /**
   * Add a default row that was read from a file.
   * @param defaults the list to add the default row to.
   * @param row the default row (initial star char removed).
   * @param begin an iterator to the first column of the default row to read (for error reporting).
   * @param defaultDest the valid destination address extracted from the file name (from ZZ part), or empty.
   * @param defaultCircuit the valid circuit name extracted from the file name (from IDENT part), or empty.
   * @param defaultSuffix the valid circuit name suffix (starting with a ".") extracted from the file name (number after after IDENT part and "."), or empty.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addDefaultFromFile(vector< vector<string> >& defaults, vector<string>& row,
      vector<string>::iterator& begin, string defaultDest, string defaultCircuit, string defaultSuffix,
      const string& filename, unsigned int lineNo) {
    defaults.push_back(row);
    begin = row.end();
    return RESULT_OK;
  }

  /**
   * Add a definition that was read from a file.
   * @param begin an iterator to the first column of the definition row to read.
   * @param end the end iterator of the definition row to read.
   * @param defaults all previously read default rows (initial star char removed), or NULL if not supported.
   * @param defaultDest the valid destination address extracted from the file name (from ZZ part), or empty.
   * @param defaultCircuit the valid circuit name extracted from the file name (from IDENT part), or empty.
   * @param defaultSuffix the valid circuit name suffix (starting with a ".") extracted from the file name (number after after IDENT part and "."), or empty.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end,
    vector< vector<string> >* defaults, const string& defaultDest, const string& defaultCircuit,
    const string& defaultSuffix, const string& filename, unsigned int lineNo) = 0;

  /**
   * Left and right trim the string.
   * @param str the @a string to trim.
   */
  static void trim(string& str) {
    size_t pos = str.find_first_not_of(" \t");
    if (pos != string::npos) {
      str.erase(0, pos);
    }
    pos = str.find_last_not_of(" \t");
    if (pos != string::npos) {
      str.erase(pos+1);
    }
  }

  /**
   * Convert all upper case characters in the string to lower case.
   * @param str the @a string to convert.
   */
  static void tolower(string& str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
  }

  /**
   * Split the next line(s) from the @a istring into fields.
   * @param ifs the @a istream to read from.
   * @param row the @a vector to which to add the fields. This will be empty for completely empty and comment lines.
   * @param lineNo the current line number (incremented with each line read).
   * @return true if there are more lines to read, false when there are no more lines left.
   */
  static bool splitFields(istream& ifs, vector<string>& row, unsigned int& lineNo) {
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

      size_t length = line.length();
      if (!quotedText && (length == 0 || line[0] == '#' || (line.length() > 1 && line[0] == '/' && line[1] == '/'))) {
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
            field << VALUE_SEPARATOR;
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

  /**
   * Extract default values from the file name.
   * @param name the file name (without path) in the form "ZZ[.IDENT][.CIRCUIT][.SUFFIX][.SWXXXX][.HWXXXX][.*].csv".
   * @param dest the output destination address ZZ (hex digits).
   * @param ident the identification part IDENT (up to 5 characters, set to empty if not present).
   * @param circuit the circuit part CIRCUIT (set to IDENT if not present).
   * @param suffix the suffix part SUFFIX including the leading dot (decimal digit, set to empty if not present).
   * @param software the software version part SWXXXX (BCD digits, set to @a UINT_MAX if not present).
   * @param hardware the hardware version part HWXXXX (BCD digits, set to @a UINT_MAX if not present).
   * @return true if at least the address and the identification part were extracted, false otherwise.
   */
  static bool extractDefaultsFromFilename(string name, unsigned char& dest, string& ident, string& circuit,
    string& suffix, unsigned int& software, unsigned int& hardware) {
    ident = circuit = suffix = "";
    software = hardware = UINT_MAX;
    if (name.length() > 4 && name.substr(name.length()-4) == ".csv") {
      name = name.substr(0, name.length()-3);  // including trailing "."
    }
    size_t pos = name.find('.');
    if (pos != 2) {
      return false;  // missing "ZZ."
    }
    result_t result = RESULT_OK;
    dest = (unsigned char)parseInt(name.substr(0, pos).c_str(), 16, 0, 0xff, result, NULL);
    if (result != RESULT_OK || !isValidAddress(dest)) {
      return false;  // invalid "ZZ"
    }
    name.erase(0, pos);
    if (name.length() > 1) {
      pos = name.rfind(".SW");  // check for ".SWxxxx."
      if (pos != string::npos && name.find(".", pos+1) == pos+7) {
        software = parseInt(name.substr(pos+3, 4).c_str(), 10, 0, 9999, result, NULL);
        if (result != RESULT_OK) {
          return false;  // invalid "SWxxxx"
        }
        name.erase(pos, 7);
      }
    }
    if (name.length() > 1) {
      pos = name.rfind(".HW");  // check for ".HWxxxx."
      if (pos != string::npos && name.find(".", pos+1) == pos+7) {
        hardware = parseInt(name.substr(pos+3, 4).c_str(), 10, 0, 9999, result, NULL);
        if (result != RESULT_OK) {
          return false;  // invalid "HWxxxx"
        }
        name.erase(pos, 7);
      }
    }
    if (name.length() > 1) {
      pos = name.find('.', 1);  // check for ".IDENT."
      if (pos != string::npos && pos >= 1 && pos <= 6) {
        // up to 5 chars between two "."s, immediately after "ZZ.", or ".."
        ident = circuit = name.substr(1, pos-1);
        name.erase(0, pos);
        pos = name.find('.', 1);  // check for ".CIRCUIT."
        if (pos != string::npos && (pos>2 || name[1]<'0' || name[1]>'9')) {
          circuit = name.substr(1, pos-1);
          name.erase(0, pos);
          pos = name.find('.', 1);  // check for ".SUFFIX."
        }
        if (pos != string::npos && pos == 2 && name[1] >= '0' && name[1] <= '9') {
          suffix = name.substr(0, 2);
          name.erase(0, pos);
        }
      }
    }
    return true;
  }


 private:
  /** whether this instance supports rows with defaults (starting with a star). */
  bool m_supportsDefaults;


 protected:
  /** a @a string describing the last error position. */
  string m_lastError;
};

}  // namespace ebusd

#endif  // LIB_EBUS_FILEREADER_H_

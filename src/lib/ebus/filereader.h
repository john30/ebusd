/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2025 John Baier <ebusd@ebusd.eu>
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

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <iomanip>
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"
#include "lib/utils/thread.h"

namespace ebusd {

/** @file lib/ebus/filereader.h
 * Helper class and constants for reading configuration files.
 *
 * The @a FileReader template class allows to read CSV compliant text files
 * while splitting each read line into fields.
 * It also supports special treatment of comment lines starting with a "#", as
 * well as so called "default values" indicated by the first field starting
 * with a "*" symbol.
 */

using std::string;
using std::map;
using std::ostream;
using std::istream;
using std::hex;
using std::dec;
using std::setw;
using std::setfill;

/** the separator character used between fields. */
#define FIELD_SEPARATOR ','

/** the separator character used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR '"'

/** the separator character as string used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR_STR "\""

/** the separator character used between multiple values (in CSV only). */
#define VALUE_SEPARATOR ';'

/** special marker string for skipping columns in @a MappedFileReader. */
static const char SKIP_COLUMN[] = "\b";

/**
 * An abstract class that support reading definitions from a file.
 */
class FileReader {
 public:
  /**
   * Constructor.
   */
  FileReader() {}

  /**
   * Destructor.
   */
  virtual ~FileReader() {}

  /**
   * Open a file as stream for reading.
   * @param filename the name of the file being read.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param time optional pointer to a @a time_t value for storing the modification time of the file, or nullptr.
   * @param isLink optional pointer for strong whether the file is a link, or nullptr.
   * @return the opened @a istream on success, or nullptr on error.
   */
  static istream* openFile(const string& filename, string* errorDescription, time_t* time = nullptr, bool *isLink = nullptr);

  /**
   * Read the definitions from a stream.
   * @param stream the @a istream to read from.
   * @param filename the relative name of the file being read.
   * @param mtime a @a time_t value with the modification time of the file.
   * @param verbose whether to verbosely log problems.
   * @param defaults the default values by name (potentially overwritten by file name), or nullptr to not use defaults.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param replace whether to replace an already existing entry.
   * @param hash optional pointer to a @a size_t value for storing the hash of the file, or nullptr.
   * @param size optional pointer to a @a size_t value for storing the normalized size of the file, or nullptr.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readFromStream(istream* stream, const string& filename, const time_t& mtime, bool verbose,
      map<string, string>* defaults, string* errorDescription, bool replace = false, size_t* hash = nullptr,
      size_t* size = nullptr);

  /**
   * Read a single line definition from the stream.
   * @param stream the @a istream to read from.
   * @param filename the name of the file being read.
   * @param verbose whether to verbosely log problems.
   * @param lineNo the last line number (incremented with each line read).
   * @param row the definition row to clear and update with the read data (for performance reasons only).
   * @param errorDescription a string in which to store the error description in case of error.
   * @param replace whether to replace an already existing entry.
   * @param hash optional pointer to a @a size_t value for updating with the hash of the line, or nullptr.
   * @param size optional pointer to a @a size_t value for updating with the normalized length of the line, or nullptr.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readLineFromStream(istream* stream, const string& filename, bool verbose,
      unsigned int* lineNo, vector<string>* row, string* errorDescription, bool replace, size_t* hash, size_t* size);

  /**
   * Add a definition that was read from a file.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @param row the definition row (allowed to be modified).
   * @param errorDescription a string in which to store the error description in case of error.
   * @param replace whether to replace an already existing entry.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addFromFile(const string& filename, unsigned int lineNo, vector<string>* row,
      string* errorDescription, bool replace) = 0;

  /**
   * Left and right trim the string.
   * @param str the @a string to trim.
   */
  static void trim(string* str);

  /**
   * Convert all upper case characters in the string to lower case.
   * @param str the @a string to convert.
   */
  static void tolower(string* str);

  /**
   * Check the input string against the search pattern.
   * @param input the input string to check.
   * @param search the search pattern to match against. May contain alternatives separated by a "|".
   * Each alternative may
   * - start with "^" to match the beginning of the input,
   * - end with "$" to match the end of the input,
   * - contain a single "*" (between other characters) to match an arbitrary number of characters.
   * @param ignoreCase true to ignore case differences.
   * @param searchIsLower true if search is already known to be in lowercase only.
   * @return true if the input string matches the search pattern.
   */
  static bool matches(const string& input, const string& search, bool ignoreCase = false, bool searchIsLower = false);

  /**
   * Split the next line(s) from the @a istream into fields.
   * @param stream the @a istream to read from.
   * @param row the @a vector to which to add the fields. This will be empty for completely empty and comment lines.
   * @param lineNo the current line number (incremented with each line read).
   * @param hash optional pointer to a @a size_t value for combining the hash of the line with, or nullptr.
   * @param size optional pointer to a @a size_t value to add the trimmed line length to, or nullptr.
   * @param clear whether to clear the fields before adding any.
   * @return true if there are more lines to read, false when there are no more lines left.
   */
  static bool splitFields(istream* stream, vector<string>* row, unsigned int* lineNo,
      size_t* hash = nullptr, size_t* size = nullptr, bool clear = true);

  /**
   * Format the specified hash as 8 hex digits to the output stream.
   * @param hash the hash code.
   * @param stream the @a ostream to write to.
   */
  static void formatHash(size_t hash, ostream* stream) {
    *stream << hex << setw(8) << setfill('0') << (hash & 0xffffffff) << dec << setw(0);
  }

  /**
   * Format the error description with the input data.
   * @param filename the name of the file.
   * @param lineNo the line number in the file.
   * @param result the result code.
   * @param error the error message.
   * @param errorDescription a string in which to store the error description.
   * @return the result code.
   */
  static result_t formatError(const string& filename, unsigned int lineNo, result_t result,
      const string& error, string* errorDescription);
};


/**
 * An abstract class derived from @a FileReader that additionally allows to using mapped name/value pairs with one
 * main map and many sub maps.
 */
class MappedFileReader : public FileReader {
 public:
  /**
   * Constructor.
   * @param supportsDefaults whether this instance supports rows with defaults (starting with a star).
   * @param preferLanguage the preferred language code, or empty.
   */
  explicit MappedFileReader(bool supportsDefaults, const string& preferLanguage = "")
    : FileReader(), m_supportsDefaults(supportsDefaults), m_preferLanguage(normalizeLanguage(preferLanguage)) {
  }

  /**
   * Destructor.
   */
  virtual ~MappedFileReader() {
    m_columnNames.clear();
    m_lastDefaults.clear();
    m_lastSubDefaults.clear();
  }

  /**
   * Normalize the language string to a lower case, max. 2 characters long language code.
   * @param lang the language string to normalize.
   * @return the normalized language code.
   */
  static const string normalizeLanguage(const string& lang);

  /**
   * @return the preferred language code (up to 2 characters), or empty.
   */
  const string getPreferLanguage() const { return m_preferLanguage; }

  // @copydoc
  result_t readFromStream(istream* stream, const string& filename, const time_t& mtime, bool verbose,
      map<string, string>* defaults, string* errorDescription, bool replace = false, size_t* hash = nullptr,
      size_t* size = nullptr) override;

  /**
   * Extract default values from the file name.
   * @param filename the name of the file (without path)
   * @param defaults the default values by name to add to.
   * @param destAddress optional pointer to a variable in which to store the numeric destination address, or nullptr.
   * @param software optional pointer to a in which to store the numeric software version, or nullptr.
   * @param hardware optional pointer to a in which to store the numeric hardware version, or nullptr.
   * @return true if the minimum parts were extracted, false otherwise.
   */
  virtual bool extractDefaultsFromFilename(const string& filename, map<string, string>* defaults,
      symbol_t* destAddress = nullptr, unsigned int* software = nullptr, unsigned int* hardware = nullptr) const {
    return false;
  }

  // @copydoc
  result_t addFromFile(const string& filename, unsigned int lineNo, vector<string>* row,
      string* errorDescription, bool replace) override;

  /**
   * Get the field mapping from the given first line.
   * @param row the first line from which to extract the field mapping, or empty to use the default mapping.
   * Columns set to @a SKIP_COLUMN are skipped in @a addFromFile(). Columns starting with a "*" mark the beginning of
   * a repeated sub row.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param preferLanguage the preferred language code (up to 2 characters), or empty.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t getFieldMap(const string& preferLanguage, vector<string>* row, string* errorDescription) const = 0;

  /**
   * Add a default row that was read from a file.
   * @param row the default row by field name.
   * @param subRows the sub default rows, each by field name.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addDefaultFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
      vector< map<string, string> >* subRows, string* errorDescription) {
    *errorDescription = "defaults not supported";
    return RESULT_ERR_INVALID_ARG;
  }

  /**
   * Add a definition that was read from a file.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @param row the main definition row by field name (may be modified).
   * @param subRows the sub definition rows, each by field name (may be modified).
   * @param errorDescription a string in which to store the error description in case of error.
   * @param replace whether to replace an already existing entry.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addFromFile(const string& filename, unsigned int lineNo, map<string, string>* row,
      vector< map<string, string> >* subRows, string* errorDescription, bool replace = false) = 0;

  /**
   * @return a reference to all previously extracted default values by type and field name.
   */
  map<string, map<string, string> >& getDefaults() {
    return m_lastDefaults;
  }

  /**
   * @return a reference to all previously extracted sub default values by type and field name.
   */
  map<string, vector< map<string, string> > >& getSubDefaults() {
    return m_lastSubDefaults;
  }

  /**
   * Combine the row to a single string.
   * @param row the mapped row.
   * @return the combined string.
   */
  static const string combineRow(const map<string, string>& row);

 protected:
  /** a @a Mutex for access to defaults. */
  Mutex m_mutex;

 private:
  /** whether this instance supports rows with defaults (starting with a star). */
  const bool m_supportsDefaults;

  /** the preferred language code (up to 2 characters), or empty. */
  const string m_preferLanguage;

  /** the name of each column. */
  vector<string> m_columnNames;

  /** all previously extracted default values by type and field name. */
  map<string, map<string, string> > m_lastDefaults;

  /** all previously extracted sub default values by type and field name. */
  map<string, vector< map<string, string> > > m_lastSubDefaults;
};

}  // namespace ebusd

#endif  // LIB_EBUS_FILEREADER_H_

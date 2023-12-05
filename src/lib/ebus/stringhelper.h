/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022-2023 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_EBUS_STRINGHELPER_H_
#define LIB_EBUS_STRINGHELPER_H_

#include <unistd.h>
#include <cstdint>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include "lib/ebus/message.h"

namespace ebusd {

/** @file lib/ebus/stringhelper.h
 * Helper classes for string replacement.
 */

using std::map;
using std::pair;
using std::ostringstream;
using std::string;
using std::vector;


/**
 * Helper class for replacing a template string with real values.
 */
class StringReplacer {
 public:
  /**
   * Normalize the string to contain only alpha numeric characters plus underscore by replacing other characters with
   * an underscore.
   * @param str the string to normalize.
   */
  static void normalize(string& str);

  /**
   * Get the template string.
   * @return the template string (might already be partially reduced).
   */
  const string str() const;

  /**
   * Parse the template string.
   * @param templateStr the template string.
   * @param onlyKnown true to allow only known field names from @a knownFieldNames.
   * @param noKnownDuplicates true to now allow duplicates from @a knownFieldNames.
   * @param emptyIfMissing true when the complete result is supposed to be empty when at least one referenced variable
   * is empty or not defined.
   * @return true on success, false on malformed template string.
   */
  bool parse(const string& templateStr, bool onlyKnown = false, bool noKnownDuplicates = false,
             bool emptyIfMissing = false);

  /**
   * Ensure the default parts are present (package prefix if empty, circuit and message name).
   * @param separator the separator between prefix, circuit, and message name (default slash).
   */
  void ensureDefault(const string& separator = "/");

  /**
   * Return whether this replacer is completely empty.
   * @return true when empty.
   */
  bool empty() const;

  /**
   * Return whether the specified field is used.
   * @param field the field name to check.
   * @return true when the specified field is used.
   */
  bool has(const string& field) const;

  /**
   * Get the replaced template string.
   * @param values the named values for replacement.
   * @param untilFirstEmpty true to only return the prefix before the first empty field.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @return the replaced template string.
   */
  string get(const map<string, string>& values, bool untilFirstEmpty = true, bool onlyAlphanum = false) const;

  /**
   * Get the replaced template string.
   * @param circuit the circuit name for replacement.
   * @param name the message name for replacement.
   * @param fieldName the field name for replacement.
   * @return the replaced template string.
   */
  string get(const string& circuit, const string& name, const string& fieldName = "") const;

  /**
   * Get the replaced template string.
   * @param message the Message from which to extract the values for replacement.
   * @param fieldName the field name for replacement.
   * @return the replaced template string.
   */
  string get(const Message* message, const string& fieldName = "") const;

  /**
   * Check if the fields can be reduced to a constant value.
   * @param values the named values for replacement.
   * @return true if the result is final.
   */
  bool isReducable(const map<string, string>& values) const;

  /**
   * Compress all subsequent constant values to a single constant value if possible.
   * @param values the named values for replacement.
   */
  void compress(const map<string, string>& values);

  /**
   * Reduce the fields to a constant value if possible.
   * @param values the named values for replacement.
   * @param result the string to store the result in.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @return true if the result is final.
   */
  bool reduce(const map<string, string>& values, string& result, bool onlyAlphanum = false) const;

  /**
   * Check match-ability against a string.
   * @return true on success, false on bad match-ability.
   */
  bool checkMatchability() const;

  /**
   * Match a string against the constant and variables parts.
   * @param str the string to match.
   * @param circuit pointer to the string receiving the circuit name if present.
   * @param name pointer to the string receiving the message name if present.
   * @param field pointer to the string receiving the field name if present.
   * @param separator the separator expected in the extra non-matched non-field parts (default slash).
   * @return the index of the last unmatched part, or the negative index minus one for extra non-matched non-field parts.
   */
  ssize_t match(const string& str, string* circuit, string* name, string* field, const string& separator = "/") const;

 private:
  /**
   * the list of parts the template is composed of.
   * the string is either the plain string or the name of the field.
   * the number is negative for plain strings, the index to @a knownFieldNames for a known field, or the size of
   * @a knownFieldNames for an unknown field.
   */
  vector<pair<string, int>> m_parts;

  /** true when the complete result is supposed to be empty when at least one referenced variable
   * is empty or not defined. */
  bool m_emptyIfMissing;

  /**
   * Create a named field or constant.
   * @param name the plain string or the name of the field.
   * @param isField true when it is a field.
   * @return the created pair.
   */
  static pair<string, int> makeField(const string& name, bool isField);

  /**
   * Add a part to the list of parts.
   * @param stack the parsing stack.
   * @param inField 1 after '%', 2 after '%{', 0 otherwise.
   */
  void addPart(ostringstream& stack, int inField);
};


/**
 * A set of constants and @a StringReplacer variables.
 */
class StringReplacers {
 public:
  /**
   * Get the value of the specified key from the constants only.
   * @param key the key for which to get the value.
   * @return the value string or empty.
   */
  const string& operator[](const string& key) const;

  /**
   * Parse a continuation-normalized line.
   * @param line the line to parse.
   */
  void parseLine(const string& line);

  /**
   * Parse a file with constants and variables.
   * @param filename the name (and path) of the file to parse.
   * @return true on success, false if the file is not readable.
   */
  bool parseFile(const char* filename);

  /**
   * Check if the specified field is used by one of the replacers.
   * @param field the name of the field to check.
   * @return true if the specified field is used by one of the replacers.
   */
  bool uses(const string& field) const;

  /**
   * Get the variable value of the specified key.
   * @param key the key for which to get the value.
   * @return the value @a StringReplacer.
   */
  StringReplacer& get(const string& key);

  /**
   * Get the variable value of the specified key.
   * @param key the key for which to get the value.
   * @return the value @a StringReplacer.
   */
  StringReplacer get(const string& key) const;

  /**
   * Get the variable or constant value of the specified key.
   * @param key the key for which to get the value.
   * @param untilFirstEmpty true to only return the prefix before the first empty field.
   * @param onlyAlphanum whether to only allow alpha numeric characters plus underscore.
   * @param fallbackKey optional fallback key to use when key value is undefined.
   * @return the value string or empty.
   */
  string get(const string& key, bool untilFirstEmpty, bool onlyAlphanum = false, const string& fallbackKey = "") const;

  /**
   * Set the constant value of the specified key and additionally normalized with uppercase key only (if the key does
   * not contain an underscore).
   * @param key the key to store.
   * @param value the value string.
   * @param removeReplacer true to remove a replacer with the same name.
   * @return true when an upper case key was stored/updates as well.
   */
  bool set(const string& key, const string& value, bool removeReplacer = true);

  /**
   * Set the constant value of the specified key.
   * @param key the key to store.
   * @param value the numeric value (converted to a string).
   */
  void set(const string& key, int value);

  /**
   * Reduce as many variables to constants as possible.
   * @param compress true to compress non-reducable replacers if possible.
   */
  void reduce(bool compress = false);

  /**
   * Get all set keys.
   * @return the set keys.
   */
  vector<string> keys() const;

 private:
  /** constant values from the integration file. */
  map<string, string> m_constants;

  /** variable values from the integration file. */
  map<string, StringReplacer> m_replacers;
};


}  // namespace ebusd

#endif  // LIB_EBUS_STRINGHELPER_H_

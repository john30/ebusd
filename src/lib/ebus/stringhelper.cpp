/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2022-2024 John Baier <ebusd@ebusd.eu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "lib/ebus/stringhelper.h"
#include <algorithm>
#include <stack>

namespace ebusd {

using std::ostringstream;

/** the known field names for identifying a message field. */
static const char* knownFieldNames[] = {
    "circuit",
    "name",
    "field",
};

/** the number of known field names. */
static const size_t knownFieldCount = sizeof(knownFieldNames) / sizeof(char*);

std::pair<string, int> StringReplacer::makeField(const string& name, bool isField) {
  if (!isField) {
    return {name, -1};
  }
  for (int idx = 0; idx < static_cast<int>(knownFieldCount); idx++) {
    if (name == knownFieldNames[idx]) {
      return {name, idx};
    }
  }
  return {name, knownFieldCount};
}

void StringReplacer::addPart(ostringstream& stack, int inField) {
  string str = stack.str();
  if (inField == 1 && str == "_") {
    inField = 0;  // single "%_" pattern to reduce to "_"
  } else if (inField == 2) {
    str = "%{" + str;
    inField = 0;
  }
  if (inField == 0 && str.empty()) {
    return;
  }
  stack.str("");
  if (inField == 0 && !m_parts.empty() && m_parts[m_parts.size()-1].second < 0) {
    // append constant to previous constant
    m_parts[m_parts.size()-1].first += str;
    return;
  }
  m_parts.push_back(makeField(str, inField > 0));
}

bool StringReplacer::parse(const string& templateStr, bool onlyKnown, bool noKnownDuplicates, bool emptyIfMissing) {
  m_parts.clear();
  int inField = 0;  // 1 after '%', 2 after '%{'
  ostringstream stack;
  for (auto ch : templateStr) {
    bool empty = stack.tellp() <= 0;
    if (ch == '%') {
      if (inField == 1 && empty) {  // %% for plain %
        inField = 0;
        stack << ch;
      } else {
        addPart(stack, inField);
        inField = 1;
      }
    } else if (ch == '{' && inField == 1 && empty) {
      inField = 2;
    } else if (ch == '}' && inField == 2) {
      addPart(stack, 1);
      inField = 0;
    } else {
      if (inField > 0 && !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')) {
        // invalid field character
        addPart(stack, inField);
        inField = 0;
      }
      stack << ch;
    }
  }
  addPart(stack, inField);
  if (onlyKnown || noKnownDuplicates) {
    int foundMask = 0;
    int knownCount = knownFieldCount;
    for (const auto &it : m_parts) {
      if (it.second < 0) {
        continue;  // unknown field
      }
      if (onlyKnown && it.second >= knownCount) {
        return false;
      }
      if (noKnownDuplicates && it.second < knownCount) {
        int bit = 1 << it.second;
        if (foundMask & bit) {
          return false;  // duplicate known field
        }
        foundMask |= bit;
      }
    }
  }
  m_emptyIfMissing = emptyIfMissing;
  return true;
}

void StringReplacer::normalize(string& str) {
  transform(str.begin(), str.end(), str.begin(), [](unsigned char c){
    return isalnum(c) ? c : '_';
  });
}

const string StringReplacer::str() const {
  ostringstream ret;
  for (const auto &it : m_parts) {
    if (it.second >= 0) {
      ret << '%';
    }
    ret << it.first;
  }
  return ret.str();
}

void StringReplacer::ensureDefault(const string& separator) {
  if (m_parts.empty()) {
    m_parts.emplace_back(string(PACKAGE) + separator, -1);
  }
  if (!has("circuit")) {
    if (m_parts.back().second >= 0) {
      // add separator between two variables
      m_parts.emplace_back(separator, -1);
    } else if (m_parts.back().first.back() != '/') {
      m_parts[m_parts.size() - 1] = {m_parts.back().first + separator, -1};  // ensure trailing slash
    }
    m_parts.emplace_back("circuit", 0);  // index of circuit in knownFieldNames
  }
  if (!has("name")) {
    if (m_parts.back().second >= 0) {
      // add separator between two variables
      m_parts.emplace_back(separator, -1);
    } else if (m_parts.back().first.back() != '/') {
      m_parts[m_parts.size() - 1] = {m_parts.back().first + separator, -1};  // ensure trailing slash
    }
    m_parts.emplace_back("name", 1);  // index of name in knownFieldNames
  }
}

bool StringReplacer::empty() const {
  return m_parts.empty();
}

bool StringReplacer::has(const string& field) const {
  for (const auto &it : m_parts) {
    if (it.second >= 0 && it.first == field) {
      return true;
    }
  }
  return false;
}

string StringReplacer::get(const map<string, string>& values, bool untilFirstEmpty, bool onlyAlphanum) const {
  ostringstream ret;
  for (const auto &it : m_parts) {
    if (it.second < 0) {
      ret << it.first;
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos == values.cend()) {
      if (untilFirstEmpty) {
        break;
      }
      if (m_emptyIfMissing) {
        return "";
      }
    } else if (pos->second.empty()) {
      if (untilFirstEmpty) {
        break;
      }
      if (m_emptyIfMissing) {
        return "";
      }
    } else {
      ret << pos->second;
    }
  }
  if (!onlyAlphanum) {
    return ret.str();
  }
  string str = ret.str();
  normalize(str);
  return str;
}

string StringReplacer::get(const string& circuit, const string& name, const string& fieldName) const {
  map <string, string> values;
  values["circuit"] = circuit;
  values["name"] = name;
  if (!fieldName.empty()) {
    values["field"] = fieldName;
  }
  return get(values, true);
}

string StringReplacer::get(const Message* message, const string& fieldName) const {
  map<string, string> values;
  values["circuit"] = message->getCircuit();
  values["name"] = message->getName();
  if (!fieldName.empty()) {
    values["field"] = fieldName;
  }
  return get(message->getCircuit(), message->getName(), fieldName);
}

bool StringReplacer::isReducable(const map<string, string>& values) const {
  for (const auto &it : m_parts) {
    if (it.second < 0) {
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos == values.cend()) {
      return false;
    }
  }
  return true;
}

void StringReplacer::compress(const map<string, string>& values) {
  bool lastConstant = false;
  for (auto it = m_parts.begin(); it != m_parts.end(); ) {
    bool isConstant = it->second < 0;
    if (!isConstant) {
      const auto pos = values.find(it->first);
      if (pos != values.cend()) {
        it->second = -1;
        it->first = pos->second;
        isConstant = true;
      }
    }
    if (!lastConstant || !isConstant) {
      lastConstant = isConstant;
      ++it;
      continue;
    }
    (it-1)->first += it->first;
    it = m_parts.erase(it);
  }
}

bool StringReplacer::reduce(const map<string, string>& values, string& result, bool onlyAlphanum) const {
  ostringstream ret;
  for (const auto &it : m_parts) {
    if (it.second < 0) {
      ret << it.first;
      continue;
    }
    const auto pos = values.find(it.first);
    if (pos == values.cend()) {
      if (m_emptyIfMissing) {
        result = "";
      } else {
        result = ret.str();
      }
      return false;
    }
    if (m_emptyIfMissing && pos->second.empty()) {
      result = "";
      return true;
    }
    ret << pos->second;
  }
  result = ret.str();
  if (onlyAlphanum) {
    normalize(result);
  }
  return true;
}

bool StringReplacer::checkMatchability() const {
  bool lastField = false;
  for (const auto& part : m_parts) {
    bool field = part.second >= 0;
    if (field && lastField) {
      return false;
    }
    lastField = field;
  }
  return true;
}

ssize_t StringReplacer::match(const string& strIn, string* circuit, string* name, string* field,
  const string& separator, bool ignoreCase) const {
  string str = strIn;
  if (ignoreCase) {
    FileReader::tolower(&str);
  }
  size_t last = 0;
  size_t count = m_parts.size();
  size_t idx;
  bool incomplete = false;
  for (idx = 0; idx < count && !incomplete; idx++) {
    const auto part = m_parts[idx];
    if (part.second < 0) {
      if (str.substr(last, part.first.length()) != part.first) {
        return static_cast<ssize_t>(idx);
      }
      last += part.first.length();
      continue;
    }
    string value;
    if (idx+1 < count) {
      string chk = m_parts[idx+1].first;
      if (ignoreCase) {
        FileReader::tolower(&chk);
      }
      size_t pos = str.find(chk, last);
      if (pos == string::npos) {
        // next part not found, consume the rest and mark incomplete
        value = str.substr(last);
        incomplete = true;
      } else {
        value = str.substr(last, pos - last);
      }
    } else {
      // last part is a field name
      if (str.find(separator, last) != string::npos) {
        // non-name in remainder found
        return -static_cast<ssize_t>(idx)-1;
      }
      value = str.substr(last);
    }
    last += value.length();
    switch (part.second) {
      case 0: *circuit = value; break;
      case 1: *name = value; break;
      case 2: *field = value; break;
      default:  // unknown field
        break;
    }
  }
  if (incomplete) {
    return -static_cast<ssize_t>(idx)-1;
  }
  return static_cast<ssize_t>(idx);
}


static const string EMPTY = "";

const string& StringReplacers::operator[](const string& key) const {
  auto itc = m_constants.find(key);
  if (itc == m_constants.end()) {
    return EMPTY;
  }
  return itc->second;
}

void StringReplacers::parseLine(const string& line) {
  if (line.empty()) {
    return;
  }
  size_t pos = line.find('=');
  if (pos == string::npos || pos == 0) {
    return;
  }
  bool emptyIfMissing = line[pos-1] == '?';
  bool append = !emptyIfMissing && line[pos-1] == '+';
  string key = line.substr(0, (emptyIfMissing || append) ? pos-1 : pos);
  FileReader::trim(&key);
  string value = line.substr(pos+1);
  if (append) {
    value = get(key).str() + value;
  }
  FileReader::trim(&value);
  if (value.find('%') == string::npos) {
    set(key, value);  // constant value
  } else {
    // simple variable
    get(key).parse(value, false, false, emptyIfMissing);
  }
}

bool StringReplacers::parseFile(const char* filename) {
  std::ifstream stream;
  stream.open(filename, std::ifstream::in);
  if (!stream.is_open()) {
    return false;
  }
  string line, last;
  while (stream.peek() != EOF && getline(stream, line)) {
    if (line.empty()) {
      parseLine(last);
      last = "";
      continue;
    }
    if (line[0] == '#') {
      // only ignore it to allow commented lines in the middle of e.g. payload
      continue;
    }
    if (last.empty()) {
      last = line;
    } else if (line[0] == '\t' || line[0] == ' ') {  // continuation
      last += "\n" + line;
    } else {
      parseLine(last);
      last = line;
    }
  }
  stream.close();
  parseLine(last);
  return true;
}

bool StringReplacers::uses(const string& field) const {
  for (const auto &it : m_replacers) {
    if (it.second.has(field)) {
      return true;
    }
  }
  return false;
}

StringReplacer& StringReplacers::get(const string& key) {
  StringReplacer& ret = m_replacers[key];
  auto it = m_constants.find(key);
  if (it != m_constants.end()) {
    // constant with the same name found
    if (ret.empty()) {
      ret.parse(it->second);  // convert to replacer
    }
    m_constants.erase(it);
  }
  return ret;
}

StringReplacer StringReplacers::get(const string& key) const {
  const auto& it = m_replacers.find(key);
  if (it != m_replacers.cend()) {
    return it->second;
  }
  return StringReplacer();
}

string StringReplacers::get(const string& key, bool untilFirstEmpty, bool onlyAlphanum,
  const string& fallbackKey) const {
  auto itc = m_constants.find(key);
  if (itc != m_constants.end()) {
    return itc->second;
  }
  auto itv = m_replacers.find(key);
  if (itv != m_replacers.end()) {
    return itv->second.get(m_constants, untilFirstEmpty, onlyAlphanum);
  }
  if (!fallbackKey.empty()) {
    itc = m_constants.find(fallbackKey);
    if (itc != m_constants.end()) {
      return itc->second;
    }
    itv = m_replacers.find(fallbackKey);
    if (itv != m_replacers.end()) {
      return itv->second.get(m_constants, untilFirstEmpty, onlyAlphanum);
    }
  }
  return "";
}

bool StringReplacers::set(const string& key, const string& value, bool removeReplacer) {
  m_constants[key] = value;
  if (removeReplacer) {
    m_replacers.erase(key);
  }
  if (key.find_first_of("-_/") != string::npos) {
    return false;
  }
  string upper = key;
  transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  if (upper == key) {
    return false;
  }
  string val = value;
  StringReplacer::normalize(val);
  m_constants[upper] = val;
  if (removeReplacer) {
    m_replacers.erase(upper);
  }
  return true;
}

void StringReplacers::set(const string& key, int value) {
  ostringstream str;
  str << static_cast<signed>(value);
  m_constants[key] = str.str();
}

void StringReplacers::reduce(bool compress) {
  // iterate through variables and reduce as many to constants as possible
  bool reduced = false;
  do {
    reduced = false;
    for (auto it = m_replacers.begin(); it != m_replacers.end(); ) {
      string str;
      if (!it->second.isReducable(m_constants)
          || !it->second.reduce(m_constants, str)) {
        if (compress) {
          it->second.compress(m_constants);
        }
        ++it;
        continue;
      }
      string key = it->first;
      it = m_replacers.erase(it);
      bool restart = set(key, str, false);
      reduced = true;
      if (restart) {
        transform(key.begin(), key.end(), key.begin(), ::toupper);
        if (m_replacers.erase(key) > 0) {
          break;  // restart as iterator is now invalid
        }
      }
    }
  } while (reduced);
}

vector<string> StringReplacers::keys() const {
  vector<string> ret;
  for (auto& it : m_constants) {
    ret.push_back(it.first);
  }
  for (auto& it : m_replacers) {
    ret.push_back(it.first);
  }
  return ret;
}

}  // namespace ebusd

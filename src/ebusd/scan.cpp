/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#include "ebusd/scan.h"
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <map>
#include <vector>
#include <functional>
#include "ebusd/bushandler.h"
#include "lib/utils/log.h"


namespace ebusd {

using std::dec;
using std::hex;
using std::setfill;
using std::setw;
using std::nouppercase;


ScanHelper::~ScanHelper() {
  // free templates
  for (const auto& it : m_templatesByPath) {
    if (it.second != &m_globalTemplates) {
      delete it.second;
    }
  }
  m_templatesByPath.clear();
  if (m_configHttpClient) {
    delete m_configHttpClient;
    m_configHttpClient = nullptr;
  }
}

// the time slice to sleep when repeating an HTTP request
#define REPEAT_NANOS 1000000

result_t ScanHelper::collectConfigFiles(const string& relPath, const string& prefix, const string& extension,
    vector<string>* files,
    bool ignoreAddressPrefix, const string& query,
    vector<string>* dirs, bool* hasTemplates) {
  const string relPathWithSlash = relPath.empty() ? "" : relPath + "/";
  if (!m_configUriPrefix.empty()) {
    string uri = m_configUriPrefix + relPathWithSlash + m_configLangQuery + (m_configLangQuery.empty() ? "?" : "&")
      + "t=" + extension.substr(1) + query;
    string names;
    bool repeat = false;
    bool json = true;
    if (!m_configHttpClient->get(uri, "", &names, &repeat, nullptr, &json)) {
      if (!names.empty() || json) {
        logError(lf_main, "HTTP failure%s: %s", repeat ? ", repeating" : "", names.c_str());
        names = "";
      }
      if (!repeat) {
        return RESULT_ERR_NOTFOUND;
      }
      usleep(REPEAT_NANOS);
      if (!m_configHttpClient->get(uri, "", &names)) {
        return RESULT_ERR_NOTFOUND;
      }
    } else if (!json && names[0]=='<') {  // html
      uri = m_configUriPrefix + relPathWithSlash + "index.json";
      json = true;
      logDebug(lf_main, "trying index.json");
      if (!m_configHttpClient->get(uri, "", &names, nullptr, nullptr, &json)) {
        return RESULT_ERR_NOTFOUND;
      }
    }
    istringstream stream(names);
    string name;
    while (getline(stream, name)) {
      if (name.empty()) {
        continue;
      }
      if (name == "_templates"+extension) {
        if (hasTemplates) {
          *hasTemplates = true;
        }
        continue;
      }
      if (prefix.length() == 0 ? (!ignoreAddressPrefix || name.length() < 3 || name.find_first_of('.') != 2)
      : (name.length() >= prefix.length() && name.substr(0, prefix.length()) == prefix)) {
        files->push_back(relPathWithSlash + name);
      }
    }
    return RESULT_OK;
  }
  const string path = m_configLocalPrefix + relPathWithSlash;
  logDebug(lf_main, "reading directory %s", path.c_str());
  DIR* dir = opendir(path.c_str());
  if (dir == nullptr) {
    return RESULT_ERR_NOTFOUND;
  }
  dirent* d;
  while ((d = readdir(dir)) != nullptr) {
    string name = d->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    const string p = path + name;
    struct stat stat_buf = {};
    if (stat(p.c_str(), &stat_buf) != 0) {
      logError(lf_main, "unable to stat file %s", p.c_str());
      continue;
    }
    logDebug(lf_main, "file type of %s is %s", p.c_str(),
             S_ISDIR(stat_buf.st_mode) ? "dir" : S_ISREG(stat_buf.st_mode) ? "file" : "other");
    if (S_ISDIR(stat_buf.st_mode)) {
      if (dirs != nullptr) {
        dirs->push_back(relPathWithSlash + name);
      }
    } else if (S_ISREG(stat_buf.st_mode) && name.length() >= extension.length()
    && name.substr(name.length()-extension.length()) == extension) {
      if (name == "_templates"+extension) {
        if (hasTemplates) {
          *hasTemplates = true;
        }
        continue;
      }
      if (prefix.length() == 0 ? (!ignoreAddressPrefix || name.length() < 3 || name.find_first_of('.') != 2)
          : (name.length() >= prefix.length() && name.substr(0, prefix.length()) == prefix)) {
        files->push_back(relPathWithSlash + name);
      }
    }
  }
  closedir(dir);

  return RESULT_OK;
}

DataFieldTemplates* ScanHelper::getTemplates(const string& filename) {
  if (filename == "*") {
    size_t maxLength = 0;
    DataFieldTemplates* best = nullptr;
    for (auto it : m_templatesByPath) {
      if (it.first.size() > maxLength) {
        best = it.second;
      }
    }
    if (best) {
      return best;
    }
  } else {
    string path;
    size_t pos = filename.find_last_of('/');
    if (pos != string::npos) {
      path = filename.substr(0, pos);
    }
    const auto it = m_templatesByPath.find(path);
    if (it != m_templatesByPath.end()) {
      return it->second;
    }
  }
  return &m_globalTemplates;
}

bool ScanHelper::readTemplates(const string relPath, const string extension, bool available) {
  const auto it = m_templatesByPath.find(relPath);
  if (it != m_templatesByPath.end()) {
    return false;
  }
  DataFieldTemplates* templates;
  if (relPath.empty() || !available) {
    templates = &m_globalTemplates;
  } else {
    templates = new DataFieldTemplates(m_globalTemplates);
  }
  m_templatesByPath[relPath] = templates;
  if (!available) {
    // global templates are stored as replacement in order to determine whether the directory was already loaded
    return true;
  }
  string errorDescription;
  string logPath = relPath.empty() ? "/" : relPath;
  logInfo(lf_main, "reading templates %s", logPath.c_str());
  string file = (relPath.empty() ? "" : relPath + "/") + "_templates" + extension;
  result_t result = loadDefinitionsFromConfigPath(templates, file, nullptr, &errorDescription, true);
  if (result == RESULT_OK) {
    logInfo(lf_main, "read templates in %s", logPath.c_str());
    return true;
  }
  logError(lf_main, "error reading templates in %s: %s, last error: %s", logPath.c_str(), getResultCode(result),
       errorDescription.c_str());
  return false;
}

void ScanHelper::dumpTemplates(OutputFormat outputFormat, ostream* output) const {
  bool prependSeparator = false;
  for (auto it : m_templatesByPath) {
    if (prependSeparator) {
      *output << ",";
    }
    const auto templates = it.second;
    if (templates->dump(outputFormat, output)) {
      prependSeparator = true;
    }
  }
}

result_t ScanHelper::readConfigFiles(const string& relPath, const string& extension, bool recursive,
  string* errorDescription) {
  vector<string> files, dirs;
  bool hasTemplates = false;
  result_t result = collectConfigFiles(relPath, "", extension, &files, false, "", &dirs, &hasTemplates);
  if (result != RESULT_OK) {
    return result;
  }
  readTemplates(relPath, extension, hasTemplates);
  for (const auto& name : files) {
    logInfo(lf_main, "reading file %s", name.c_str());
    result = loadDefinitionsFromConfigPath(m_messages, name, nullptr, errorDescription);
    if (result != RESULT_OK) {
      return result;
    }
    logInfo(lf_main, "successfully read file %s", name.c_str());
  }
  if (recursive) {
    for (const auto& name : dirs) {
      logInfo(lf_main, "reading dir  %s", name.c_str());
      result = readConfigFiles(name, extension, true, errorDescription);
      if (result != RESULT_OK) {
        return result;
      }
      logInfo(lf_main, "successfully read dir %s", name.c_str());
    }
  }
  return RESULT_OK;
}

static BusHandler* executeInstructionsBusHandlerInstance = nullptr;

/**
  * Helper method for immediate reading of a @a Message from the bus.
  * @param message the @a Message to read.
  */
static void readMessage(Message* message) {
  if (!executeInstructionsBusHandlerInstance || !message) {
    return;
  }
  result_t result = executeInstructionsBusHandlerInstance->readFromBus(message, "");
  if (result != RESULT_OK) {
    logError(lf_main, "error reading message %s %s: %s", message->getCircuit().c_str(), message->getName().c_str(),
        getResultCode(result));
  }
}

result_t ScanHelper::executeInstructions(BusHandler* busHandler) {
  string errorDescription;
  result_t result = m_messages->resolveConditions(m_verbose, &errorDescription);
  if (result != RESULT_OK) {
    logError(lf_main, "error resolving conditions: %s, last error: %s", getResultCode(result),
        errorDescription.c_str());
  }
  ostringstream log;
  executeInstructionsBusHandlerInstance = busHandler;
  result = m_messages->executeInstructions(&readMessage, &log);
  if (result != RESULT_OK) {
    logError(lf_main, "error executing instructions: %s, last error: %s", getResultCode(result),
        log.str().c_str());
  } else if (m_verbose && log.tellp() > 0) {
    logInfo(lf_main, log.str().c_str());
  }
  logNotice(lf_main, "found messages: %d (%d conditional on %d conditions, %d poll, %d update)", m_messages->size(),
      m_messages->sizeConditional(), m_messages->sizeConditions(), m_messages->sizePoll(), m_messages->sizePassive());
  return result;
}

result_t ScanHelper::loadDefinitionsFromConfigPath(FileReader* reader, const string& filename,
    map<string, string>* defaults, string* errorDescription, bool replace) {
  istream* stream = nullptr;
  time_t mtime = 0;
  if (m_configUriPrefix.empty()) {
    stream = FileReader::openFile(m_configLocalPrefix + filename, errorDescription, &mtime);
  } else if (m_configHttpClient) {
    string uri = m_configUriPrefix + filename + m_configLangQuery;
    string content;
    bool repeat = false;
    if (m_configHttpClient->get(uri, "", &content, &repeat, &mtime)) {
      stream = new istringstream(content);
    } else {
      if (!content.empty()) {
        logError(lf_main, "HTTP failure%s: %s", repeat ? ", repeating" : "", content.c_str());
        content = "";
      }
      if (repeat) {
        usleep(REPEAT_NANOS);
        if (m_configHttpClient->get(uri, "", &content, nullptr, &mtime)) {
          stream = new istringstream(content);
        }
      }
    }
  }
  result_t result;
  if (stream) {
    result = reader->readFromStream(stream, filename, mtime, m_verbose, defaults, errorDescription, replace);
    delete(stream);
  } else {
    result = RESULT_ERR_NOTFOUND;
  }
  return result;
}

result_t ScanHelper::loadConfigFiles(bool recursive) {
  logInfo(lf_main, "loading configuration files from %s", m_configPath.c_str());
  m_messages->lock();
  m_messages->clear();
  m_globalTemplates.clear();
  for (auto& it : m_templatesByPath) {
    if (it.second != &m_globalTemplates) {
      delete it.second;
    }
    it.second = nullptr;
  }
  m_templatesByPath.clear();

  string errorDescription;
  result_t result = readConfigFiles("", ".csv", recursive, &errorDescription);
  if (result == RESULT_OK) {
    logInfo(lf_main, "read config files, got %d messages", m_messages->size());
  } else {
    logError(lf_main, "error reading config files from %s: %s, last error: %s", m_configPath.c_str(),
             getResultCode(result), errorDescription.c_str());
  }
  m_messages->unlock();
  return result;
}

result_t ScanHelper::loadScanConfigFile(symbol_t address, string* relativeFile) {
  Message* message = m_messages->getScanMessage(address);
  if (!message || message->getLastUpdateTime() == 0) {
    return RESULT_ERR_NOTFOUND;
  }
  const SlaveSymbolString& data = message->getLastSlaveData();
  if (data.getDataSize() < 1+5+2+2) {
    logError(lf_main, "unable to load scan config %2.2x: slave part too short (%d)", address, data.getDataSize());
    return RESULT_EMPTY;
  }
  DataFieldSet* identFields = DataFieldSet::getIdentFields();
  string manufStr, addrStr, ident;  // path: cfgpath/MANUFACTURER, prefix: ZZ., ident: C[C[C[C[C]]]], SW: xxxx, HW: xxxx
  unsigned int sw = 0, hw = 0;
  ostringstream out;
  size_t offset = 0;
  size_t field = 0;
  bool fromLocal = m_configUriPrefix.empty();
  // manufacturer name
  result_t result = (*identFields)[field]->read(data, offset, false, nullptr, -1, OF_NONE, -1, &out);
  if (result == RESULT_ERR_NOTFOUND && fromLocal) {
    result = (*identFields)[field]->read(data, offset, false, nullptr, -1, OF_NUMERIC, -1, &out);  // manufacturer name
  }
  if (result == RESULT_OK) {
    manufStr = out.str();
    transform(manufStr.begin(), manufStr.end(), manufStr.begin(), ::tolower);
    out.str("");
    out << setw(2) << hex << setfill('0') << nouppercase << static_cast<unsigned>(address);
    addrStr = out.str();
    out.str("");
    out.clear();
    offset += (*identFields)[field++]->getLength(pt_slaveData, MAX_LEN);
    result = (*identFields)[field]->read(data, offset, false, nullptr, -1, OF_NONE, -1, &out);  // identification string
  }
  if (result == RESULT_OK) {
    ident = out.str();
    out.str("");
    out.clear();
    offset += (*identFields)[field++]->getLength(pt_slaveData, MAX_LEN);
    result = (*identFields)[field]->read(data, offset, nullptr, -1, &sw);  // software version number
    if (result == RESULT_ERR_OUT_OF_RANGE) {
      sw = (data.dataAt(offset) << 16) | data.dataAt(offset+1);  // use hex value instead
      result = RESULT_OK;
    }
  }
  if (result == RESULT_OK) {
    offset += (*identFields)[field++]->getLength(pt_slaveData, MAX_LEN);
    result = (*identFields)[field]->read(data, offset, nullptr, -1, &hw);  // hardware version number
    if (result == RESULT_ERR_OUT_OF_RANGE) {
      hw = (data.dataAt(offset) << 16) | data.dataAt(offset+1);  // use hex value instead
      result = RESULT_OK;
    }
  }
  if (result != RESULT_OK) {
    logError(lf_main, "unable to load scan config %2.2x: decode field %s %s", address,
             identFields->getName(field).c_str(), getResultCode(result));
    return result;
  }
  bool hasTemplates = false;
  string best;
  map<string, string> bestDefaults;
  vector<string> files;
  auto it = ident.begin();
  while (it != ident.end()) {
    if (*it != '_' && !::isalnum(*it)) {
      it = ident.erase(it);
    } else {
      *it = static_cast<char>(::tolower(*it));
      it++;
    }
  }
  // find files matching MANUFACTURER/ZZ.*csv in cfgpath
  string query;
  if (!fromLocal) {
    out << "&a=" << addrStr << "&i=" << ident << "&h=" << dec << static_cast<unsigned>(hw) << "&s=" << dec
        << static_cast<unsigned>(sw);
    query = out.str();
    out.str("");
    out.clear();
  }
  result = collectConfigFiles(manufStr, addrStr + ".", ".csv", &files, false, query, nullptr, &hasTemplates);
  if (result != RESULT_OK) {
    logError(lf_main, "unable to load scan config %2.2x: list files in %s %s", address, manufStr.c_str(),
        getResultCode(result));
    return result;
  }
  if (files.empty()) {
    logError(lf_main, "unable to load scan config %2.2x: no file from %s with prefix %s found", address,
        manufStr.c_str(), addrStr.c_str());
    return RESULT_ERR_NOTFOUND;
  }
  logDebug(lf_main, "found %d matching scan config files from %s with prefix %s: %s", files.size(), manufStr.c_str(),
           addrStr.c_str(), getResultCode(result));
  // complete name: cfgpath/MANUFACTURER/ZZ[.C[C[C[C[C]]]]][.circuit][.suffix][.*][.SWxxxx][.HWxxxx][.*].csv
  size_t bestMatch = 0;
  for (const auto& name : files) {
    symbol_t checkDest;
    unsigned int checkSw, checkHw;
    map<string, string> defaults;
    const string filename = name.substr(manufStr.length()+1);
    if (!m_messages->extractDefaultsFromFilename(filename, &defaults, &checkDest, &checkSw, &checkHw)) {
      continue;
    }
    if (address != checkDest || (checkSw != UINT_MAX && sw != checkSw) || (checkHw != UINT_MAX && hw != checkHw)) {
      continue;
    }
    size_t match = 1;
    string checkIdent = defaults["name"];
    if (!checkIdent.empty()) {
      string remain = ident;
      bool matches = false;
      while (remain.length() > 0 && remain.length() >= checkIdent.length()) {
        if (checkIdent == remain) {
          matches = true;
          break;
        }
        if (!::isdigit(remain[remain.length()-1])) {
          break;
        }
        remain.erase(remain.length()-1);  // remove trailing digit
      }
      if (!matches) {
        continue;  // IDENT mismatch
      }
      match += remain.length();
    }
    if (match >= bestMatch) {
      bestMatch = match;
      best = name;
      bestDefaults = defaults;
    }
  }

  if (best.empty()) {
    logError(lf_main,
        "unable to load scan config %2.2x: no file from %s with prefix %s matches ID \"%s\", SW%4.4d, HW%4.4d",
        address, manufStr.c_str(), addrStr.c_str(), ident.c_str(), sw, hw);
    return RESULT_ERR_NOTFOUND;
  }

  // found the right file. load the templates if necessary, then load the file itself
  bool readCommon = readTemplates(manufStr, ".csv", hasTemplates);
  if (readCommon) {
    result = collectConfigFiles(manufStr, "", ".csv", &files, true, "&a=-");
    if (result == RESULT_OK && !files.empty()) {
      for (const auto& name : files) {
        string baseName = name.substr(manufStr.length()+1, name.length()-manufStr.length()-strlen(".csv"));  // *.
        if (baseName == "_templates.") {  // skip templates
          continue;
        }
        if (baseName.length() < 3 || baseName.find_first_of('.') != 2) {  // different from the scheme "ZZ."
          string errorDescription;
          result = loadDefinitionsFromConfigPath(m_messages, name, nullptr, &errorDescription);
          if (result == RESULT_OK) {
            logNotice(lf_main, "read common config file %s", name.c_str());
          } else {
            logError(lf_main, "error reading common config file %s: %s, %s", name.c_str(), getResultCode(result),
                errorDescription.c_str());
          }
        }
      }
    }
  }
  bestDefaults["name"] = ident;
  string errorDescription;
  result = loadDefinitionsFromConfigPath(m_messages, best, &bestDefaults, &errorDescription);
  if (result != RESULT_OK) {
    logError(lf_main, "error reading scan config file %s for ID \"%s\", SW%4.4d, HW%4.4d: %s, %s", best.c_str(),
        ident.c_str(), sw, hw, getResultCode(result), errorDescription.c_str());
    return result;
  }
  logNotice(lf_main, "read scan config file %s for ID \"%s\", SW%4.4d, HW%4.4d", best.c_str(), ident.c_str(), sw, hw);
  *relativeFile = best;
  return RESULT_OK;
}

bool ScanHelper::parseMessage(const string& arg, bool onlyMasterSlave, MasterSymbolString* master,
  SlaveSymbolString* slave) {
  size_t pos = arg.find_first_of('/');
  if (pos == string::npos) {
    logError(lf_main, "invalid message %s: missing \"/\"", arg.c_str());
    return false;
  }
  result_t result = master->parseHex(arg.substr(0, pos));
  if (result == RESULT_OK) {
    result = slave->parseHex(arg.substr(pos+1));
  }
  if (result != RESULT_OK) {
    logError(lf_main, "invalid message %s: %s", arg.c_str(), getResultCode(result));
    return false;
  }
  if (master->size() < 5) {  // skip QQ ZZ PB SB NN
    logError(lf_main, "invalid message %s: master part too short", arg.c_str());
    return false;
  }
  if (!isMaster((*master)[0])) {
    logError(lf_main, "invalid message %s: QQ is no master", arg.c_str());
    return false;
  }
  if (!isValidAddress((*master)[1], !onlyMasterSlave) || (onlyMasterSlave && isMaster((*master)[1]))) {
    logError(lf_main, "invalid message %s: ZZ is invalid", arg.c_str());
    return false;
  }
  return true;
}

}  // namespace ebusd

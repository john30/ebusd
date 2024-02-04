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

#ifndef EBUSD_SCAN_H_
#define EBUSD_SCAN_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "lib/ebus/data.h"
#include "lib/ebus/message.h"
#include "lib/ebus/result.h"
#include "lib/utils/httpclient.h"
#include "lib/utils/log.h"

namespace ebusd {

/** \file ebusd/scan.h
 * Helpers for handling device scanning and config loading.
 */

class BusHandler;

/**
 * Helper class for handling device scanning and config loading.
 */
class ScanHelper : public Resolver {
 public:
  /**
   * Constructor.
   * @param messages the @a MessageMap to load the messages into.
   * @param configPath the (optionally corrected) config path for retrieving configuration files from.
   * @param configLocalPrefix the path prefix (including trailing "/") for retrieving configuration files from local files (empty for HTTPS).
   * @param configUriPrefix the URI prefix (including trailing "/") for retrieving configuration files from HTTPS (empty for local files).
   * @param configLangQuery the optional language query part for retrieving configuration files from HTTPS (empty for local files). 
   * @param configHttpClient the @a HttpClient for retrieving configuration files from HTTPS.
   * @param verbose whether to verbosely log problems.
   */
  ScanHelper(MessageMap* messages,
  const string configPath, const string configLocalPrefix,
  const string configUriPrefix, const string configLangQuery,
  HttpClient* configHttpClient, bool verbose)
    : Resolver(), m_messages(messages),
    m_configPath(configPath), m_configLocalPrefix(configLocalPrefix),
    m_configUriPrefix(configUriPrefix), m_configLangQuery(configLangQuery),
    m_configHttpClient(configHttpClient), m_verbose(verbose) {}

  /**
   * Destructor.
   */
  virtual ~ScanHelper();

  /**
   * @return the (optionally corrected) config path for retrieving configuration files from.
   */
  const string getConfigPath() const { return m_configPath; }

  /**
   * Try to connect to the specified server.
   * @param host the host name to connect to.
   * @param port the port to connect to.
   * @param https true for HTTPS, false for HTTP.
   * @param timeout the timeout in seconds, defaults to 5 seconds.
   * @return true on success, false on connect failure.
   */
  bool connect(const string& host, uint16_t port, bool https = false, int timeout = 5);

  /**
   * Get the @a DataFieldTemplates for the specified configuration file.
   * @param filename the full name of the configuration file, or "*" to get the non-root templates with the longest name
   * or the root templates if not available.
   * @return the @a DataFieldTemplates.
   */
  virtual DataFieldTemplates* getTemplates(const string& filename);

  /**
   * Load the message definitions from configuration files.
   * @param recursive whether to load all files recursively.
   * @return the result code.
   */
  result_t loadConfigFiles(bool recursive = true);

  /**
   * Load the message definitions from a configuration file matching the scan result.
   * @param address the address of the scan participant
   * (either master for broadcast master data or slave for read slave data).
   * @param data the scan @a SlaveSymbolString for which to load the configuration file.
   * @param relativeFile the string in which the name of the configuration file is stored on success.
   * @return the result code.
   */
  result_t loadScanConfigFile(symbol_t address, string* relativeFile);

  /**
   * Helper method for executing all loaded and resolvable instructions.
   * @param busHandler the @a BusHandler instance.
   * @return the result code.
   */
  result_t executeInstructions(BusHandler* busHandler);

  /**
   * Helper method for loading definitions from a relative file from the config path/URL.
   * @param reader the @a FileReader instance to load with the definitions.
   * @param filename the relative name of the file being read.
   * @param defaults the default values by name (potentially overwritten by file name), or nullptr to not use defaults.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param replace whether to replace an already existing entry.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t loadDefinitionsFromConfigPath(FileReader* reader, const string& filename,
      map<string, string>* defaults, string* errorDescription, bool replace = false);

  /**
   * Helper method for parsing a master/slave message pair from a command line argument.
   * @param arg the argument to parse.
   * @param onlyMasterSlave true to parse only a MS message, false to also parse MM and BC message.
   * @param master the @a MasterSymbolString to parse into.
   * @param slave the @a SlaveSymbolString to parse into.
   * @return true when the argument was valid, false otherwise.
   */
  bool parseMessage(const string& arg, bool onlyMasterSlave, MasterSymbolString* master, SlaveSymbolString* slave);


 private:
  /**
   * Collect configuration files matching the prefix and extension from the specified path.
   * @param relPath the relative path from which to collect the files (without trailing "/").
   * @param prefix the filename prefix the files have to match, or empty.
   * @param extension the filename extension the files have to match.
   * @param files the @a vector to which to add the matching files.
   * @param query the query string suffix for HTTPS retrieval starting with "&", or empty.
   * @param dirs the @a vector to which to add found directories (without any name check), or nullptr to ignore.
   * @param hasTemplates the bool to set when the templates file was found in the path, or nullptr to ignore.
   * @return the result code.
   */
  result_t collectConfigFiles(const string& relPath, const string& prefix, const string& extension,
  vector<string>* files,
  bool ignoreAddressPrefix = false, const string& query = "",
  vector<string>* dirs = nullptr, bool* hasTemplates = nullptr);

  /**
   * Read the @a DataFieldTemplates for the specified path if necessary.
   * @param relPath the relative path from which to read the files (without trailing "/").
   * @param extension the filename extension of the files to read.
   * @param available whether the templates file is available in the path.
   * @return false when the templates for the path were already loaded before, true when the templates for the path were added (independent from @a available).
   * @return the @a DataFieldTemplates.
   */
  bool readTemplates(const string relPath, const string extension, bool available);

  /**
   * Dump the loaded @a DataFieldTemplates to the output.
   * @param outputFormat the @a OutputFormat options.
   * @param output the @a ostream to dump to.
   */
  void dumpTemplates(OutputFormat outputFormat, ostream* output) const;

  /**
   * Read the configuration files from the specified path.
   * @param relPath the relative path from which to read the files (without trailing "/").
   * @param extension the filename extension of the files to read.
   * @param recursive whether to load all files recursively.
   * @param errorDescription a string in which to store the error description in case of error.
   * @return the result code.
   */
  result_t readConfigFiles(const string& relPath, const string& extension, bool recursive,
  string* errorDescription);

  /** the @a MessageMap instance. */
  MessageMap* m_messages;

  /** the (optionally corrected) config path for retrieving configuration files from. */
  const string m_configPath;

  /** the path prefix (including trailing "/") for retrieving configuration files from local files (empty for HTTPS). */
  const string m_configLocalPrefix;

  /** the URI prefix (including trailing "/") for retrieving configuration files from HTTPS (empty for local files). */
  const string m_configUriPrefix;

  /** the optional language query part for retrieving configuration files from HTTPS (empty for local files). */
  const string m_configLangQuery;

  /** the @a HttpClient for retrieving configuration files from HTTPS. */
  HttpClient* m_configHttpClient;

  /** whether to verbosely log problems. */
  const bool m_verbose;

  /** the global @a DataFieldTemplates. */
  DataFieldTemplates m_globalTemplates;

  /**
   * the loaded @a DataFieldTemplates by relative path (may also carry
   * @a globalTemplates as replacement for missing file).
   */
  map<string, DataFieldTemplates*> m_templatesByPath;
};

}  // namespace ebusd

#endif  // EBUSD_SCAN_H_

/*
 * Copyright (C) John Baier 2014-2015 <ebusd@johnm.de>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBEBUS_FILEREADER_H_
#define LIBEBUS_FILEREADER_H_

#include "symbol.h"
#include "result.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

/** \file filereader.h */

using namespace std;

/** the separator character used between fields. */
#define FIELD_SEPARATOR ','

/** the separator character used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR '"'

extern void printErrorPos(vector<string>::iterator begin, const vector<string>::iterator end, vector<string>::iterator pos, string filename, size_t lineNo, result_t result);


/**
 * An abstract class that support reading definitions from a file.
 */
template<typename T>
class FileReader
{
public:

	/**
	 * Construct a new instance.
	 */
	FileReader(bool supportsDefaults)
			: m_supportsDefaults(supportsDefaults) {}

	/**
	 * Destructor.
	 */
	virtual ~FileReader() {}

	/**
	 * Read the definitions from a file.
	 * @param filename the name of the file being read.
	 * @param arg an argument to pass to @a addFromFile().
	 * @param verbose whether to verbosely log problems.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t readFromFile(const string filename, T arg=NULL, bool verbose=false)
	{
		ifstream ifs;
		ifs.open(filename.c_str(), ifstream::in);
		if (!ifs.is_open())
			return RESULT_ERR_NOTFOUND;

		string line;
		unsigned int lineNo = 0;
		vector<string> row;
		string token;
		vector< vector<string> > defaults;
		while (getline(ifs, line) != 0) {
			lineNo++;
			// skip empty lines and comments
			size_t length = line.length();
			if (length == 0 || line[0] == '#' || (line.length() > 1 && line[0] == '/' && line[1] == '/'))
				continue;

			row.clear();
			bool quotedText = false;
			ostringstream field;
			char prev = FIELD_SEPARATOR;
			for (size_t pos = 0; pos < length; pos++) {
				char ch = line[pos];
				switch (ch)
				{
				case FIELD_SEPARATOR:
					if (quotedText)
						field << ch;
					else {
						row.push_back(field.str());
						field.str("");
					}
					break;
				case TEXT_SEPARATOR:
					if (quotedText) {
						quotedText = false;
					}
					else if (prev == TEXT_SEPARATOR) { // double dquote
						quotedText = true;
						field << ch;
					}
					else if (prev == FIELD_SEPARATOR) {
						quotedText = true;
					}
					else
						field << ch;
					break;
				case '\r':
					break;
				default:
					field << ch;
					break;
				}
				prev = ch;
			}
			row.push_back(field.str());

			result_t result;
			vector<string>::iterator it = row.begin();
			const vector<string>::iterator end = row.end();
			if (m_supportsDefaults) {
				if (line[0] == '*') {
					row[0] = row[0].substr(1);
					defaults.push_back(row);
					continue;
				}
				result = addFromFile(it, end, arg, &defaults, filename, lineNo);
			}
			else
				result = addFromFile(it, end, arg, NULL, filename, lineNo);

			if (result != RESULT_OK) {
				if (!verbose) {
					ifs.close();
					return result;
				}
				printErrorPos(row.begin(), end, it, filename, lineNo, result);
			}
		}

		ifs.close();
		return RESULT_OK;
	}

	/**
	 * Add a definition that was read from a file.
	 * @param begin an iterator to the first column of the definition row to read.
	 * @param end the end iterator of the definition row to read.
	 * @param arg the argument passed to @a readFromFile().
	 * @param defaults all previously read default rows (initial star char removed), or NULL if not supported.
	 * @param filename the name of the file being read.
	 * @param lineNo the current line number in the file being read.
	 * @return @a RESULT_OK on success, or an error code.
	 */
	virtual result_t addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end, T arg, vector< vector<string> >* defaults, const string& filename, unsigned int lineNo) = 0;

private:

	/** whether this instance supports rows with defaults (starting with a star). */
	bool m_supportsDefaults;

};

#endif // LIBEBUS_FILEREADER_H_

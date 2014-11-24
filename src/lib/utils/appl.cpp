/*
 * Copyright (C) Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include "appl.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>

using namespace std;

Appl& Appl::Instance(const bool command, const bool argument)
{
	static Appl instance(command, argument);
	return instance;
}

Appl::~Appl()
{
	m_opts.clear();
	m_optvals.clear();
}

void Appl::addText(const char* text)
{
	opt_t opt;
	opt.name = "__text_only__";
	opt.shortname = "";
	opt.datatype = dt_none;
	opt.optiontype = ot_none;
	opt.description = text;
	m_opts.push_back(opt);
}

void Appl::addOption(const char* name, const char* shortname, OptVal optval,
		     DataType datatype, OptionType optiontype, const char* description)
{
	if (strlen(name) != 0) {

		m_optvals[name] = optval;

		opt_t opt;
		opt.name = name;
		opt.shortname = shortname;
		opt.datatype = datatype;
		opt.optiontype = optiontype;
		opt.description = description;
		m_opts.push_back(opt);
	}
}

void Appl::parseArgs(int argc, char* argv[])
{
	vector<string> _argv(argv, argv + argc);
	m_argv = _argv;

	// walk through all arguments
	for (int i = 1; i < argc; i++) {

		// find option with long format '--'
		if (_argv[i].rfind("--") == 0 && _argv[i].size() > 2) {

			// is next item an added argument?
			if (i+1 < argc && _argv[i+1].rfind("-", 0) == string::npos) {
				if (checkOption(_argv[i].substr(2), _argv[i+1]) == false)
					printHelp();
			}
			else {
				if (checkOption(_argv[i].substr(2), "") == false)
					printHelp();
			}

		// find option with short format '-'
		} else if (_argv[i].rfind("-") == 0 && _argv[i].size() > 1) {

			// walk through all characters
			for (size_t j = 1; j < _argv[i].size(); j++) {

				// only last charater could have an argument
				if (i+1 < argc && _argv[i+1].rfind("-", 0) == string::npos
				&& j+1 == _argv[i].size()) {
					if (checkOption(_argv[i].substr(j,1), _argv[i+1]) == false)
						printHelp();
				}
				else {
					if (checkOption(_argv[i].substr(j,1), "") == false)
						printHelp();
				}
			}
		}

	}

	// check command
	if (m_needCommand == true) {
		for (int i = 1; i < argc; i++) {

			if (_argv[i].rfind("-", 0) != string::npos) {
				i++;
				continue;
			}
			if (m_command.size() == 0)
				m_command = _argv[i];
			else
				m_arguments.push_back(_argv[i]);
		}

		if (m_command.size() == 0) {
			cerr << endl << "command needed" << endl;
			printHelp();
		}
	}

}

bool Appl::checkOption(const string& option, const string& value)
{
	if (strcmp(option.c_str(), "settings") == 0)
		printSettings();

	if (strcmp(option.c_str(), "version") == 0)
		printVersion();

	if (strcmp(option.c_str(), "h") == 0 || strcmp(option.c_str(), "help") == 0)
		printHelp();

	for (o_it = m_opts.begin(); o_it < m_opts.end(); o_it++) {
		if (o_it->shortname == option || o_it->name == option) {

			// need this option and argument?
			if (o_it->optiontype == ot_mandatory && value.size() == 0) {
				cerr << endl << "option requires an argument '"
					  << option << "'" << endl;
				return false;
			}

			// add given value to option
			if ((o_it->optiontype == ot_optional && value.size() != 0)
			|| o_it->optiontype != ot_optional)
				setOptVal(o_it->name, value, o_it->datatype);

			return true;
		}
	}

	cerr << endl << "unknown option '" << option << "'" << endl;
	return false;
}

void Appl::setOptVal(const char* option, const string value, DataType datatype)
{
	switch (datatype) {
	case dt_bool:
		m_optvals[option] = true;
		break;
	case dt_int:
		m_optvals[option] = strtol(value.c_str(), NULL, 10);
		break;
	case dt_long:
		m_optvals[option] = strtol(value.c_str(), NULL, 10);
		break;
	case dt_float:
		m_optvals[option] = static_cast<float>(strtod(value.c_str(), NULL));
		break;
	case dt_string:
		m_optvals[option] = value.c_str();
		break;
	default:
		break;
	}
}

void Appl::printVersion()
{
	cerr << m_version << endl;
	exit(EXIT_SUCCESS);
}

void Appl::printHelp()
{
	cerr << endl << "Usage:" << endl << "  "
		  << m_argv[0].substr(m_argv[0].find_last_of("/\\") + 1) << " [OPTIONS...]" ;

	if (m_needCommand == true)
		if (m_withArgument == true)
			cerr << " COMMAND {ARGS...}" << endl << endl;
		else
			cerr << " COMMAND" << endl << endl;
	else
		cerr << endl << endl;

	for (o_it = m_opts.begin(); o_it < m_opts.end(); o_it++) {
		if (strcmp(o_it->name, "__text_only__") == 0)
			cerr << o_it->description << endl;
		else {
			const char* c = (strlen(o_it->shortname) == 1) ? o_it->shortname : " ";
			cerr << ((strcmp(c, " ") == 0) ? " " : "-") << c
				  << " | --" << o_it->name
				  << "\t" << o_it->description
				  << endl;
		}
	}

	cerr << endl << "   | --settings\n   | --version\n-h | --help" << endl << endl;
	exit(EXIT_SUCCESS);
}

void Appl::printSettings()
{
	cerr << endl << "Settings:" << endl << endl;

	for (o_it = m_opts.begin(); o_it < m_opts.end(); o_it++) {
		if (strcmp(o_it->name, "__text_only__") == 0)
			continue;

		const char* c = (strlen(o_it->shortname) == 1) ? o_it->shortname : " ";
		cerr << ((strcmp(c, " ") == 0) ? " " : "-") << c
			  << " | --" << o_it->name
			  << " = ";
		if (o_it->datatype == dt_bool) {
			if (getOptVal<bool>(o_it->name) == true)
				cerr << "yes" << endl;
			else
				cerr << "no" << endl;
		}
		else if (o_it->datatype == dt_int) {
			cerr << getOptVal<int>(o_it->name) << endl;
		}
		else if (o_it->datatype == dt_long) {
			cerr << getOptVal<long>(o_it->name) << endl;
		}
		else if (o_it->datatype == dt_float) {
			cerr << getOptVal<float>(o_it->name) << endl;
		}
		else if (o_it->datatype == dt_string) {
			cerr << getOptVal<const char*>(o_it->name) << endl;
		}

	}

	cerr << endl;
	exit(EXIT_SUCCESS);
}

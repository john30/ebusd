/*
 * Copyright (C) Roland Jax 2012-2014 <roland.jax@liwest.at>
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

Appl& Appl::Instance()
{
	static Appl instance;
	return instance;
}

Appl::~Appl()
{
	m_args.clear();
	m_params.clear();
}

void Appl::addArgs(const std::string argTxt, const int argNum)
{
	m_argTxt = argTxt;
	m_argNum = argNum;
}

void Appl::addItem(const char* name, Param param, const char* shortname,
	     const char* longname, const char* description,
	     Datatype datatype, Optiontype optiontype)
{
	if (strlen(name) != 0)
		m_params[name] = param;

	if (strlen(longname) != 0) {
		Arg arg;
		arg.name = name;
		arg.shortname = shortname;
		arg.longname = longname;
		arg.description = description;
		arg.datatype = datatype;
		arg.optiontype = optiontype;
		m_args.push_back(arg);
	}
}

void Appl::printArgs()
{
	std::cerr << std::endl << "Usage:" << std::endl << "  "
		  << m_argv[0].substr(2) << " [Options]" ;

	if (m_argTxt.size() != 0)
		std::cerr << " " << m_argTxt;

	std::cerr << std::endl << std::endl << "Options:" << std::endl;

	for (a_it = m_args.begin(); a_it < m_args.end(); a_it++) {
		const char* c = (strlen(a_it->shortname) == 1) ? a_it->shortname : " ";
		std::cerr << ((strcmp(c, " ") == 0) ? " " : "-") << c
			  << " | --" << a_it->longname
			  << "\t" << a_it->description
			  << std::endl;
	}

	std::cerr << std::endl;
}

bool Appl::parseArgs(int argc, char* argv[])
{
	std::vector<std::string> _argv(argv, argv + argc);
	m_argc = argc;
	m_argv = _argv;

	for (int i = 1; i < m_argc; i++) {

		// find option with long format '--'
		if (m_argv[i].rfind("--") == 0 && m_argv[i].size() > 2) {
			// is next item an added argument?
			if (i+1 < m_argc && m_argv[i+1].rfind("-", 0) == std::string::npos) {
				if (checkArg(m_argv[i].substr(2), m_argv[i+1]) == false)
					return false;
			} else {
				if (checkArg(m_argv[i].substr(2), "") == false)
					return false;
			}

		// find option with short format '-'
		} else if (m_argv[i].rfind("-") == 0 && m_argv[i].size() > 1) {

			// walk through all characters
			for (size_t j = 1; j < m_argv[i].size(); j++) {

				// only last charater could have an argument
				if (i+1 < m_argc && m_argv[i+1].rfind("-", 0) == std::string::npos
				&& j+1 == m_argv[i].size()) {
					if (checkArg(m_argv[i].substr(j,1), m_argv[i+1]) == false)
						return false;
				} else {
					if (checkArg(m_argv[i].substr(j,1), "") == false)
						return false;
				}
			}
		}

	}

	// check args
	if (m_argNum > 0) {
		for (int i = 0; i <= m_argNum; i++)
			if (m_argv[m_argc - 1 - i].rfind("-", 0) != std::string::npos)
				return false;
	}

	return true;
}

void Appl::printSettings()
{
	std::cerr << std::endl << "Settings:" << std::endl;

	for (a_it = m_args.begin(); a_it < m_args.end(); a_it++) {
		const char* c = (strlen(a_it->shortname) == 1) ? a_it->shortname : " ";
		std::cerr << ((strcmp(c, " ") == 0) ? " " : "-") << c
			  << " | --" << a_it->longname
			  << " = ";
		if (a_it->datatype == type_bool) {
			if (getParam<bool>(a_it->name) == true)
				std::cerr << "yes" << std::endl;
			else
				std::cerr << "no" << std::endl;
		}
		else if (a_it->datatype == type_int) {
			std::cerr << getParam<int>(a_it->name) << std::endl;
		}
		else if (a_it->datatype == type_long) {
			std::cerr << getParam<long>(a_it->name) << std::endl;
		}
		else if (a_it->datatype == type_float) {
			std::cerr << getParam<float>(a_it->name) << std::endl;
		}
		else if (a_it->datatype == type_string) {
			std::cerr << getParam<const char*>(a_it->name) << std::endl;
		}

	}

	std::cerr << std::endl;
}

bool Appl::checkArg(const std::string& name, const std::string& arg)
{
	for (a_it = m_args.begin(); a_it < m_args.end(); a_it++) {
		if (a_it->shortname == name || a_it->longname == name) {
			if (a_it->optiontype == opt_mandatory && arg.size() == 0) {
				std::cerr << std::endl << "option requires an argument '"
					  << name << "'" << std::endl;
				return false;
			}

			if ((a_it->optiontype == opt_optional && arg.size() != 0)
			|| a_it->optiontype != opt_optional)
				addParam(a_it->name, arg, a_it->datatype);

			return true;
		}
	}

	std::cerr << m_argv[0].substr(2) << ": Unknown Option -- " << name << std::endl;
	return false;
}

void Appl::addParam(const char* name, const std::string arg, Datatype datatype)
{
	switch (datatype) {
	case type_bool:
		m_params[name] = true;
		break;
	case type_int:
		m_params[name] = strtol(arg.c_str(), NULL, 10);
		break;
	case type_long:
		m_params[name] = strtol(arg.c_str(), NULL, 10);
		break;
	case type_float:
		m_params[name] = static_cast<float>(strtod(arg.c_str(), NULL));
		break;
	case type_string:
		m_params[name] = arg.c_str();
		break;
	default:
		break;
	}
}



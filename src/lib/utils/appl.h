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

#ifndef LIBUTILS_APPL_H_
#define LIBUTILS_APPL_H_

#include <string>
#include <cstring>
#include <map>
#include <vector>

/** the available data types. */
enum DataType {
	dt_none,       // default value
	dt_bool,       //
	dt_int,        //
	dt_long,       //
	dt_float,      //
	dt_string,     //
};

/** option types. */
enum OptionType {
	ot_none,       // default value
	ot_optional,   // a value is optional
	ot_mandatory,  // a value is mandatory
};

/** structure for defining application options */
typedef struct opt {
	const char* name;
	const char* shortname;
	const char* description;
	DataType datatype;
	OptionType optiontype;
} opt_t;

/** union for option values */
union OptVal {
	bool b;
	int i;
	long l;
	float f;
	const char* c;

	OptVal() { memset(this, 0, sizeof(OptVal)); }
	OptVal(bool _b) : b(_b) {}
	OptVal(int _i) : i(_i) {}
	OptVal(long _l) : l(_l) {}
	OptVal(float _f) : f(_f) {}
	OptVal(const char* _c) : c(_c) {}
};


/**
 * @brief class for all kinds of application parameters.
 */
class Appl
{

public:
	/**
	 * @brief create an appl instance and return the reference.
	 * @param command is true if an command is needed.
	 * @return the reference to instance.
	 */
	static Appl& Instance(const bool command=false);

	/**
	 * @brief destructor.
	 */
	~Appl();

	/**
	 * @brief save application version string.
	 * @param version string
	 */
	void setVersion(const char* version) { m_version = version; }

	/**
	 * @brief create new entry of application option only for help page.
	 * @param text string to print
	 */
	void addText(const char* text);

	/**
	 * @brief create new entry of application option.
	 * @param name the long name
	 * @param shortname optional short name
	 * @param optval value of option
	 * @param datatype data type of option value
	 * @param optiontype type of given option
	 * @param description hint text for help page
	 */
	void addOption(const char* name, const char* shortname, OptVal optval,
		       DataType datatype, OptionType optiontype, const char* description);

	/**
	 * @brief returns the value of the interested option
	 * @param name the interested option
	 * @return casted value
	 */
	template <typename T>
	T getOptVal(const char* name)
	{
		ov_it = m_optvals.find(name);
		return (reinterpret_cast<T&>(ov_it->second));
	}

	/**
	 * @brief parse application arguments.
	 * @param argc the number of options
	 * @param argv the given options
	 */
	void parseArgs(int argc, char* argv[]);

	/**
	 * @brief returns the number of saved commands and arguments
	 * @return number of commands and arguments
	 */
	int numArgs() const { return m_arguments.size(); }

	/**
	 * @brief returns the string of an interested argument (0 = command)
	 * @param num number of interested argument
	 * @return string value
	 */
	std::string getArg(const int num) const { return m_arguments[num]; }

private:
	/** private constructor - singleton pattern*/
	Appl(const bool command) : m_needCommand(command) {}
	Appl(const Appl&);
	Appl& operator=(const Appl&);

	/** application options */
	std::vector<opt_t> m_opts;
	std::vector<opt_t>::const_iterator o_it;

	/** map option - value */
	std::map<const char*, OptVal> m_optvals;
	std::map<const char*, OptVal>::iterator ov_it;

	/** given arguments */
	std::vector<std::string> m_argv;

	/** application version string */
	const char* m_version;

	/** true if the application need a command */
	bool m_needCommand;

	/** arguments (argument 0 = command) string */
	std::vector<std::string> m_arguments;

	/**
	 * @brief checks the passed parameter if this is a valid option.
	 * @param option to check
	 * @param value to save if paramter is a valid option
	 */
	bool checkOption(const std::string& option, const std::string& value);

	/**
	 * @brief save the passed value to option.
	 * @param option name
	 * @param value to save
	 * @param datatype of given option
	 */
	void setOptVal(const char* option, const std::string value, DataType datatype);

	/**
	 * @brief print application version.
	 */
	void printVersion();

	/**
	 * @brief print help page.
	 */
	void printHelp();

	/**
	 * @brief print used option settings.
	 */
	void printSettings();
};

#endif // LIBUTILS_APPL_H_

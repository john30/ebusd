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

/** \file appl.h */

using namespace std;

/** @brief the available data types. */
enum DataType {
	dt_none,    /*!< default for __text_only__ */
	dt_bool,    /*!< boolean */
	dt_hex,     /*!< hex integer */
	dt_int,     /*!< dec integer */
	dt_long,    /*!< long */
	dt_float,   /*!< float */
	dt_string   /*!< string */
};

/** @brief option types. */
enum OptionType {
	ot_none,      /*!< no option type is needed */
	ot_optional,  /*!< a value is optional */
	ot_mandatory  /*!< a value is mandatory */
};

/** @brief structure for defining application options */
typedef struct {
	/** long option name */
	const char* name;
	/** short option name */
	const char* shortname;
	/** description for this option */
	const char* description;
	/** data type for this option */
	DataType datatype;
	/** indicates whether an option takes an argument */
	OptionType optiontype;
} opt_t;

/**
 * @brief union for option values
 */
union OptVal {
	/** boolean */
	bool b;
	/** integer */
	int i;
	/** long */
	long l;
	/** float */
	float f;
	/** string */
	const char* c;

	/**
	 * @brief clear memory
	 */
	OptVal() { memset(this, 0, sizeof(OptVal)); }
	/**
	 * @brief create boolean type
	 * @param _b the boolean
	 */
	OptVal(bool _b) : b(_b) {}
	/**
	 * @brief create integer type
	 * @param _i the integer
	 */
	OptVal(int _i) : i(_i) {}
	/**
	 * @brief create long type
	 * @param _l the long
	 */
	OptVal(long _l) : l(_l) {}
	/**
	 * @brief create float type
	 * @param _f the float
	 */
	OptVal(float _f) : f(_f) {}
	/**
	 * @brief create string type
	 * @param _c the string
	 */
	OptVal(const char* _c) : c(_c) {}
};


/**
 * @brief class for all kinds of application parameters.
 */
class Appl
{

public:
	/**
	 * @brief create an instance and return the reference.
	 * @param command string for help page.
	 * @param argument string for help page.
	 * @return the reference to instance.
	 */
	static Appl& Instance(const char* command="", const char* argument="");

	/**
	 * @brief destructor.
	 */
	~Appl();

	/**
	 * @brief save application version string.
	 * @param version string.
	 */
	void setVersion(const char* version) { m_version = version; }

	/**
	 * @brief create new entry of application option only for help page.
	 * @param text string to print.
	 */
	void addText(const char* text);

	/**
	 * @brief create new entry of application option.
	 * @param name the long name.
	 * @param shortname optional short name.
	 * @param optval value of option.
	 * @param datatype data type of option value.
	 * @param optiontype type of given option.
	 * @param description hint text for help page.
	 */
	void addOption(const char* name, const char* shortname, OptVal optval,
		       DataType datatype, OptionType optiontype, const char* description);

	/**
	 * @brief returns the value of the interested option.
	 * @param name the interested option.
	 * @return casted value.
	 */
	template <typename T>
	T getOptVal(const char* name)
	{
		ov_it = m_optvals.find(name);
		return (reinterpret_cast<T&>(ov_it->second));
	}

	/**
	 * @brief parse application arguments.
	 * @param argc the number of options.
	 * @param argv the given options.
	 * @return false if the arguments are invalid or a help/error message was printed and the process should exit.
	 */
	bool parseArgs(int argc, char* argv[]);

	/**
	 * @brief returns the number of saved commands and arguments.
	 * @return number of commands and arguments.
	 */
	int numArgs() const { return m_arguments.size(); }
	/**
	 * @brief returns the string of an interested argument.
	 * @param num number of interested argument.
	 * @return the argument string.
	 */
	string getArg(const int num) const { return m_arguments[num]; }

	/**
	 * @brief returns the string of given command.
	 * @return the command string.
	 */
	string getCommand() const { return m_command; }

	/**
	 * @brief returns the string of given command.
	 * @return the command string.
	 */
	bool missingCommand() const { return (m_command.size() == 0 ? true : false); }

private:
	/**
	 * @brief private construtor.
	 * @param command string for help page.
	 * @param argument string for help page.
	 */
	Appl(const char* command, const char* argument)
		: m_version(NULL), m_withCommand(command), m_withArgument(argument) {}

	/**
	 * @brief private copy construtor.
	 * @param reference to an instance.
	 */
	Appl(const Appl&);

	/**
	 * @brief private = operator.
	 * @param reference to an instance.
	 * @return reference to instance.
	 */
	Appl& operator=(const Appl&);

	/** application options */
	vector<opt_t> m_opts;

	/** application options iterator */
	vector<opt_t>::const_iterator o_it;

	/** map option - value */
	map<const char*, OptVal> m_optvals;

	/** map option - value iterator */
	map<const char*, OptVal>::iterator ov_it;

	/** given arguments */
	vector<string> m_argv;

	/** application version string */
	const char* m_version;

	/** command string for help page */
	const char* m_withCommand;

	/** argument string for help page */
	const char* m_withArgument;

	/** command (argument 0 = command) string */
	string m_command;

	/** arguments (argument >= 1) string */
	vector<string> m_arguments;

	/**
	 * @brief checks the passed parameter if this is a valid option.
	 * @param option to check.
	 * @param value to save.
	 * @return true if parameter is a valid option.
	 */
	bool checkOption(const string& option, const string& value);

	/**
	 * @brief save the passed value to option.
	 * @param option name.
	 * @param value to save.
	 * @param datatype of given option.
	 */
	void setOptVal(const char* option, const string value, DataType datatype);

	/**
	 * @brief print application version.
	 * @return false.
	 */
	bool printVersion();

	/**
	 * @brief print help page.
	 * @return false.
	 */
	bool printHelp();

	/**
	 * @brief print used option settings.
	 * @return false.
	 */
	bool printSettings();
};

#endif // LIBUTILS_APPL_H_

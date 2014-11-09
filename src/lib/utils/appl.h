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

class Appl
{

private:
	struct Option;

public:
	enum Datatype { type_none, type_bool, type_int, type_long, type_float, type_string };
	enum Optiontype { opt_none, opt_optional, opt_mandatory };

	union Param {
		bool b;
		int i;
		long l;
		float f;
		const char* c;

		Param() { memset(this, 0, sizeof(Param)); }
		Param(bool _b) : b(_b) {}
		Param(int _i) : i(_i) {}
		Param(long _l) : l(_l) {}
		Param(float _f) : f(_f) {}
		Param(const char* _c) : c(_c) {}
	};

	template <typename T>
	T getParam(const char* name)
	{
		p_it = m_params.find(name);
		return (reinterpret_cast<T&>(p_it->second));
	}

	static Appl& Instance();

	~Appl();

	void addArgs(const std::string argTxt, const int argNum);
	size_t numArg() const { return m_argValues.size(); }
	std::string getArg(const int argNum) const { return m_argValues[argNum]; }

	void addItem(const char* name, Param param, const char* shortname,
		     const char* longname, const char* description,
		     Datatype datatype, Optiontype optiontype);

	void printArgs();

	bool parseArgs(int argc, char* argv[]);
	void printSettings();

private:
	Appl() {}
	Appl(const Appl&);
	Appl& operator= (const Appl&);

	struct Arg {
		const char* name;
		const char* shortname;
		const char* longname;
		const char* description;
		Datatype datatype;
		Optiontype optiontype;
	};

	size_t m_argc;
	std::vector<std::string> m_argv;

	std::vector<Arg> m_args;
	std::vector<Arg>::const_iterator a_it;

	std::map<const char*, Param> m_params;
	std::map<const char*, Param>::iterator p_it;

	std::string m_argTxt;
	size_t m_argNum;

	std::vector<std::string> m_argValues;

	bool checkArg(const std::string& name, const std::string& arg);

	void addParam(const char* name, Param param) { m_params[name] = param; }

	void addParam(const char* name, const std::string arg, Datatype datatype);
};

#endif // LIBUTILS_APPL_H_

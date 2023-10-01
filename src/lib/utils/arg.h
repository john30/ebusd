/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2023 John Baier <ebusd@ebusd.eu>
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

#ifndef LIB_UTILS_ARG_H_
#define LIB_UTILS_ARG_H_

namespace ebusd {

/** \file lib/utils/args.h */

/** the available arg flags. */
enum ArgFlag {
  af_optional = 1<<0,  //!< optional argument value
  af_noHelp = 1<<1,  //!< do not include -?/--help option
  af_noVersion = 1<<2,  //!< do not include -V/--version option
};

/** Definition of a single argument. */
typedef struct argDef {
  const char* name;  //!< the (long) name of the argument, or nullptr for a group header
  int key;  //!< the argument key, also used as short name if alphabetic or the question mark
  const char* valueName;  //!< the optional argument value name
  int flags;  //!< flags for the argument, bit combination of @a ArgFlag
  const char* help;  //!< help text (mandatory)
  int unused;  //!< currently unused (kept for compatibility to argp)
} argDef;

struct argParseOpt;

/**
 * Function to be called for each argument.
 * @param key the argument key as defined.
 * @param arg the argument value, or nullptr.
 * @param parseOpt ppointer to the @a argParseOpt structure.
 * @return 0 on success, non-zero otherwise.
 */
typedef int (*parse_function_t)(int key, char *arg, const struct argParseOpt *parseOpt);

/** Options for child definitions. */
typedef struct argParseChildOpt {
  const argDef *argDefs;  //!< pointer to the argument defintions (last one needs to have nullptr help as end sign)
  parse_function_t parser;  //!< parse function to use
} argParseChildOpt;

/** Options to pass to @a argParse(). */
typedef struct argParseOpt {
  const argDef *argDefs;  //!< pointer to the argument defintions (last one needs to have nullptr help as end sign)
  parse_function_t parser;  //!< parse function to use
  int flags;  //!< flags for the parser, bit combination of @a ArgFlag
  const char* name;  //!< name of the program parsed
  const char* positional;  //!< help text for optional positional argument
  const char* help;  //!< help text for the program (second line of help output)
  const char* suffix;  //!< optional help suffix text
  const argParseChildOpt *childOpts;  //!< optional child definitions
  void* userArg;  //!< optional user argument
} argParseOpt;

/**
 * Parse the arguments given in @a argv.
 * @param parseOpt pointer to the @a argParseOpt structure.
 * @param argc the argument count (including the full program name in index 0).
 * @param argv the argument values (including the full program name in index 0).
 * @param argIndex optional pointer for storing the index to the first non-argument found in argv.
 * @return 0 on success, '!' for an invalid argument value, ':' for a missing argument value,
 * '?' when "-?" was given, or the result of the parse function if non-zero.
 */
int argParse(const argParseOpt *parseOpt, int argc, char **argv, int *argIndex);

/**
 * Print the help text.
 * @param parseOpt ppointer to the @a argParseOpt structure.
 */
void argHelp(const argParseOpt *parseOpt);

/**
 * Convenience macro to print an error message to stderr.
*/
#define argParseError(argParseOpt, message) fprintf(stderr, "%s\n", message);

}  // namespace ebusd

#endif  // LIB_UTILS_ARG_H_

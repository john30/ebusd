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

#include "lib/utils/arg.h"

#include <getopt.h>
#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

namespace ebusd {

#define isAlpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

void calcCounts(const argDef *argDefs, int *count, int *shortCharsCount, int *shortOptsCount) {
  for (const argDef *arg = argDefs; arg && arg->help; arg++) {
    if (!arg->name) {
      continue;
    }
    (*count)++;
    if (!isAlpha(arg->key)) {
      continue;
    }
    (*shortCharsCount)++;
    (*shortOptsCount)++;
    if (arg->valueName) {
      (*shortOptsCount)++;
      if (arg->flags & af_optional) {
        (*shortOptsCount)++;
      }
    }
  }
}

void buildOpts(const argDef *argDefs, int *count, int *shortCharsCount, int *shortOptsCount,
  struct option *longOpts, char *shortChars, int *shortIndexes, char *shortOpts, int argDefIdx) {
  struct option *opt = longOpts+(*count);
  for (const argDef *arg = argDefs; arg && arg->help; arg++, argDefIdx++) {
    if (!arg->name) {
      continue;
    }
    opt->name = arg->name;
    opt->has_arg = arg->valueName ? ((arg->flags & af_optional) ? optional_argument : required_argument) : no_argument;
    opt->flag = NULL;
    opt->val = argDefIdx;
    if (isAlpha(arg->key)) {
      shortChars[(*shortCharsCount)] = static_cast<char>(arg->key);
      shortIndexes[(*shortCharsCount)++] = *count;
      shortOpts[(*shortOptsCount)++] = static_cast<char>(arg->key);
      if (arg->valueName) {
        shortOpts[(*shortOptsCount)++] = ':';
        if (arg->flags & af_optional) {
          shortOpts[(*shortOptsCount)++] = ':';
        }
      }
    }
    opt++;
    (*count)++;
  }
}

static const argDef endArgDef = {nullptr, 0, nullptr, 0, nullptr};
static const argDef helpArgDef = {"help", '?', nullptr, 0, "Give this help list"};
static const argDef helpArgDefs[] = {
  helpArgDef,
  endArgDef
};
static const argDef versionArgDef = {"version", 'V', nullptr, 0, "Print program version"};
static const argDef versionArgDefs[] = {
  versionArgDef,
  endArgDef
};

int argParse(const argParseOpt *parseOpt, int argc, char **argv, void* userArg) {
  int count = 0, shortCharsCount = 0, shortOptsCount = 0;
  if (!(parseOpt->flags & af_noHelp)) {
    calcCounts(helpArgDefs, &count, &shortCharsCount, &shortOptsCount);
  }
  if (!(parseOpt->flags & af_noVersion)) {
    calcCounts(versionArgDefs, &count, &shortCharsCount, &shortOptsCount);
  }
  calcCounts(parseOpt->argDefs, &count, &shortCharsCount, &shortOptsCount);
  for (const argParseChildOpt *child = parseOpt->childOpts; child && child->argDefs; child++) {
    calcCounts(child->argDefs, &count, &shortCharsCount, &shortOptsCount);
  }
  struct option *longOpts = (struct option*)calloc(count+1, sizeof(struct option));  // room for EOF
  char *shortChars = reinterpret_cast<char*>(calloc(shortCharsCount+1, sizeof(char)));  // room for \0
  int *shortIndexes = reinterpret_cast<int*>(calloc(shortCharsCount, sizeof(int)));
  char *shortOpts = reinterpret_cast<char*>(calloc(2+shortOptsCount+1, sizeof(char)));  // room for +, :, and \0
  count = 0;
  shortCharsCount = 0;
  shortOptsCount = 0;
  shortOpts[shortOptsCount++] = '+';  // posix mode to stop at first non-option
  shortOpts[shortOptsCount++] = ':';  // return ':' for missing option
  if (!(parseOpt->flags & af_noHelp)) {
    buildOpts(helpArgDefs, &count, &shortCharsCount, &shortOptsCount, longOpts, shortChars,
      shortIndexes, shortOpts, 0xff00);
  }
  if (!(parseOpt->flags & af_noVersion)) {
    buildOpts(versionArgDefs, &count, &shortCharsCount, &shortOptsCount, longOpts, shortChars,
      shortIndexes, shortOpts, 0xff01);
  }
  buildOpts(parseOpt->argDefs, &count, &shortCharsCount, &shortOptsCount, longOpts, shortChars,
    shortIndexes, shortOpts, 0);
  int children = 0;
  for (const argParseChildOpt *child = parseOpt->childOpts; child && child->argDefs; child++) {
    buildOpts(child->argDefs, &count, &shortCharsCount, &shortOptsCount, longOpts, shortChars,
      shortIndexes, shortOpts, 0x100*(++children));
  }
  optind = 1;  // setting to 0 does not work
  int c = 0, longIdx = -1, ret = 0;
  while ((c = getopt_long(argc, argv, shortOpts, longOpts, &longIdx)) != -1) {
    if (c == '?') {
      // unknown option or help
      if (optopt != '?') {
        ret = '!';
        fprintf(stderr, "invalid argument %s\n", argv[optind - 1]);
      } else {
        ret = c;
      }
      break;
    }
    if (c == ':') {
      // missing option
      fprintf(stderr, "missing argument to %s\n", argv[optind - 1]);
      ret = c;
      break;
    }
    if (isAlpha(c)) {
      // short name
      int idx = static_cast<int>(strchr(shortChars, c) - shortChars);
      if (idx >= 0 && idx < shortCharsCount) {
        longIdx = shortIndexes[idx];
      } else {
        longIdx = -1;
      }
    } else if (c >= 0 && longIdx < 0) {
      longIdx = c;
    }
    if (longIdx < 0 || longIdx >= count) {
      ret = '!';  // error
      break;
    }
    int val = longOpts[longIdx].val;
    if (val == 0xff00) {  // help
      ret = '?';
      break;
    }
    if (val == 0xff01) {  // version
      ret = 'V';
      break;
    }
    const argDef *argDefs;
    parse_function_t parser;
    if (val & 0xff00) {
      const argParseChildOpt *child = parseOpt->childOpts + ((val>>8)-1);
      argDefs = child->argDefs;
      parser = child->parser;
    } else {
      argDefs = parseOpt->argDefs;
      parser = parseOpt->parser;
    }
    const argDef *arg = argDefs + (val & 0xff);
    c = parser(arg->key, optarg, parseOpt, userArg);
    if (c != 0) {
      ret = c;
      break;
    }
  }
  if (ret == 0) {
    // check for positionals
    for (const argDef *arg = parseOpt->argDefs; arg && arg->help; arg++) {
      if (arg->name || !arg->valueName) {
        continue;  // short/long arg or group
      }
      if (optind < argc) {
        int key = arg->key;
        do {
          c = parseOpt->parser(key, argv[optind], parseOpt, userArg);
          if (c != 0) {
            ret = c;
            break;
          }
          if (optind+1 >= argc || !(arg->flags & af_multiple)) {
            break;
          }
          key++;
          optind++;
        } while (true);
        if (ret != 0) {
          break;
        }
      } else if (!(arg->flags & af_optional)) {
        ret = ':';  // missing argument
        fprintf(stderr, "missing argument\n");
        break;
      }
      optind++;
    }
    if (ret == 0 && optind < argc) {
      ret = '!';  // extra unexpected argument
      fprintf(stderr, "extra argument %s\n", argv[optind]);
    }
  }
  if (ret == '?') {
    argHelp(argv[0], parseOpt);
  }
  free(longOpts);
  free(shortChars);
  free(shortOpts);
  return ret;
}

#define MIN_INDENT 18
#define MAX_INDENT 29
#define MAX_BREAK 79

void wrap(const char* str, size_t pos, size_t indent) {
  const char* end = strchr(str, 0);
  const char* eol = strchr(str, '\n');
  char buf[MAX_BREAK + 1];
  bool first = true;
  while (*str && str < end) {
    if (!first) {
      if (indent) {
        printf("%*c", static_cast<int>(indent), ' ');
      }
      pos = indent;
    }
    // start from max position backwards to find a break char
    size_t cnt = MAX_BREAK - pos;
    if (eol && eol < str) {
      eol = strchr(str, '\n');
    }
    if (eol && eol < str + cnt) {
      // EOL is before latest possible break
      cnt = eol - str;
    } else if (end < str + cnt) {
      cnt = end - str;
    }
    for (; cnt > 0; cnt--) {
      char ch = str[cnt];
      if (ch == ' ' || ch == '\n' || ch == 0) {
        // break found
        buf[0] = 0;
        strncat(buf, str, cnt);
        printf("%s\n", buf);
        str += cnt;
        if (*str) {
          str++;
        }
        break;  // restart
      }
    }
    if (cnt == 0 && *str) {
      // final
      printf("%s\n", str);
      break;
    }
    first = false;
  }
}

size_t calcIndent(const argDef *argDefs) {
  size_t indent = 0;
  for (const argDef *arg = argDefs; arg && arg->help; arg++) {
    if (!arg->name) {
      continue;
    }
    // e.g. "  -d, --device=DEV       Use DEV..."
    size_t length = 2 + 3 + 3 + strlen(arg->name) + 2;
    if (arg->valueName) {
      length += 1 + strlen(arg->valueName);
      if (arg->flags & af_optional) {
        length += 2;
      }
    }
    if (length > indent) {
      indent = length;
      if (indent > MAX_INDENT) {
        return indent;
      }
    }
  }
  return indent;
}

void printArgs(const argDef *argDefs, size_t indent) {
  for (const argDef *arg = argDefs; arg && arg->help; arg++) {
    if (!arg->name && !arg->valueName) {
      if (*arg->help) {
        printf("\n %s\n", arg->help);
      } else {
        printf("\n");
      }
      continue;
    }
    printf("  ");
    if (isAlpha(arg->key) || arg->key == '?') {
      printf("-%c,", arg->key);
    } else {
      printf("   ");
    }
    size_t taken = 2 + 3 + 3;
    if (arg->name) {
      printf(" --%s", arg->name);
      taken += strlen(arg->name);
    } else {
      printf("   ");
    }
    if (arg->valueName) {
      bool multi = arg->flags & af_multiple;
      taken += (arg->name ? 1 : 0) + strlen(arg->valueName) + (multi ? 3 : 0);
      if (arg->flags & af_optional) {
        printf("[%s%s%s]", arg->name ? "=" : "", arg->valueName, multi ? "..." : "");
        taken += 2;
      } else {
        printf("%s%s%s", arg->name ? "=" : "", arg->valueName, multi ? "..." : "");
      }
    }
    if (taken > indent) {
      printf(" ");
      wrap(arg->help, taken+1, indent);
    } else {
      printf("%*c", static_cast<int>(indent - taken), ' ');
      wrap(arg->help, indent, indent);
    }
  }
}

void argHelp(const char* name, const argParseOpt *parseOpt) {
  size_t indent = calcIndent(parseOpt->argDefs);
  if (indent < MAX_INDENT) {
    for (const argParseChildOpt *child = parseOpt->childOpts; child && child->argDefs; child++) {
      size_t childIndent = calcIndent(child->argDefs);
      if (childIndent > indent) {
        indent = childIndent;
        if (indent > MAX_INDENT) {
          break;
        }
      }
    }
  }
  if (indent > MAX_INDENT) {
    indent = MAX_INDENT;
  } else if (indent < MIN_INDENT) {
    indent = MIN_INDENT;
  }
  printf("Usage: %s [OPTION...]", basename(name));
  for (const argDef *arg = parseOpt->argDefs; arg && arg->help; arg++) {
    if (arg->name || !arg->valueName) {
      continue;
    }
    bool multi = arg->flags & af_multiple;
    if (arg->flags & af_optional) {
      printf(" [%s%s]", arg->valueName, multi ? "..." : "");
    } else {
      printf(" %s%s", arg->valueName, multi ? "..." : "");
    }
  }
  printf("\n");
  wrap(parseOpt->help, 0, 0);
  printArgs(parseOpt->argDefs, indent);
  for (const argParseChildOpt *child = parseOpt->childOpts; child && child->argDefs; child++) {
    printArgs(child->argDefs, indent);
  }
  if (!(parseOpt->flags & (af_noHelp|af_noVersion))) {
    printf("\n");
    if (!(parseOpt->flags & af_noHelp)) {
      printArgs(helpArgDefs, indent);
    }
    if (!(parseOpt->flags & af_noVersion)) {
      printArgs(versionArgDefs, indent);
    }
  }
  if (parseOpt->suffix) {
    printf("\n");
    wrap(parseOpt->suffix, 0, 0);
  }
  fflush(stdout);
}

}  // namespace ebusd

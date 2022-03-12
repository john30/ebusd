/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2022 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include <argp.h>
#include <string.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include "lib/utils/tcpsocket.h"

namespace ebusd {

using std::ostringstream;
using std::cin;
using std::cout;
using std::string;
using std::endl;

/** A structure holding all program options. */
struct options {
  const char* server;     //!< ebusd server host (name or ip) [localhost]
  uint16_t port;          //!< ebusd server port [8888]
  uint16_t timeout;       //!< ebusd connect/send/receive timeout

  char* const *args;      //!< arguments to pass to ebusd
  unsigned int argCount;  //!< number of arguments to pass to ebusd
};

/** the program options. */
static struct options opt = {
  "localhost",  // server
  8888,         // port
  60,            // timeout

  nullptr,         // args
  0             // argCount
};

/** the version string of the program. */
const char *argp_program_version = "ebusctl of """ PACKAGE_STRING "";

/** the report bugs to address of the program. */
const char *argp_program_bug_address = "" PACKAGE_BUGREPORT "";

/** the documentation of the program. */
static const char argpdoc[] =
  "Client for acessing " PACKAGE " via TCP.\n"
  "\v"
  "If given, send COMMAND together with CMDOPT options to " PACKAGE ".\n"
  "Use 'help' as COMMAND for help on available " PACKAGE " commands.";

/** the description of the accepted arguments. */
static char argpargsdoc[] = "\nCOMMAND [CMDOPT...]";

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
  {nullptr,     0, nullptr, 0, "Options:", 1 },
  {"server",  's', "HOST",  0, "Connect to " PACKAGE " on HOST (name or IP) [localhost]", 0 },
  {"port",    'p', "PORT",  0, "Connect to " PACKAGE " on PORT [8888]", 0 },
  {"timeout", 't', "SECS",  0, "Timeout for connecting to/receiving from " PACKAGE
                               ", 0 for none [60]", 0 },

  {nullptr,     0, nullptr, 0, nullptr, 0 },
};

/**
 * The program argument parsing function.
 * @param key the key from @a argpoptions.
 * @param arg the option argument, or nullptr.
 * @param state the parsing state.
 */
error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct options *opt = (struct options*)state->input;
  char* strEnd = nullptr;
  unsigned int value;
  switch (key) {
  // Device settings:
  case 's':  // --server=localhost
    if (arg == nullptr || arg[0] == 0) {
      argp_error(state, "invalid server");
      return EINVAL;
    }
    opt->server = arg;
    break;
  case 'p':  // --port=8888
    value = strtoul(arg, &strEnd, 10);
    if (strEnd == nullptr || strEnd == arg || *strEnd != 0 || value < 1 || value > 65535) {
      argp_error(state, "invalid port");
      return EINVAL;
    }
    opt->port = (uint16_t)value;
    break;
  case 't':  // --timeout=10
    value = strtoul(arg, &strEnd, 10);
    if (strEnd == nullptr || strEnd == arg || *strEnd != 0 || value > 3600) {
      argp_error(state, "invalid timeout");
      return EINVAL;
    }
    opt->timeout = (uint16_t)value;
    break;
  case ARGP_KEY_ARGS:
    opt->args = state->argv + state->next;
    opt->argCount = state->argc - state->next;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

string fetchData(ebusd::TCPSocket* socket, bool &listening, uint16_t timeout, bool &errored) {
  char data[1024];
  ssize_t datalen;
  ostringstream ostream;
  string message, sendmessage;

  int ret = 0;
  struct timespec tdiff;

  // set timeout
  tdiff.tv_sec = 0;
  tdiff.tv_nsec = 200000000;  // 200 ms
  time_t now;
  time(&now);
  time_t endTime = now + timeout;
#ifdef HAVE_PPOLL
  int nfds = 2;
  struct pollfd fds[nfds];

  memset(fds, 0, sizeof(fds));

#define IDX_STDIN 1
#define IDX_SOCK 0

  fds[IDX_STDIN].fd = STDIN_FILENO;
  fds[IDX_STDIN].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

  fds[IDX_SOCK].fd = socket->getFD();
  fds[IDX_SOCK].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
#else
#ifdef HAVE_PSELECT
  int maxfd;
  fd_set checkfds;

  FD_ZERO(&checkfds);
  FD_SET(STDIN_FILENO, &checkfds);
  FD_SET(socket->getFD(), &checkfds);
  maxfd = STDIN_FILENO;
  if (socket->getFD() > maxfd) {
    maxfd = socket->getFD();
  }
#endif
#endif

  bool inputClosed = false;
  while (!errored && time(&now) && now < endTime) {
#ifdef HAVE_PPOLL
    // wait for new fd event
    ret = ppoll(fds, nfds, &tdiff, nullptr);
    if (ret < 0) {
      perror("ebusctl poll");
      errored = true;
      break;
    }
    if (ret > 0 && ((fds[IDX_STDIN].revents & POLLERR) || (fds[IDX_SOCK].revents & POLLERR))) {
      errored = true;
      break;
    }
    if (ret > 0 && (fds[IDX_STDIN].revents & (POLLHUP | POLLRDHUP))) {
      inputClosed = true;  // wait once more for data to arrive
      nfds = 1;  // stop polling stdin
    } else if (inputClosed && !errored) {
      errored = true;
    }
    if (ret > 0 && (fds[IDX_SOCK].revents & (POLLHUP | POLLRDHUP))) {
      errored = true;
    }
#else
#ifdef HAVE_PSELECT
    // set readfds to inital checkfds
    fd_set readfds = checkfds;
    // wait for new fd event
    ret = pselect(maxfd + 1, &readfds, nullptr, nullptr, &tdiff, nullptr);
#endif
#endif

    bool newData = false;
    bool newInput = false;
    if (ret != 0) {
#ifdef HAVE_PPOLL
      // new data from notify
      newInput = fds[IDX_STDIN].revents & POLLIN;

      // new data from socket
      newData = fds[IDX_SOCK].revents & POLLIN;
#else
#ifdef HAVE_PSELECT
      // new data from notify
      newInput = FD_ISSET(STDIN_FILENO, &readfds);

      // new data from socket
      newData = FD_ISSET(socket->getFD(), &readfds);
#endif
#endif
    }

    if (newData) {
      datalen = socket->recv(data, sizeof(data));

      if (datalen < 0) {
        perror("ebusctl recv");
        errored = true;
        break;
      }

      for (int i = 0; i < datalen; i++) {
        ostream << data[i];
      }
      string str = ostream.str();
      if (listening) {
        return str;
      }
      if (str.length() >= 2 && str[str.length()-2] == '\n' && str[str.length()-1] == '\n') {
        return str;
      }
    } else if (newInput) {
      getline(cin, message);
      if (message.length() == 0) {
        continue;
      }
      sendmessage = message+'\n';
      if (socket->send(sendmessage.c_str(), sendmessage.size()) < 0) {
        perror("ebusctl send in fetch");
        errored = true;
        break;
      }

      if (strcasecmp(message.c_str(), "Q") == 0
      || strcasecmp(message.c_str(), "QUIT") == 0
      || strcasecmp(message.c_str(), "STOP") == 0) {
        exit(EXIT_SUCCESS);
        return "";
      }
      message.clear();
    }
  }

  return ostream.str();
}

bool connect(const char* host, uint16_t port, uint16_t timeout, char* const *args, int argCount) {
  TCPSocket* socket = TCPSocket::connect(host, port, timeout);
  bool ret;

  bool once = args != nullptr && argCount > 0;
  ret = socket != nullptr;
  if (ret) {
    string message, sendmessage;
    bool errored = false;
    do {
      bool listening = false;

      if (!once) {
        cout << host << ": ";
        getline(cin, message);
      } else {
        for (int i = 0; i < argCount; i++) {
          if (i > 0) {
            message += " ";
          }
          bool quote = strchr(args[i], ' ') != nullptr && strchr(args[i], '"') == nullptr;
          if (quote) {
            message += "\"";
          }
          message += args[i];
          if (quote) {
            message += "\"";
          }
        }
      }

      sendmessage = message+'\n';
      if (socket->send(sendmessage.c_str(), sendmessage.size()) < 0) {
        perror("ebusctl send");
        ret = false;
        break;
      }

      if (strcasecmp(message.c_str(), "Q") == 0
      || strcasecmp(message.c_str(), "QUIT") == 0
      || strcasecmp(message.c_str(), "STOP") == 0)
        break;

      if (message.length() > 0) {
        if (strcasecmp(message.c_str(), "L") == 0
        || strcasecmp(message.c_str(), "LISTEN") == 0) {
          listening = true;
          while (!errored && listening && !cin.eof()) {
            string result(fetchData(socket, listening, timeout, errored));
            cout << result;
            cout.flush();
            if (strcasecmp(result.c_str(), "LISTEN STOPPED") == 0) {
              break;
            }
          }
        } else {
          cout << fetchData(socket, listening, timeout, errored);
          cout.flush();
        }
      }
    } while (!errored && !once && !cin.eof());
    delete socket;
  } else {
    cout << "error connecting to " << host << ":" << port << endl;
  }
  return ret;
}


/**
 * Main function.
 * @param argc the number of command line arguments.
 * @param argv the command line arguments.
 * @return the exit code.
 */
int main(int argc, char* argv[]) {
  struct argp argp = { argpoptions, parse_opt, argpargsdoc, argpdoc, nullptr, nullptr, nullptr };
  setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);
  if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, nullptr, &opt) != 0) {
    return EINVAL;
  }
  bool success = connect(opt.server, opt.port, opt.timeout, opt.args, opt.argCount);

  exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

}  // namespace ebusd

int main(int argc, char* argv[]) {
  return ebusd::main(argc, argv);
}

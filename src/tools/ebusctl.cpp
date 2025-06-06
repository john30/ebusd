/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2025 John Baier <ebusd@ebusd.eu>, Roland Jax 2012-2014 <ebusd@liwest.at>
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

#include <string.h>
#ifdef HAVE_PPOLL
#  include <poll.h>
#endif
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include "lib/utils/arg.h"
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
  bool errorResponse;     //!< non-zero exit on error response

  unsigned int argCount;  //!< number of arguments to pass to ebusd
};

/** the program options. */
static struct options opt = {
  "localhost",  // server
  8888,         // port
  60,           // timeout
  false,        // non-zero exit on error response

  0             // argCount
};

/** the definition of the known program arguments. */
static const argDef argDefs[] = {
  {nullptr,     0, nullptr, 0, "Options:"},
  {"server",  's', "HOST",  0, "Connect to " PACKAGE " on HOST (name or IP) [localhost]"},
  {"port",    'p', "PORT",  0, "Connect to " PACKAGE " on PORT [8888]"},
  {"timeout", 't', "SECS",  0, "Timeout for connecting to/receiving from " PACKAGE
                               ", 0 for none [60]"},
  {"error",   'e', nullptr, 0, "Exit non-zero if the connection was fine but the response indicates non-success"},

  {nullptr, 0x100, "COMMAND", af_optional|af_multiple, "COMMAND (and arguments) to send to " PACKAGE "."},

  {nullptr,     0, nullptr, 0, nullptr},
};

/**
 * The program argument parsing function.
 * @param key the key from @a argDefs.
 * @param arg the option argument, or nullptr.
 * @param parseOpt the parse options.
 */
static int parse_opt(int key, char *arg, const argParseOpt *parseOpt, struct options *opt) {
  char* strEnd = nullptr;
  unsigned int value;
  switch (key) {
  // Device settings:
  case 's':  // --server=localhost
    if (arg == nullptr || arg[0] == 0) {
      argParseError(parseOpt, "invalid server");
      return EINVAL;
    }
    opt->server = arg;
    break;
  case 'p':  // --port=8888
    value = strtoul(arg, &strEnd, 10);
    if (strEnd == nullptr || strEnd == arg || *strEnd != 0 || value < 1 || value > 65535) {
      argParseError(parseOpt, "invalid port");
      return EINVAL;
    }
    opt->port = (uint16_t)value;
    break;
  case 't':  // --timeout=10
    value = strtoul(arg, &strEnd, 10);
    if (strEnd == nullptr || strEnd == arg || *strEnd != 0 || value > 3600) {
      argParseError(parseOpt, "invalid timeout");
      return EINVAL;
    }
    opt->timeout = (uint16_t)value;
    break;
  case 'e':  // --error
    opt->errorResponse = true;
    break;
  default:
    if (key >= 0x100) {
      opt->argCount++;
    } else {
      return ESRCH;
    }
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

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

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
          string response = fetchData(socket, listening, timeout, errored);
          cout << response;
          cout.flush();
          if (errored || (opt.errorResponse && response.substr(0, 4) == "ERR:")) {
            ret = false;
          }
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
  argParseOpt parseOpt = {
    argDefs,
    reinterpret_cast<parse_function_t>(parse_opt),
    af_noVersion,
    "Client for accessing " PACKAGE " via TCP.",
    "If given, send COMMAND together with arguments to " PACKAGE ".\n"
    "Use 'help' as COMMAND for help on available " PACKAGE " commands.",
    nullptr,
  };
  switch (argParse(&parseOpt, argc, argv, &opt)) {
    case 0:  // OK
      break;
    case '?':  // help printed
      return 0;
    default:
      return EINVAL;
  }

  bool success = connect(opt.server, opt.port, opt.timeout, argv + argc - opt.argCount, opt.argCount);

  exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

}  // namespace ebusd

int main(int argc, char* argv[]) {
  return ebusd::main(argc, argv);
}

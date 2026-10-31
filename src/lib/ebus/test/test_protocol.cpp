/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2026 ebusd contributors
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

#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include "lib/ebus/protocol_direct.h"

using namespace std;
using namespace ebusd;

/** Byte source for deterministic, synchronous protocol replays, without I/O. */
class ReplayDevice : public Device {
 public:
  struct Input {
    symbol_t symbol;
    result_t result;
  };

  ReplayDevice(size_t commandSize, const vector<Input>& response)
    : finished(false), m_commandSize(commandSize), m_response(response),
      m_sendCount(0), m_arbitration(SYN), m_recvCount(0) {
    m_input.push_back({SYN, RESULT_OK});
  }

  const char* getName() const override { return "protocol replay"; }
  void formatInfo(ostringstream*, bool, bool) override {}
  result_t open() override { return RESULT_OK; }
  bool isValid() override { return true; }

  result_t send(symbol_t symbol) override {
    // The adapter echoes each transmitted byte. After the command CRC, the
    // scripted response becomes available to the real receive loop.
    m_input.push_front({symbol, RESULT_OK});
    if (++m_sendCount == m_commandSize) {
      m_input.insert(m_input.end(), m_response.begin(), m_response.end());
    }
    return RESULT_OK;
  }

  result_t recv(unsigned int, symbol_t* symbol, ArbitrationState* state) override {
    *state = as_none;
    if (++m_recvCount > 1024) {
      finished = true;
      return RESULT_ERR_TIMEOUT;
    }
    if (m_arbitration != SYN) {
      *symbol = m_arbitration;
      *state = as_won;
      m_arbitration = SYN;
      return RESULT_OK;
    }
    if (m_input.empty()) {
      finished = true;
      return RESULT_ERR_TIMEOUT;
    }
    Input input = m_input.front();
    m_input.pop_front();
    *symbol = input.symbol;
    return input.result;
  }

  result_t startArbitration(symbol_t master) override {
    m_arbitration = master;
    return RESULT_OK;
  }
  bool isArbitrating() const override { return m_arbitration != SYN; }
  bool cancelRunningArbitration(ArbitrationState*) override {
    bool running = isArbitrating();
    m_arbitration = SYN;
    return running;
  }

  bool finished;

 private:
  const size_t m_commandSize;
  const vector<Input> m_response;
  size_t m_sendCount;
  symbol_t m_arbitration;
  unsigned int m_recvCount;
  deque<Input> m_input;
};

/** Run the unmodified production receive loop synchronously until input ends. */
class ReplayProtocol : public DirectProtocolHandler {
 public:
  ReplayProtocol(const ebus_protocol_config_t config, ReplayDevice* device, ProtocolListener* listener)
    : DirectProtocolHandler(config, device, listener), m_replay(device) {}
  bool isRunning() override { return !m_replay->finished; }

 private:
  ReplayDevice* m_replay;
};

struct Result {
  bool notified = false;
  result_t result = RESULT_CONTINUE;
  string response;
};

class ReplayRequest : public BusRequest {
 public:
  ReplayRequest(const MasterSymbolString& master, Result* result)
    : BusRequest(master, false), m_result(result) {}
  bool notify(result_t result, const SlaveSymbolString& response) override {
    m_result->notified = true;
    m_result->result = result;
    m_result->response = response.getStr();
    return false;
  }

 private:
  Result* m_result;
};

class ReplayListener : public ProtocolListener {
 public:
  struct Message {
    MessageDirection direction;
    string command;
    string response;
  };
  void notifyProtocolStatus(ProtocolState, result_t) override {}
  void notifyProtocolSeenAddress(symbol_t) override {}
  void notifyProtocolMessage(MessageDirection direction, const MasterSymbolString& command,
      const SlaveSymbolString& response) override {
    messages.push_back({direction, command.getStr(), response.getStr()});
  }
  vector<Message> messages;
};

static void append(vector<ReplayDevice::Input>* input, const string& hex) {
  for (size_t pos = 0; pos < hex.size(); pos += 2) {
    input->push_back({static_cast<symbol_t>(stoul(hex.substr(pos, 2), nullptr, 16)), RESULT_CONTINUE});
  }
  input->back().result = RESULT_OK;
}

static Result replay(const string& request, const vector<ReplayDevice::Input>& input, ReplayListener* listener) {
  MasterSymbolString master;
  master.parseHex(request);
  ebus_protocol_config_t config = {};
  config.ownAddress = 0x31;
  config.slaveRecvTimeout = 100;
  config.lockCount = 3;
  Result result;
  ReplayDevice* device = new ReplayDevice(master.size(), input);
  ReplayProtocol handler(config, device, listener);
  handler.addRequest(new ReplayRequest(master, &result), false);
  handler.run();
  return result;
}

int main() {
  // Normal own response: ACK, length, 23.8 C, valid response CRC.
  vector<ReplayDevice::Input> input;
  append(&input, "0002ee00fe");
  ReplayListener normal;
  Result result = replay("3185502203ccb427", input, &normal);
  if (!result.notified || result.result != RESULT_OK || result.response != "02ee00"
      || normal.messages.size() != 1 || normal.messages[0].direction != md_send) {
    cerr << "normal active response failed" << endl;
    return 1;
  }

  ReplayListener interrupted;
  result = replay("3185502203ccb427", {{SYN, RESULT_OK}}, &interrupted);
  if (!result.notified || result.result >= RESULT_OK || !interrupted.messages.empty()) {
    cerr << "unbuffered SYN did not abort the active request" << endl;
    return 1;
  }

  // Recorded foreign commands, their CRC, ACK, response, and response CRC.
  // Room temperature may become 19.1/0.1 C, or counter words 43/173 percent,
  // if a foreign response is incorrectly attached to the pending own request.
  struct Case {
    string request;
    string foreign;
    string command;
    string response;
  };
  const Case cases[] = {
    {"3185502203ccb427", "3051502203cc2b0a9d0002bf009f", "3051502203cc2b0a", "02bf00"},
    {"313550220300b427", "3008502203cc29021c00020100b7", "3008502203cc2902", "020100"},
    {"3108502203cc6f01", "3008502203cc28028700022b1d7a", "3008502203cc2802", "022b1d"},
    {"3108502203cc6f01", "3008502203cc2a022a0002ad4242", "3008502203cc2a02", "02ad42"},
  };
  for (const auto& test : cases) {
    // Interrupt both before the command ACK and during reception of a response.
    for (bool afterAck : {false, true}) {
      input.clear();
      if (afterAck) {
        append(&input, "0002");
        input.back().result = RESULT_CONTINUE;
      }
      input.push_back({SYN, RESULT_CONTINUE});
      append(&input, test.foreign);
      input.push_back({ACK, RESULT_OK});  // foreign master's final ACK
      ReplayListener foreign;
      result = replay(test.request, input, &foreign);
      if (!result.notified || result.result >= RESULT_OK || foreign.messages.size() != 1
          || foreign.messages[0].direction != md_recv || foreign.messages[0].command != test.command
          || foreign.messages[0].response != test.response) {
        cerr << "foreign response accepted/misattributed for " << test.request
             << (afterAck ? " after ACK" : " before ACK") << endl;
        cerr << "request result=" << getResultCode(result.result) << " response=" << result.response << endl;
        for (const auto& message : foreign.messages) {
          cerr << "direction=" << message.direction << " command=" << message.command
               << " response=" << message.response << endl;
        }
        return 1;
      }
    }
  }
  cout << "protocol replay: OK" << endl;
  return 0;
}

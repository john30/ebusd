/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2024 John Baier <ebusd@ebusd.eu>
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

#include "ebusd/bushandler.h"
#include <iomanip>
#include "lib/utils/log.h"

namespace ebusd {

using std::dec;
using std::hex;
using std::setfill;
using std::setw;
using std::endl;


result_t PollRequest::prepare(symbol_t ownMasterAddress) {
  istringstream input;
  result_t result = m_message->prepareMaster(m_index, ownMasterAddress, SYN, UI_FIELD_SEPARATOR, &input, &m_master);
  if (result == RESULT_OK) {
    string str = m_master.getStr();
    logInfo(lf_bus, "poll cmd: %s", str.c_str());
  }
  return result;
}

bool PollRequest::notify(result_t result, const SlaveSymbolString& slave) {
  if (result == RESULT_OK) {
    result = m_message->storeLastData(m_index, slave);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
  }
  if (result < RESULT_OK) {
    logError(lf_bus, "poll %s %s failed: %s", m_message->getCircuit().c_str(), m_message->getName().c_str(),
        getResultCode(result));
  }
  return false;
}


result_t ScanRequest::prepare(symbol_t ownMasterAddress) {
  if (m_slaves.empty()) {
    return RESULT_ERR_EOF;
  }
  symbol_t dstAddress = m_slaves.front();
  istringstream input;
  m_result = m_message->prepareMaster(m_index, ownMasterAddress, dstAddress, UI_FIELD_SEPARATOR, &input, &m_master);
  if (m_result >= RESULT_OK) {
    string str = m_master.getStr();
    logInfo(lf_bus, "scan %2.2x cmd: %s", dstAddress, str.c_str());
  }
  return m_result;
}

bool ScanRequest::notify(result_t result, const SlaveSymbolString& slave) {
  symbol_t dstAddress = m_master[1];
  m_busHandler->setScanResult(dstAddress, 0, "");
  if (result == RESULT_OK) {
    if (m_message == m_messageMap->getScanMessage()) {
      Message* message = m_messageMap->getScanMessage(dstAddress);
      if (message != nullptr) {
        m_message = message;
        m_message->storeLastData(m_index, m_master);  // expected to work since this is a clone
      }
    } else if (m_message->getDstAddress() == SYN) {
      m_message = m_message->derive(dstAddress, true);
      m_messageMap->add(true, m_message);
      m_message->storeLastData(m_index, m_master);  // expected to work since this is a clone
    }
    result = m_message->storeLastData(m_index, slave);
    if (result >= RESULT_OK && m_index+1 < m_message->getCount()) {
      m_index++;
      result = prepare(m_master[0]);
      if (result >= RESULT_OK) {
        return true;
      }
    }
    if (result == RESULT_OK) {
      ostringstream output;
      result = m_message->decodeLastData(true, nullptr, -1, OF_NONE, &output);  // decode data
      string str = output.str();
      m_busHandler->setScanResult(dstAddress, m_notifyIndex+m_index, str);
    }
  }
  if (result < RESULT_OK) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    if (m_deleteOnFinish) {
      if (result == RESULT_ERR_TIMEOUT) {
        logNotice(lf_bus, "scan %2.2x timed out (%d slaves left)", dstAddress, m_slaves.size());
      } else {
        logError(lf_bus, "scan %2.2x failed (%d slaves left): %s", dstAddress, m_slaves.size(), getResultCode(result));
      }
    }
    m_messages.clear();  // skip remaining secondary messages
  } else if (m_messages.empty()) {
    if (!m_slaves.empty()) {
      m_slaves.pop_front();
    }
    if (m_deleteOnFinish) {
      logNotice(lf_bus, "scan %2.2x completed (%d slaves left)", dstAddress, m_slaves.size());
    }
  }
  m_result = result;
  if (m_slaves.empty() || result == RESULT_ERR_NO_SIGNAL) {
    if (m_deleteOnFinish) {
      logNotice(lf_bus, "scan finished");
    }
    m_busHandler->setScanFinished();
    return false;
  }
  if (m_messages.empty()) {
    m_messages = m_allMessages;
  }
  m_index = 0;
  m_message = m_messages.front();
  m_messages.pop_front();
  result = prepare(m_master[0]);
  if (result < RESULT_OK) {
    m_busHandler->setScanFinished();
    if (result != RESULT_ERR_EOF) {
      m_result = result;
    }
    return false;  // give up
  }
  return true;
}


void GrabbedMessage::setLastData(const MasterSymbolString& master, const SlaveSymbolString& slave) {
  time(&m_lastTime);
  m_lastMaster = master;
  m_lastSlave = slave;
  m_count++;
}


/**
 * Decode the input @a SymbolString with the specified @a DataType and length.
 * @param type the @a DataType.
 * @param input the @a SymbolString to read the binary value from.
 * @param length the number of symbols to read.
 * @param offsets the last offset to the baseOffset to read.
 * @param firstOnly whether to read only the first non-erroneous offset.
 * @param output the ostringstream to append the formatted value to.
 * @return @a RESULT_OK on success, or an error code.
 */
bool decodeType(const DataType* type, const SymbolString& input, size_t length,
    size_t offsets, bool firstOnly, ostringstream* output) {
  bool first = true;
  string in = input.getStr(input.getDataOffset());
  for (size_t offset = 0; offset <= offsets; offset++) {
    ostringstream out;
    result_t result = type->readSymbols(offset, length, input, OF_NONE, &out);
    if (result != RESULT_OK) {
      continue;
    }
    if (type->isNumeric() && type->hasFlag(DAY)) {
      unsigned int value = 0;
      if (type->readRawValue(offset, length, input, &value) == RESULT_OK) {
        out.str("");
        out << DataField::getDayName(reinterpret_cast<const NumberDataType*>(type)->getMinValue()+value);
      }
    }
    if (first) {
      first = false;
      *output << endl << " ";
      ostringstream::pos_type cnt = output->tellp();
      type->dump(OF_NONE, length, false, output);
      cnt = output->tellp() - cnt;
      while (cnt < 5) {
        *output << " ";
        cnt += 1;
      }
    } else {
      *output << ",";
    }
    *output << " " << in.substr(offset*2, length*2);
    if (type->isNumeric()) {
      *output << "=" << out.str();
    } else {
      *output << "=\"" << out.str() << "\"";
    }
    if (firstOnly) {
      return true;  // only the first offset with maximum length when adjustable maximum size is at least 8 bytes
    }
  }
  return !first;
}

bool GrabbedMessage::dump(bool unknown, MessageMap* messages, bool first, OutputFormat outputFormat,
    ostringstream* output, bool isDirectMode) const {
  Message* message = messages->find(m_lastMaster);
  if (unknown && message) {
    return false;
  }
  if (!first) {
    if (outputFormat & OF_JSON) {
      *output << ",";
    } else {
      *output << endl;
    }
  }
  symbol_t dstAddress = m_lastMaster[1];
  if (outputFormat & OF_JSON) {
    if (outputFormat & OF_SHORT) {
      *output << '"' << m_lastMaster.getStr() << '/';
      if (dstAddress != BROADCAST && !isMaster(dstAddress)) {
        *output << m_lastSlave.getStr();
      }
      *output << '/' << static_cast<unsigned>(m_count);
      if (message) {
        *output << '/' << message->getName();
      }
      *output << '"';
      return true;
    }
    *output << "\n{";
    if (m_lastMaster.dumpJson(false, output)) {
      *output << ", ";
      if (dstAddress != BROADCAST && !isMaster(dstAddress) && m_lastSlave.dumpJson(false, output)) {
        *output << ", ";
      }
    }
    *output << "\"count\": " << static_cast<unsigned>(m_count);
    *output << ", \"lastup\": " << setw(0) << dec << m_lastTime;
    if (message) {
      *output << ", \"circuit\": \"" << message->getCircuit() << "\""
              << ", \"name\": \"" << message->getName() << "\"";
    }
    *output << "}";
  } else {
    *output << m_lastMaster.getStr();
    if (dstAddress != BROADCAST && !isMaster(dstAddress)) {
      *output << (isDirectMode ? " " : " / ") << m_lastSlave.getStr();
    }
    if (!isDirectMode) {
      *output << " = " << m_count;
      if (message) {
        *output << ": " << message->getCircuit() << " " << message->getName();
      }
    }
  }
  if (!(outputFormat & OF_DEFINITION) || (outputFormat & OF_JSON)) {
    return true;
  }
  DataTypeList *types = DataTypeList::getInstance();
  if (!types) {
    return true;
  }
  bool master = isMaster(dstAddress) || dstAddress == BROADCAST || m_lastSlave.getDataSize() <= 0;
  size_t remain = master ? m_lastMaster.getDataSize() : m_lastSlave.getDataSize();
  if (remain == 0) {
    return true;
  }
  for (const auto& it : *types) {
    const DataType* baseType = it.second;
    if ((baseType->getBitCount() % 8) != 0 || baseType->isIgnored() || baseType->hasFlag(DUP)) {
      // skip bit and ignored types
      continue;
    }
    size_t maxLength = baseType->getBitCount()/8;
    bool firstOnly = maxLength >= 8;
    if (maxLength > remain) {
      maxLength = remain;
    }
    if (baseType->isAdjustableLength()) {
      for (size_t length = maxLength; length >= 1; length--) {
        const DataType* type = types->get(baseType->getId(), length);
        bool decoded;
        if (master) {
          decoded = decodeType(type, m_lastMaster, length, remain-length, firstOnly, output);
        } else {
          decoded = decodeType(type, m_lastSlave, length, remain-length, firstOnly, output);
        }
        if (decoded && firstOnly) {
          break;  // only a single offset with maximum length when adjustable maximum size is at least 8 bytes
        }
      }
    } else if (maxLength > 0) {
      if (master) {
        decodeType(baseType, m_lastMaster, maxLength, remain-maxLength, false, output);
      } else {
        decodeType(baseType, m_lastSlave, maxLength, remain-maxLength, false, output);
      }
    }
  }
  return true;
}


void BusHandler::clear() {
  m_protocol->clear();
  m_scanResults.clear();
}

result_t BusHandler::readFromBus(Message* message, const string& inputStr, symbol_t dstAddress,
    symbol_t srcAddress) {
  symbol_t masterAddress = srcAddress == SYN ? m_protocol->getOwnMasterAddress() : srcAddress;
  result_t ret = RESULT_EMPTY;
  MasterSymbolString master;
  SlaveSymbolString slave;
  for (size_t index = 0; index < message->getCount(); index++) {
    istringstream input(inputStr);
    ret = message->prepareMaster(index, masterAddress, dstAddress, UI_FIELD_SEPARATOR, &input, &master);
    if (ret != RESULT_OK) {
      logError(lf_bus, "prepare message part %d: %s", index, getResultCode(ret));
      break;
    }
    // send message
    ret = m_protocol->sendAndWait(master, &slave);
    if (ret != RESULT_OK) {
      logError(lf_bus, "send message part %d: %s", index, getResultCode(ret));
      break;
    }
    ret = message->storeLastData(index, slave);
    if (ret < RESULT_OK) {
      logError(lf_bus, "store message part %d: %s", index, getResultCode(ret));
      break;
    }
  }
  return ret;
}

void BusHandler::notifyProtocolStatus(ProtocolState state, result_t result) {
  if (state == ps_empty && m_pollInterval > 0) {  // check for poll/scan
    time_t now;
    time(&now);
    if (m_lastPoll == 0 || difftime(now, m_lastPoll) > m_pollInterval) {
      Message* message = m_messages->getNextPoll();
      if (message != nullptr) {
        m_lastPoll = now;
        if (difftime(now, message->getLastUpdateTime()) > m_pollInterval) {
          // only poll this message if it was not updated already by other means within the interval
          auto request = new PollRequest(message);
          result_t ret = request->prepare(m_protocol->getOwnMasterAddress());
          if (ret != RESULT_OK) {
            logError(lf_bus, "prepare poll message: %s", getResultCode(ret));
            delete request;
          } else {
            ret = m_protocol->addRequest(request, false);
            if (ret != RESULT_OK) {
              logError(lf_bus, "push poll message: %s", getResultCode(ret));
              delete request;
            }
          }
        }
      }
    }
  }
}

void BusHandler::notifyProtocolSeenAddress(symbol_t address) {
  m_seenAddresses[address] |= SEEN;
}

void BusHandler::notifyProtocolMessage(MessageDirection direction, const MasterSymbolString& command,
  const SlaveSymbolString& response) {
  symbol_t srcAddress = command[0], dstAddress = command[1];
  bool master = isMaster(dstAddress);
  if (dstAddress == BROADCAST) {
    if (command.getDataSize() >= 10 && command[2] == 0x07 && command[3] == 0x04) {
      symbol_t slaveAddress = getSlaveAddress(srcAddress);
      notifyProtocolSeenAddress(slaveAddress);
      Message* message = m_messages->getScanMessage(slaveAddress);
      if (message && (message->getLastUpdateTime() == 0 || message->getLastSlaveData().getDataSize() < 10)) {
        // e.g. 10fe07040a b5564149303001248901
        MasterSymbolString dummyMaster;
        istringstream input;
        result_t result = message->prepareMaster(0, m_protocol->getOwnMasterAddress(), SYN, UI_FIELD_SEPARATOR, &input,
            &dummyMaster);
        if (result == RESULT_OK) {
          SlaveSymbolString idData;
          idData.push_back(10);
          for (size_t i = 0; i < 10; i++) {
            idData.push_back(command.dataAt(i));
          }
          result = message->storeLastData(0, idData);
          if (result == RESULT_OK) {
            ostringstream output;
            result = message->decodeLastData(true, nullptr, -1, OF_NONE, &output);
            if (result == RESULT_OK) {
              string str = output.str();
              setScanResult(slaveAddress, 0, str);
            }
          }
        }
        logNotice(lf_update, "store broadcast ident: %s", getResultCode(result));
      }
    }
  } else if (!master) {
    if (command.size() >= 5 && command[2] == 0x07 && command[3] == 0x04) {
      Message* message = m_messages->getScanMessage(dstAddress);
      if (message && (message->getLastUpdateTime() == 0 || message->getLastSlaveData().getDataSize() < 10)) {
        result_t result = message->storeLastData(command, response);
        if (result == RESULT_OK) {
          ostringstream output;
          result = message->decodeLastData(true, nullptr, -1, OF_NONE, &output);
          if (result == RESULT_OK) {
            string str = output.str();
            setScanResult(dstAddress, 0, str);
          }
        }
        logNotice(lf_update, "store %2.2x ident: %s", dstAddress, getResultCode(result));
      }
    }
  }
  Message* message = m_messages->find(command);
  if (m_grabMessages) {
    uint64_t key;
    if (message) {
      key = message->getKey();
    } else {
      key = Message::createKey(command, command[1] == BROADCAST ? 1 : 4);  // up to 4 DD bytes (1 for broadcast)
    }
    m_grabbedMessages[key].setLastData(command, response);
  }
  if (direction == md_answer) {
    size_t idLen = command.getDataSize();
    if (master && idLen >= response.size()) {
      // build MS auto-answer from MM with same ID
      SlaveSymbolString answer;
      answer.push_back(0);  // room for length
      idLen -= response.size();
      for (size_t pos = idLen; pos < response.size(); pos++) {
        answer.push_back(command.dataAt(pos));
      }
      m_protocol->setAnswer(SYN, command[1], command[2], command[3], command.data() + 5, idLen, answer);
      // TODO could use loaded messages for identifying MM/MS message pair
    }
  }
  const char* prefix = direction == md_answer ? "answered" : direction == md_send ? "sent" : "received";
  if (message == nullptr) {
    if (dstAddress == BROADCAST || master) {
      logNotice(lf_update, "%s unknown %s cmd: %s", prefix, master ? "MM" : "BC", command.getStr().c_str());
    } else {
      logNotice(lf_update, "%s unknown MS cmd: %s / %s", prefix, command.getStr().c_str(),
        response.getStr().c_str());
    }
  } else {
    m_messages->invalidateCache(message);
    string circuit = message->getCircuit();
    string name = message->getName();
    const char* mode = message->isScanMessage() ? message->isWrite() ? "scan-write" : "scan-read"
      : message->isPassive() ? message->isWrite() ? "update-write" : "update-read"
      : message->getPollPriority() > 0 ? message->isWrite() ? "poll-write" : "poll-read"
      : message->isWrite() ? "write" : "read";
    result_t result = message->storeLastData(command, response);
    ostringstream output;
    if (result == RESULT_OK) {
      result = message->decodeLastData(false, nullptr, -1, OF_NONE, &output);
    }
    if (result < RESULT_OK) {
      logError(lf_update, "unable to parse %s %s %s from %s / %s: %s", mode, circuit.c_str(), name.c_str(),
          command.getStr().c_str(), response.getStr().c_str(), getResultCode(result));
    } else {
      string data = output.str();
      if (m_protocol->isOwnAddress(dstAddress)) {
        logNotice(lf_update, "%s %s self-update %s %s QQ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(),
            srcAddress, data.c_str());
      } else if (message->getDstAddress() == SYN) {  // any destination
        if (message->getSrcAddress() == SYN) {  // any destination and any source
          logNotice(lf_update, "%s %s %s %s QQ=%2.2x ZZ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(),
              srcAddress, dstAddress, data.c_str());
        } else {
          logNotice(lf_update, "%s %s %s %s ZZ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(), dstAddress,
              data.c_str());
        }
      } else if (message->getSrcAddress() == SYN) {  // any source
        logNotice(lf_update, "%s %s %s %s QQ=%2.2x: %s", prefix, mode, circuit.c_str(), name.c_str(), srcAddress,
            data.c_str());
      } else {
        logNotice(lf_update, "%s %s %s %s: %s", prefix, mode, circuit.c_str(), name.c_str(), data.c_str());
      }
    }
  }
}

result_t BusHandler::prepareScan(symbol_t slave, bool full, const string& levels, bool* reload,
    ScanRequest** request) {
  Message* scanMessage = m_messages->getScanMessage();
  if (scanMessage == nullptr) {
    return RESULT_ERR_NOTFOUND;
  }
  if (m_protocol->isReadOnly()) {
    return RESULT_OK;
  }
  deque<Message*> messages;
  m_messages->findAll("scan", "", levels, true, true, false, false, true, true, 0, 0, false, &messages);
  auto it = messages.begin();
  while (it != messages.end()) {
    Message* message = *it;
    if (message->getPrimaryCommand() == 0x07 && message->getSecondaryCommand() == 0x04) {
      it = messages.erase(it);  // query pb 0x07 / sb 0x04 only once
    } else {
      it++;
    }
  }

  deque<symbol_t> slaves;
  if (slave != SYN) {
    slaves.push_back(slave);
    if (!*reload) {
      Message* message = m_messages->getScanMessage(slave);
      if (message == nullptr || message->getLastChangeTime() == 0) {
        *reload = true;
      }
    }
  } else {
    *reload = true;
    for (slave = 1; slave != 0; slave++) {  // 0 is known to be a master
      if (!isValidAddress(slave, false) || isMaster(slave)) {
        continue;
      }
      if (!full && (m_seenAddresses[slave]&SEEN) == 0) {
        symbol_t master = getMasterAddress(slave);  // check if we saw the corresponding master already
        if (master == SYN || (m_seenAddresses[master]&SEEN) == 0) {
          continue;
        }
      }
      slaves.push_back(slave);
    }
  }
  if (*reload) {
    messages.push_front(scanMessage);
  }
  if (messages.empty()) {
    return RESULT_OK;
  }
  *request = new ScanRequest(slave == SYN, m_messages, messages, slaves, this, *reload ? 0 : 1);
  result_t result = (*request)->prepare(m_protocol->getOwnMasterAddress());
  if (result < RESULT_OK) {
    delete *request;
    *request = nullptr;
    return result == RESULT_ERR_EOF ? RESULT_EMPTY : result;
  }
  return RESULT_OK;
}

result_t BusHandler::startScan(bool full, const string& levels) {
  if (m_runningScans > 0) {
    return RESULT_ERR_DUPLICATE;
  }
  ScanRequest* request = nullptr;
  bool reload = true;
  result_t result = prepareScan(SYN, full, levels, &reload, &request);
  if (result != RESULT_OK) {
    return result;
  }
  if (!request) {
    return RESULT_ERR_NOTFOUND;
  }
  m_scanResults.clear();
  m_runningScans++;
  // request is deleted by ProtocolHandler after finish
  return m_protocol->addRequest(request, false);
}

void BusHandler::setScanResult(symbol_t dstAddress, size_t index, const string& str) {
  m_seenAddresses[dstAddress] |= SCAN_INIT;
  if (str.length() > 0) {
    m_seenAddresses[dstAddress] |= SCAN_DONE;
    vector<string>& result = m_scanResults[dstAddress];
    if (index >= result.size()) {
      result.resize(index+1);
    }
    result[index] = str;
    logNotice(lf_bus, "scan %2.2x: %s", dstAddress, str.c_str());
  }
}

void BusHandler::setScanFinished() {
  if (m_runningScans > 0) {
    m_runningScans--;
  }
}

bool BusHandler::formatScanResult(symbol_t slave, bool leadingNewline, ostringstream* output) const {
  const auto it = m_scanResults.find(slave);
  if (it == m_scanResults.end()) {
    return false;
  }
  if (leadingNewline) {
    *output << endl;
  }
  *output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave);
  for (const auto &result : it->second) {
    *output << result;
  }
  return true;
}

void BusHandler::formatScanResult(ostringstream* output) const {
  if (m_runningScans > 0) {
    *output << m_runningScans << " scan(s) still running" << endl;
  }
  bool first = true;
  for (symbol_t slave = 1; slave != 0; slave++) {  // 0 is known to be a master
    if (formatScanResult(slave, !first, output)) {
      first = false;
    }
  }
  if (first) {
    // fallback to autoscan results
    for (symbol_t slave = 1; slave != 0; slave++) {  // 0 is known to be a master
      if (isValidAddress(slave, false) && !isMaster(slave) && (m_seenAddresses[slave]&SCAN_DONE) != 0) {
        Message* message = m_messages->getScanMessage(slave);
        if (message != nullptr && message->getLastUpdateTime() > 0) {
          if (first) {
            first = false;
          } else {
            *output << endl;
          }
          *output << hex << setw(2) << setfill('0') << static_cast<unsigned>(slave);
          message->decodeLastData(true, nullptr, -1, OF_NONE, output);
        }
      }
    }
  }
}

void BusHandler::formatSeenInfo(ostringstream* output) const {
  symbol_t address = 0;
  for (int index = 0; index < 256; index++, address++) {
    bool ownAddress = m_protocol->isOwnAddress(address);
    if (!isValidAddress(address, false) || ((m_seenAddresses[address]&SEEN) == 0 && !ownAddress)) {
      continue;
    }
    *output << endl << "address " << setfill('0') << setw(2) << hex << static_cast<unsigned>(address);
    symbol_t master;
    if (isMaster(address)) {
      *output << ": master";
      master = address;
    } else {
      *output << ": slave";
      master = getMasterAddress(address);
    }
    if (master != SYN) {
      *output << " #" << setw(0) << dec << getMasterNumber(master);
    }
    if (ownAddress) {
      *output << ", ebusd";
    }
    if (m_protocol->hasAnswer(address)) {
      *output << " (answering)";
    }
    if (ownAddress && m_protocol->isAddressConflict(address)) {
      *output << ", conflict";
    }
    if ((m_seenAddresses[address]&SCAN_DONE) != 0) {
      *output << ", scanned";
      Message* message = m_messages->getScanMessage(address);
      if (message != nullptr && message->getLastUpdateTime() > 0) {
        // add detailed scan info: Manufacturer ID SW HW
        *output << " \"";
        result_t result = message->decodeLastData(false, nullptr, -1, OF_NAMES, output);
        if (result != RESULT_OK) {
          *output << "\" error: " << getResultCode(result);
        } else {
          *output << "\"";
        }
      }
    } else if ((m_seenAddresses[address]&SCAN_INIT) != 0) {
      *output << ", scanning";
    }
    const vector<string>& loadedFiles = m_messages->getLoadedFiles(address);
    if (!loadedFiles.empty()) {
      bool first = true;
      for (const auto& loadedFile : loadedFiles) {
        if (first) {
          first = false;
          *output << ", loaded \"";
        } else {
          *output << ", \"";
        }
        *output << loadedFile << "\"";
        string comment;
        if (m_messages->getLoadedFileInfo(loadedFile, &comment)) {
          if (!comment.empty()) {
            *output << " (" << comment << ")";
          }
        }
      }
    }
  }
}

void BusHandler::formatUpdateInfo(ostringstream* output) const {
  if (m_protocol->hasSignal()) {
    *output << ",\"s\":" << m_protocol->getMaxSymPerSec();
  }
  *output << ",\"c\":" << m_protocol->getMasterCount()
          << ",\"m\":" << m_messages->size()
          << ",\"ro\":" << (m_protocol->isReadOnly() ? 1 : 0)
          << ",\"an\":" << (m_protocol->isAnswering() ? 1 : 0)
          << ",\"co\":" << (m_protocol->isAddressConflict(SYN) ? 1 : 0);
  if (m_grabMessages) {
    size_t unknownCnt = 0;
    *output << ",\"gm\":[";
    bool first = true;
    for (auto it : m_grabbedMessages) {
      if (it.second.dump(false, m_messages, first, OF_JSON|OF_SHORT, output)) {
        first = false;
      }
      Message* message = m_messages->find(it.second.getLastMasterData());
      if (!message) {
        unknownCnt++;
      }
    }
    *output << "],\"gu\":" << unknownCnt;
  }
  if (!m_messages->getPreferLanguage().empty()) {
    *output << ",\"lc\":\"" << m_messages->getPreferLanguage() << "\"";
  }
  unsigned char address = 0;
  for (int index = 0; index < 256; index++, address++) {
    bool ownAddress = !m_protocol->isOwnAddress(address);
    if (!isValidAddress(address, false) || ((m_seenAddresses[address]&SEEN) == 0 && !ownAddress)) {
      continue;
    }
    *output << ",\"" << setfill('0') << setw(2) << hex << static_cast<unsigned>(address) << dec << setw(0)
            << "\":{\"o\":" << (ownAddress ? 1 : 0);
    const auto it = m_scanResults.find(address);
    if (it != m_scanResults.end()) {
      *output << ",\"s\":\"";
      for (const auto& result : it->second) {
        *output << result;
      }
      *output << "\"";
    }
    if ((m_seenAddresses[address]&SCAN_DONE) != 0) {
      Message* message = m_messages->getScanMessage(address);
      if (message != nullptr && message->getLastUpdateTime() > 0) {
        // add detailed scan info: Manufacturer ID SW HW
        message->decodeLastData(true, nullptr, -1, OF_NAMES|OF_NUMERIC|OF_JSON|OF_SHORT, output);
      }
    }
    const vector<string>& loadedFiles = m_messages->getLoadedFiles(address);
    if (!loadedFiles.empty()) {
      *output << ",\"f\":[";
      bool first = true;
      for (const auto& loadedFile : loadedFiles) {
        if (first) {
          first = false;
        } else {
          *output << ",";
        }
        *output << "{\"f\":\"" << loadedFile << "\"";
        string comment;
        if (m_messages->getLoadedFileInfo(loadedFile, &comment)) {
          if (!comment.empty()) {
            *output << ",\"c\":\"" << comment << "\"";
          }
        }
        *output << "}";
      }
      *output << "]";
    }
    *output << "}";
  }
  vector<string> loadedFiles = m_messages->getLoadedFiles();
  if (!loadedFiles.empty()) {
    *output << ",\"l\":{";
    bool first = true;
    for (const auto& loadedFile : loadedFiles) {
      if (first) {
        first = false;
      } else {
        *output << ",";
      }
      *output << "\"" << loadedFile << "\":{";
      string comment;
      size_t hash, size;
      time_t time;
      if (m_messages->getLoadedFileInfo(loadedFile, &comment, &hash, &size, &time)) {
        *output << "\"h\":\"";
        MappedFileReader::formatHash(hash, output);
        *output << "\",\"s\":" << size << ",\"t\":" << time;
      }
      *output << "}";
    }
    *output << "}";
  }
}

result_t BusHandler::scanAndWait(symbol_t dstAddress, bool loadScanConfig, bool reload) {
  if (!isValidAddress(dstAddress, false) || isMaster(dstAddress)) {
    return RESULT_ERR_INVALID_ADDR;
  }
  ScanRequest* request = nullptr;
  bool hasAdditionalScanMessages = m_messages->hasAdditionalScanMessages();
  result_t result = prepareScan(dstAddress, false, "", &reload, &request);
  if (result != RESULT_OK) {
    return result;
  }
  bool requestExecuted = false;
  if (request) {
    if (reload) {
      m_scanResults.erase(dstAddress);
    } else if (m_scanResults.find(dstAddress) != m_scanResults.end()) {
      m_scanResults[dstAddress].resize(1);
    }
    m_runningScans++;
    result = m_protocol->addRequest(request, true);
    requestExecuted = result == RESULT_OK;
    if (requestExecuted) {
      result = request->m_result;
    }
    delete request;
    request = nullptr;
  }
  if (loadScanConfig) {
    string file;
    bool timedOut = result == RESULT_ERR_TIMEOUT;
    bool loadFailed = false;
    if (timedOut || result == RESULT_OK) {
      result = m_scanHelper->loadScanConfigFile(dstAddress, &file);  // try to load even if one message timed out
      loadFailed = result != RESULT_OK;
      if (timedOut && loadFailed) {
        result = RESULT_ERR_TIMEOUT;  // back to previous result
      }
    }
    if (result == RESULT_OK) {
      m_scanHelper->executeInstructions(this);
      setScanConfigLoaded(dstAddress, file);
      if (!hasAdditionalScanMessages && m_messages->hasAdditionalScanMessages()) {
        // additional scan messages now available
        scanAndWait(dstAddress, false, false);
      }
    } else if (loadFailed || (requestExecuted && timedOut) || result == RESULT_ERR_NOTAUTHORIZED) {
      setScanConfigLoaded(dstAddress, "");
    }
  }
  return result;
}

bool BusHandler::enableGrab(bool enable) {
  if (enable == m_grabMessages) {
    return false;
  }
  if (!enable) {
    m_grabbedMessages.clear();
  }
  m_grabMessages = enable;
  return true;
}

void BusHandler::formatGrabResult(bool unknown, OutputFormat outputFormat, ostringstream* output, bool isDirectMode,
    time_t since, time_t until) const {
  if (!m_grabMessages) {
    if (!isDirectMode && !(outputFormat & OF_JSON)) {
      *output << "grab disabled";
    }
    return;
  }
  bool first = true;
  for (const auto& it : m_grabbedMessages) {
    if ((since > 0 && it.second.getLastTime() < since)
    || (until > 0 && it.second.getLastTime() >= until)) {
      continue;
    }
    if (it.second.dump(unknown, m_messages, first, outputFormat, output, isDirectMode)) {
      first = false;
    }
  }
  if (isDirectMode && !first) {
    *output << endl;
  }
}

symbol_t BusHandler::getNextScanAddress(symbol_t lastAddress, bool withUnfinished) const {
  if (lastAddress == SYN) {
    return SYN;
  }
  while (++lastAddress != 0) {  // 0 is known to be a master
    if (!isValidAddress(lastAddress, false) || isMaster(lastAddress)) {
      continue;
    }
    if ((m_seenAddresses[lastAddress]&(SEEN|LOAD_INIT)) == SEEN
    || (withUnfinished && (m_seenAddresses[lastAddress]&(SEEN|SCAN_DONE|LOAD_INIT)) == (SEEN|LOAD_INIT))) {
      return lastAddress;
    }
    symbol_t master = getMasterAddress(lastAddress);
    if (master == SYN || (m_seenAddresses[master]&SEEN) == 0) {
      continue;
    }
    if ((m_seenAddresses[lastAddress]&LOAD_INIT) == 0
    || (withUnfinished && (m_seenAddresses[lastAddress]&(SCAN_DONE|LOAD_INIT)) == LOAD_INIT)) {
      return lastAddress;
    }
  }
  return SYN;
}

void BusHandler::setScanConfigLoaded(symbol_t address, const string& file) {
  m_seenAddresses[address] |= LOAD_INIT;
  if (!file.empty()) {
    m_seenAddresses[address] |= LOAD_DONE;
    m_messages->addLoadedFile(address, file, "");
  }
}

}  // namespace ebusd

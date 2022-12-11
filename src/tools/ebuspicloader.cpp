/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2020-2022 John Baier <ebusd@ebusd.eu>
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

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <argp.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <cstring>
#include "intelhex/intelhexclass.h"
#include "lib/utils/tcpsocket.h"

using ebusd::socketConnect;

/** the version string of the program. */
const char *argp_program_version = "eBUS adapter PIC firmware loader";

/** the documentation of the program. */
static const char argpdoc[] =
    "A tool for loading firmware to the eBUS adapter PIC and configure some adjustable settings."
    "\vPORT is either the serial port to use (e.g./dev/ttyUSB0) that also supports a trailing wildcard '*' for testing"
    " multiple ports, or a network port as \"ip:port\" for use with e.g. socat or ebusd-esp.";

static const char argpargsdoc[] = "PORT";

/** the definition of the known program arguments. */
static const struct argp_option argpoptions[] = {
    {"verbose", 'v', nullptr, 0, "enable verbose output", 0 },
    {"dhcp",    'd', nullptr, 0, "set dynamic IP address via DHCP (default)", 0 },
    {"ip",      'i', "IP",    0, "set fix IP address (e.g. 192.168.0.10)", 0 },
    {"mask",    'm', "MASK",  0, "set fix IP mask (e.g. 24)", 0 },
    {"gateway", 'g', "GW",    0, "set fix IP gateway to GW (if necessary and other than net address + 1)", 0 },
    {"macip",   'M', nullptr, 0, "set the MAC address suffix from the IP address", 0 },
    {"macid",   'I', nullptr, 0, "set the MAC address suffix from internal ID (default)", 0 },
    {"arbdel",  'a', "US",    0, "set arbitration delay to US microseconds (0-620 in steps of 10, default 200"
                                 ", since firmware 20211128)", 0 },
    {"pingon",  'p', nullptr, 0, "enable visual ping (default)", 0 },
    {"pingoff", 'o', nullptr, 0, "disable visual ping", 0 },
    {"softvar", -3, "VARIANT", 0, "set the soft jumpers VARIANT to U=USB/RPI (default), W=WIFI, E=Ethernet,"
                                  " N=non-enhanced USB/RPI/WIFI, F=non-enhanced Ethernet"
                                  " (prefer hard jumpers in lowercase, ignore hard jumpers in uppercase"
                                  ", since firmware 20221206)", 0 },
    {"hardvar", -4,  nullptr, 0, "set the variant from hard jumpers only (ignore soft jumpers)", 0 },
    {"flash",   'f', "FILE",  0, "flash the FILE to the device", 0 },
    {"reset",   'r', nullptr, 0, "reset the device at the end on success", 0 },
    {"slow",    's', nullptr, 0, "use low speed for transfer", 0 },
    {nullptr,          0,        nullptr,    0, nullptr, 0 },
};

static bool verbose = false;
static bool setDhcp = false;
static bool setIp = false;
static uint8_t setIpAddress[] = {0, 0, 0, 0};
static bool setMacFromIp = false;
static bool setMacFromIpValue = true;
static bool setMask = false;
static uint8_t setMaskLen = 0x1f;
static bool setGateway = false;
uint32_t setGatewayBits = 0;
static bool setArbitrationDelay = false;
static uint16_t setArbitrationDelayMicros = 0;
static bool setVisualPing = false;
static bool setVisualPingOn = false;
static bool setSoftVariant = false;
static uint8_t setSoftVariantValue = 0;
static bool setSoftVariantForced = false;
static bool setHardVariant = false;
static char* flashFile = nullptr;
static bool reset = false;
static bool lowSpeed = false;

bool parseByte(const char *arg, uint8_t minValue, uint8_t maxValue, uint8_t *result) {
  char* strEnd = nullptr;
  unsigned long value = 0;
  strEnd = nullptr;
  value = strtoul(arg, &strEnd, 10);
  if (strEnd == nullptr || strEnd == arg || *strEnd != 0) {
    return false;
  }
  if (value<minValue || value>maxValue) {
    return false;
  }
  *result = (uint8_t)value;
  return true;
}

bool parseShort(const char *arg, uint16_t minValue, uint16_t maxValue, uint16_t *result) {
  char* strEnd = nullptr;
  unsigned long value = 0;
  strEnd = nullptr;
  value = strtoul(arg, &strEnd, 10);
  if (strEnd == nullptr || strEnd == arg || *strEnd != 0) {
    return false;
  }
  if (value<minValue || value>maxValue) {
    return false;
  }
  *result = (uint16_t)value;
  return true;
}

error_t parse_opt(int key, char *arg, struct argp_state *state) {
  char *ip = nullptr, *part = nullptr;
  int pos = 0, sum = 0;
  struct stat st;
  uint32_t hostBits = 0;
  switch (key) {
    case 'v':  // --verbose
      verbose = true;
      break;
    case 'd':  // --dhcp
      if (setIp || setMask || setGateway) {
        argp_error(state, "either DHCP or IP address is needed");
        return EINVAL;
      }
      setDhcp = true;
      break;
    case 'i':  // --ip=192.168.0.10
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid IP address");
        return EINVAL;
      }
      if (setDhcp) {
        argp_error(state, "either DHCP or IP address is needed");
        return EINVAL;
      }
      if (setIp) {
        argp_error(state, "IP address was specified twice");
        return EINVAL;
      }
      ip = strdup(arg);
      part = strtok(ip, ".");

      for (pos=0; part && pos < 4; pos++) {
        if (!parseByte(part, 0, 255, setIpAddress+pos)) {
          break;
        }
        sum += setIpAddress[pos];
        part = strtok(nullptr, ".");
      }
      free(ip);
      if (pos != 4 || part || sum == 0) {
        argp_error(state, "invalid IP address");
        return EINVAL;
      }
      setIp = true;
      break;
    case 'm':  // --mask=24
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid IP mask");
        return EINVAL;
      }
      if (setDhcp) {
        argp_error(state, "either DHCP or IP address is needed");
        return EINVAL;
      }
      if (setMask) {
        argp_error(state, "mask was specified twice");
        return EINVAL;
      }
      if (!parseByte(arg, 1, 0x1e, &setMaskLen)) {
        argp_error(state, "invalid IP mask");
        return EINVAL;
      }
      setMask = true;
      break;
    case 'g':  // --gateway=192.168.0.11
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid gateway");
        return EINVAL;
      }
      if (setDhcp) {
        argp_error(state, "either DHCP or IP address is needed");
        return EINVAL;
      }
      if (!setIp || !setMask) {
        argp_error(state, "IP and mask need to be specified before gateway");
        return EINVAL;
      }
      ip = strdup(arg);
      part = strtok(ip, ".");
      setGatewayBits = 0;
      hostBits = 0;
      for (pos=0; part && pos < 4; pos++) {
        uint8_t address = 0;
        if (!parseByte(part, 0, 255, &address)) {
          break;
        }
        sum += address;
        part = strtok(nullptr, ".");
        uint8_t maskRemain = setMaskLen-pos*8;
        uint8_t mask = maskRemain >= 8 ? 255 : maskRemain == 0 ? 0 : (255^((1 << (8 - maskRemain)) - 1));
        if ((address & mask) != (setIpAddress[pos] & mask)) {
          argp_error(state, "invalid gateway (different network)");
          free(ip);
          return EINVAL;
        }
        setGatewayBits = (setGatewayBits << 8) | (address & ~mask);
        hostBits = (hostBits << 8) | (setIpAddress[pos] & ~mask);
      }
      free(ip);
      if (pos != 4 || part || sum == 0 || setGatewayBits == 0) {
        argp_error(state, "invalid gateway");
        return EINVAL;
      }
      if (setGatewayBits == hostBits) {
        argp_error(state, "invalid gateway (same as address)");
        return EINVAL;
      }
      if (!setGatewayBits || setGatewayBits == ((1 << (32 - setMaskLen)) - 1)) {
        argp_error(state, "invalid gateway (net or broadcast address)");
        return EINVAL;
      }
      if (setGatewayBits == 1) {  // default
        setGatewayBits = 0x3f;
        setGateway = true;
        break;
      }
      if (setMaskLen >= 27) {
        // fine: all bits are available
        setGateway = true;
        break;
      }
      if (!(setGatewayBits >> 5)) {
        if (!(setGatewayBits & 0x1f)) {
          argp_error(state, "invalid gateway (net address)");
          return EINVAL;
        }
        // fine: host part above max gateway adjustable bits is the same and remainder non-zero
        setGatewayBits &= 0x1f;
        setGateway = true;
        break;
      }
      if ((setGatewayBits >> 5) == ((1<<((32-setMaskLen)-5))-1)) {
        // fine: host part above max gateway adjustable bits is all 1
        setGatewayBits = 0x20 | (setGatewayBits & 0x1f);
        setGateway = true;
        break;
      }
      argp_error(state, "invalid gateway (out of possible range of first/last 31 hosts in subnet)");
      return EINVAL;
    case 'M':  // --macip
      setMacFromIp = true;
      setMacFromIpValue = true;
      break;
    case 'I':  // --macid
      setMacFromIp = true;
      setMacFromIpValue = false;
      break;
    case 'a':  // --arbdel=1000
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid arbitration delay");
        return EINVAL;
      }
      if (!parseShort(arg, 0, 620, &setArbitrationDelayMicros)) {
        argp_error(state, "invalid arbitration delay");
        return EINVAL;
      }
      setArbitrationDelay = true;
      break;
    case 'p':  // --pingon
      setVisualPing = true;
      setVisualPingOn = true;
      break;
    case 'o':  // --pingoff
      setVisualPing = true;
      setVisualPingOn = false;
      break;
    case -3:  // --softvar=U|W|E|F|N|u|w|e|f|n
      if (setHardVariant) {
        argp_error(state, "can't set hard and soft jumpers");
        return EINVAL;
      }
      if (arg == nullptr || arg[0] == 0) {
        argp_error(state, "invalid variant");
        return EINVAL;
      }
      if (arg[0] == 'u' || arg[0] == 'U') {
        setSoftVariantValue = 3;
      } else if (arg[0] == 'w' || arg[0] == 'W') {
        setSoftVariantValue = 2;
      } else if (arg[0] == 'e' || arg[0] == 'E') {
        setSoftVariantValue = 1;
      } else if (arg[0] == 'f' || arg[0] == 'F') {
        setSoftVariantValue = 4;
      } else if (arg[0] == 'n' || arg[0] == 'N') {
        setSoftVariantValue = 0;
      } else {
        argp_error(state, "invalid variant");
        return EINVAL;
      }
      setSoftVariantForced = arg[0]<'a';
      setSoftVariant = true;
      break;
    case -4:  // --hardvar
      if (setSoftVariant) {
        argp_error(state, "can't set hard and soft jumpers");
        return EINVAL;
      }
      setSoftVariantValue = 3;
      setSoftVariantForced = false;
      setHardVariant = true;
      break;
    case 'f':  // --flash=firmware.hex
      if (arg == nullptr || arg[0] == 0 || stat(arg, &st) != 0 || !S_ISREG(st.st_mode)) {
        argp_error(state, "invalid flash file");
        return EINVAL;
      }
      flashFile = arg;
      break;
    case 'r':  // --reset
      reset = true;
      break;
    case 's':  // --slow
      lowSpeed = true;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

// START: copy from generated bootloader

#define WRITE_FLASH_BLOCKSIZE    32
#define ERASE_FLASH_BLOCKSIZE    32
#define END_FLASH                0x4000

// Frame Format
//
//  [<COMMAND><DATALEN><ADDRL><ADDRH><ADDRU><...DATA...>]
// These values are negative because the FSR is set to PACKET_DATA to minimize FSR reloads.
typedef union
{
  struct __attribute__((__packed__))
  {
    uint8_t     command;
    uint16_t    data_length;
    uint8_t     EE_key_1;
    uint8_t     EE_key_2;
    uint8_t     address_L;
    uint8_t     address_H;
    uint8_t     address_U;
    uint8_t     address_unused;
    uint8_t     data[2*WRITE_FLASH_BLOCKSIZE];
  };
  uint8_t  buffer[2*WRITE_FLASH_BLOCKSIZE+9];
}frame_t;

#define  STX   0x55

#define  READ_VERSION   0
#define  READ_FLASH     1
#define  WRITE_FLASH    2
#define  ERASE_FLASH    3
#define  READ_EE_DATA   4
#define  WRITE_EE_DATA  5
#define  READ_CONFIG    6
#define  WRITE_CONFIG   7
#define  CALC_CHECKSUM  8
#define  RESET_DEVICE   9
#define  CALC_CRC       10

#define MINOR_VERSION   0x08       // Version
#define MAJOR_VERSION   0x00
//#define STX             0x55       // Actually code 0x55 is 'U'  But this is what the autobaud feature of the PIC16F1 EUSART is looking for
#define ERROR_ADDRESS_OUT_OF_RANGE   0xFE
#define ERROR_INVALID_COMMAND        0xFF
#define COMMAND_SUCCESS              0x01

// END: copy from generated bootloader

#define FRAME_HEADER_LEN 9
#define FRAME_MAX_LEN (FRAME_HEADER_LEN+2*WRITE_FLASH_BLOCKSIZE)
#define BAUDRATE_LOW B115200
#define BAUDRATE_HIGH B921600
#define WAIT_BYTE_TRANSFERRED_MILLIS 200
#define WAIT_BITRATE_DETECTION_MICROS 100
#define WAIT_RESPONSE_TIMEOUT_MILLIS 100
// size of flash in bytes
#define END_FLASH_BYTES (END_FLASH*2)
// size of boot block in words
#define END_BOOT 0x0400
// size of boot block in bytes
#define END_BOOT_BYTES (END_BOOT*2)

static bool isSerial = true;
static int timeoutFactor = 1;
static int timeoutAddend = 0;

ssize_t waitWrite(int fd, uint8_t *data, size_t len, int timeoutMillis) {
  int ret;
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLOUT | POLLERR | POLLHUP;
  ret = poll(&pfd, 1, timeoutMillis*timeoutFactor + timeoutAddend);
  if (ret >= 0 && pfd.revents & (POLLERR | POLLHUP)) {
    return -1;
  }
  if (ret <= 0) {
    return ret;
  }
  ret = write(fd, data, len);
  if (ret < 0) {
    return ret;
  }
#ifdef DEBUG_RAW
  std::cout << "> " << std::dec << static_cast<unsigned>(ret) << "/" << static_cast<unsigned>(len) << ":" << std::hex;
  for (int pos = 0; pos < ret; pos++) {
    std::cout << " " << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[pos]);
  }
  std::cout << std::endl;
#endif
  return ret;
}

ssize_t waitRead(int fd, uint8_t *data, size_t len, int timeoutMillis) {
  int ret;
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN | POLLERR | POLLHUP;
  ret = poll(&pfd, 1, timeoutMillis*timeoutFactor + timeoutAddend);
  if (ret >= 0 && pfd.revents & (POLLERR | POLLHUP)) {
    return -1;
  }
  if (ret <= 0) {
    return ret;
  }
  ret = read(fd, data, len);
  if (ret < 0) {
    return ret;
  }
#ifdef DEBUG_RAW
  std::cout << "< " << std::dec << static_cast<unsigned>(ret) << "/" << static_cast<unsigned>(len) << ":" << std::hex;
  for (int pos = 0; pos < ret; pos++) {
    std::cout << " " << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[pos]);
  }
  std::cout << std::endl;
#endif
  return ret;
}

ssize_t sendReceiveFrame(int fd, frame_t& frame, size_t sendDataLen, ssize_t fixReceiveDataLen,
                         int responseTimeoutExtraMillis = 0, bool hideErrors = false) {
  // send 0x55 for auto baud detection in PIC
  unsigned char ch = STX;
  ssize_t cnt = waitWrite(fd, &ch, 1, WAIT_BYTE_TRANSFERRED_MILLIS);
  if (cnt < 0) {
    if (!hideErrors) {
      std::cerr << "write sync failed" << std::endl;
    }
    return cnt;
  }
  if (cnt == 0) {
    if (!hideErrors) {
      std::cerr << "write sync timed out" << std::endl;
    }
    return cnt;
  }
  // wait for bitrate detection to finish in PIC
  usleep(WAIT_BITRATE_DETECTION_MICROS);
  uint8_t writeCommand = frame.command;
  size_t len = FRAME_HEADER_LEN+sendDataLen;
  for (size_t pos=0; pos < len; ) {
    cnt = waitWrite(fd, frame.buffer+pos, len-pos, WAIT_BYTE_TRANSFERRED_MILLIS);
    if (cnt < 0) {
      if (!hideErrors) {
        std::cerr << "write data failed" << std::endl;
      }
      return cnt;
    }
    if (cnt == 0) {
      if (!hideErrors) {
        std::cerr << "write data timed out" << std::endl;
      }
      return -1;
    }
    pos += cnt;
  }
  cnt = waitRead(fd, &ch, 1, WAIT_RESPONSE_TIMEOUT_MILLIS + responseTimeoutExtraMillis);
  if (cnt < 0) {
    if (!hideErrors) {
      std::cerr << "read sync failed" << std::endl;
    }
    return cnt;
  }
  if (cnt == 0) {
    if (!hideErrors) {
      std::cerr << "read sync timed out" << std::endl;
    }
    return -1;
  }
  if (ch != STX) {
    if (!hideErrors) {
      std::cerr << "did not receive sync: 0x" << std::setfill('0') << std::setw(2) << std::hex
                << static_cast<unsigned>(ch) << std::endl;
    }
    return -1;
  }
  // read the answer from the device
  len = FRAME_HEADER_LEN;  // start with the header itself
  for (size_t pos=0; pos < len; ) {
    cnt = waitRead(fd, frame.buffer+pos, len-pos, WAIT_BYTE_TRANSFERRED_MILLIS);
    if (cnt < 0) {
      if (!hideErrors) {
        std::cerr << "read data failed" << std::endl;
      }
      return cnt;
    }
    if (cnt == 0) {
      if (!hideErrors) {
        std::cerr << "read data timed out" << std::endl;
      }
      return -1;
    }
    pos += cnt;
    if (pos == FRAME_HEADER_LEN) {
      if (fixReceiveDataLen < 0) {
        len += frame.data_length;
      } else {
        len += fixReceiveDataLen;
      }
      fixReceiveDataLen = 0;
    }
  }
  uint8_t dummy[4];
  waitRead(fd, dummy, 4, WAIT_BYTE_TRANSFERRED_MILLIS);  // read away potential nonsense tail
  if (frame.command != writeCommand) {
    if (!hideErrors) {
      std::cerr << "unexpected answer" << std::endl;
    }
    return -1;
  }
  return 0;
}

int readVersion(int fd, bool verbose = true) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = READ_VERSION;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, 16);
  if (ret != 0) {
    return ret;
  }
  if (frame.data[0] != MINOR_VERSION || frame.data[1] != MAJOR_VERSION) {
    std::cerr << "unexpected version" << std::endl;
    return -1;
  }
  if (verbose) {
    std::cout << "Max packet size: " << static_cast<unsigned>(frame.data[2] | (frame.data[3] << 8)) << std::endl;
  }
  std::cout << "Device ID: " << std::setfill('0') << std::setw(4) << std::hex
            << static_cast<unsigned>(frame.data[6] | (frame.data[7] << 8));
  if (frame.data[6] == 0xb0 && frame.data[7] == 0x30) {
    std::cout << " (PIC16F15356)";
  }
  std::cout << std::endl;
  if (verbose) {
    std::cout << "Blocksize erase: " << std::dec << static_cast<unsigned>(frame.data[10]) << std::endl;
    std::cout << "Blocksize write: " << std::dec << static_cast<unsigned>(frame.data[11]) << std::endl;
    std::cout << "User ID 1: " << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<unsigned>(frame.data[12]) << std::endl;
    std::cout << "User ID 2: " << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<unsigned>(frame.data[13]) << std::endl;
    std::cout << "User ID 3: " << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<unsigned>(frame.data[14]) << std::endl;
    std::cout << "User ID 4: " << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<unsigned>(frame.data[15]) << std::endl;
  }
  return 0;
}

int printFrameData(frame_t frame, bool skipHigh) {
  uint16_t address = (frame.address_H << 8)|frame.address_L;
  int pos;
  std::cout << std::hex;
  for (pos = 0; pos < frame.data_length;) {
    if ((pos%16) == 0) {
      std::cout << std::setw(4) << static_cast<unsigned>(address) << ":";
    }
    std::cout << " " << std::setw(2) << static_cast<unsigned>(frame.data[pos++]);
    if (skipHigh) {
      pos++;
    } else if (pos < frame.data_length) {
      std::cout << " " << std::setw(2) << static_cast<unsigned>(frame.data[pos++]);
    }
    address++;
    if ((pos%16) == 0) {
      std::cout << std::endl;
    }
  }
  if ((pos%16) != 0) {
    std::cout << std::endl;
  }
  return 0;
}

int printFrame(frame_t frame) {
  std::cout << "command:     0x" << std::setfill('0') << std::setw(2) << std::hex
            << static_cast<unsigned>(frame.command) << std::endl;
  std::cout << "data_length: " << std::dec << static_cast<unsigned>(frame.data_length) << std::endl;
  std::cout << "address:     0x" << std::setw(2) << std::hex << static_cast<unsigned>(frame.address_H) << std::setw(2)
            << std::hex << static_cast<unsigned>(frame.address_L);
  for (int pos = 0; pos < frame.data_length; ) {
    if ((pos%16) == 0) {
      std::cout << std::endl << std::setw(4) << static_cast<unsigned>(pos) << ":" << std::endl;
    }
    std::cout << " " << std::setw(2) << static_cast<unsigned>(frame.data[pos++]);
    pos++;
  }
  std::cout << std::endl;
  return 0;
}

int readConfig(int fd, uint16_t address, uint16_t len, bool skipHigh = false, bool print = true,
               uint8_t* storeData = nullptr) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = READ_CONFIG;
  frame.data_length = len;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, len);
  if (ret != 0) {
    return ret;
  }
  if (print) {
    printFrameData(frame, skipHigh);
  }
  if (storeData) {
    memcpy(storeData, frame.data, len);
  }
  return 0;
}

int writeConfig(int fd, uint16_t address, uint16_t len, uint8_t* data) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = WRITE_CONFIG;
  frame.data_length = len;
  frame.EE_key_1 = 0x55;
  frame.EE_key_2 = 0xaa;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  memcpy(frame.data, data, len);
  ssize_t ret = sendReceiveFrame(fd, frame, len, 1, 50);
  if (ret != 0) {
    return ret;
  }
  if (frame.data[0] != COMMAND_SUCCESS) {
    return -1;
  }
  return 0;
}

int readFlash(int fd, uint16_t address, bool skipHigh = false, bool print = true, uint8_t* storeData = nullptr) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = READ_FLASH;
  frame.data_length = 0x10;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, -1);
  if (ret != 0) {
    return ret;
  }
  if (print) {
    printFrameData(frame, skipHigh);
  }
  if (storeData) {
    memcpy(storeData, frame.data, 0x10);
  }
  return 0;
}

int writeFlash(int fd, uint16_t address, uint16_t len, uint8_t* data, bool hideErrors = false) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = WRITE_FLASH;
  frame.data_length = len;
  frame.EE_key_1 = 0x55;
  frame.EE_key_2 = 0xaa;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  memcpy(frame.data, data, len);
  ssize_t ret = sendReceiveFrame(fd, frame, len, 1, len*30, hideErrors);
  if (ret != 0) {
    return ret;
  }
  if (frame.data[0] != COMMAND_SUCCESS) {
    return -1;
  }
  return 0;
}

int eraseFlash(int fd, uint16_t address, uint16_t len) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = ERASE_FLASH;
  frame.data_length = (len+ERASE_FLASH_BLOCKSIZE-1)/ERASE_FLASH_BLOCKSIZE;
  frame.EE_key_1 = 0x55;
  frame.EE_key_2 = 0xaa;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, 1, frame.data_length*5);
  if (ret != 0) {
    return ret;
  }
  if (frame.data[0] != COMMAND_SUCCESS) {
    return -frame.data[0]-1;
  }
  return 0;
}

int calcChecksum(int fd, uint16_t address, uint16_t len) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = CALC_CHECKSUM;
  frame.data_length = len;
  frame.address_L = address&0xff;
  frame.address_H = (address>>8)&0xff;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, 2, len*30);
  if (ret != 0) {
    return ret;
  }
  return frame.data[0] | (frame.data[1] << 8);
}

int resetDevice(int fd) {
  frame_t frame;
  memset(frame.buffer, 0, FRAME_MAX_LEN);
  frame.command = RESET_DEVICE;
  ssize_t ret = sendReceiveFrame(fd, frame, 0, 1);
  if (ret != 0) {
    return ret;
  }
  if (frame.data[0] != COMMAND_SUCCESS) {
    return -frame.data[0]-1;
  }
  return 0;
}

struct termios termios_original;

int openSerial(std::string port) {
  // open serial port
  int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);  // non-blocking IO
  if (fd == -1) {
    std::cerr << "unable to open " << port << std::endl;
    return -1;
  }

  if (flock(fd, LOCK_EX|LOCK_NB)) {
    close(fd);
    std::cerr << "unable to lock " << port << std::endl;
    return -1;
  }

  // backup terminal settings
  tcgetattr(fd, &termios_original);

  // configure terminal settings
  struct termios termios;
  memset(&termios, 0, sizeof(termios));

  if (cfsetspeed(&termios, lowSpeed ? BAUDRATE_LOW : BAUDRATE_HIGH) != 0) {
    std::cerr << "unable to set speed " << std::endl;
    close(fd);
    return -1;
  }
  termios.c_iflag |= 0;
  termios.c_oflag |= 0;
  termios.c_cflag |= CS8 | CREAD | CLOCAL;
  termios.c_lflag |= 0;
  termios.c_cc[VMIN] = 1;
  termios.c_cc[VTIME] = 0;
  if (tcsetattr(fd, TCSANOW, &termios) != 0) {
    std::cerr << "unable to set serial " << std::endl;
    close(fd);
    return -1;
  }
  std::cout << "opened " << port << std::endl;
  return fd;
}

int openNet(std::string host, uint16_t port) {
  // open network port
  int fd = socketConnect(host.c_str(), port, false, nullptr, 5);
  if (fd < 0) {
    std::cerr << "unable to open " << host << std::endl;
    return -1;
  }
  fcntl(fd, F_SETFL, O_NONBLOCK);  // set non-blocking
  std::cout << "opened " << host << ":" << static_cast<unsigned>(port) << std::endl;
  return fd;
}

void closeConnection(int fd) {
  if (isSerial) {
    tcsetattr(fd, TCSANOW, &termios_original);
  }
  close(fd);
}

int calcFileChecksum(uint8_t* storeFirstBlock = nullptr) {
  std::ifstream inStream;
  inStream.open(flashFile, ifstream::in);
  if (!inStream.good()) {
    std::cerr << "unable to open file" << std::endl;
    return -1;
  }
  intelhex ih;
  inStream >> ih;
  if (ih.getNoErrors() > 0 || ih.getNoWarnings() > 0) {
    std::cerr << "unable to read file" << std::endl;
    return -1;
  }
  unsigned long startAddr = 0, endAddr = 0;
  if (!ih.startAddress(&startAddr) || !ih.endAddress(&endAddr)) {
    std::cerr << "unable to read file" << std::endl;
    return -1;
  }
  if (startAddr < END_BOOT_BYTES || endAddr >= END_FLASH_BYTES || endAddr < startAddr || (startAddr&0xf) != 0) {
    std::cerr << "invalid address range" << std::endl;
    return -1;
  }
  ih.begin();
  unsigned long nextAddr = ih.currentAddress();
  if (nextAddr != END_BOOT_BYTES) {
    std::cerr << "unexpected start address in file." << std::endl;
    return -1;
  }
  unsigned long blockStart = END_BOOT_BYTES;
  uint16_t checkSum = 0;
  uint16_t skipped = 0;
  while (blockStart < END_FLASH_BYTES && nextAddr < END_FLASH_BYTES) {
    for (int pos = 0; pos < WRITE_FLASH_BLOCKSIZE; pos++, nextAddr++) {
      unsigned long addr = ih.currentAddress();
      uint8_t value = (pos&0x1) == 1 ? 0x3f : 0xff;
      if (addr == nextAddr && ih.getData(&value)) {
        ih.incrementAddress();
      } else {
        skipped++;
      }
      if (storeFirstBlock && nextAddr < END_BOOT_BYTES+0x10) {
        storeFirstBlock[pos] = value;
      }
      checkSum += ((uint16_t)value) << ((pos&0x1)*8);
    }
    blockStart += WRITE_FLASH_BLOCKSIZE;
  }
  if (nextAddr-END_BOOT_BYTES != ih.size()+skipped) {
    std::cout << "unable to fully read file." << std::endl;
    return -1;
  }
  return checkSum;
}

void printFileChecksum() {
  uint8_t data[0x10];
  int checkSum = calcFileChecksum(data);
  if (checkSum < 0) {
    return;
  }
  int newFirmwareVersion = -1;
  if (data[0x2*2] == 0xae && data[0x2*2+1] == 0x34 && data[0x3*2+1] == 0x34) {
    newFirmwareVersion = data[0x3*2];
  }
  std::cout
    << "New firmware version: " << static_cast<unsigned>(newFirmwareVersion)
    << " [" << std::hex << std::setw(4) << std::setfill('0') << static_cast<signed>(checkSum) << "]" << std::endl;
}

bool flashPic(int fd) {
  std::ifstream inStream;
  inStream.open(flashFile, ifstream::in);
  if (!inStream.good()) {
    std::cerr << "unable to open file" << std::endl;
    return false;
  }
  intelhex ih;
//  if (verbose) {
//    ih.verboseOn();
//  }
  inStream >> ih;
  if (ih.getNoErrors() > 0 || ih.getNoWarnings() > 0) {
    std::cerr << "errors or warnings while reading the file:" << std::endl;
    string str;
    while (ih.popNextWarning(str)) {
      std::cerr << "warning: " << str << std::endl;
    }
    while (ih.popNextError(str)) {
      std::cerr << "error: " << str << std::endl;
    }
    return false;
  }
  unsigned long startAddr = 0, endAddr = 0;
  if (!ih.startAddress(&startAddr) || !ih.endAddress(&endAddr)) {
    std::cerr << "unable to read file" << std::endl;
    return false;
  }
  if (verbose) {
    std::cout << "flashing bytes 0x"
              << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(startAddr)
              << " - 0x"
              << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(endAddr)
              << std::endl;
  }
  if (startAddr < END_BOOT_BYTES || endAddr >= END_FLASH_BYTES || endAddr < startAddr || (startAddr&0xf) != 0) {
    std::cerr << "invalid address range" << std::endl;
    return false;
  }
  ih.begin();
  uint8_t buf[WRITE_FLASH_BLOCKSIZE];
  unsigned long nextAddr = ih.currentAddress();
  if (nextAddr != END_BOOT_BYTES) {
    std::cerr << "unexpected start address in file: 0x" << std::hex << std::setfill('0') << std::setw(4)
              << static_cast<unsigned>(nextAddr) << std::endl;
    return false;
  }
  unsigned long blockStart = END_BOOT_BYTES;
  uint16_t checkSum = 0;
  uint16_t skipped = 0;
  int eraseRes = eraseFlash(fd, blockStart/2, (endAddr-blockStart)/2);
  if (eraseRes != 0) {
    std::cerr << "erasing flash failed: " << static_cast<signed>(-eraseRes-1) << std::endl;
    return false;
  }
  std::cout << "erasing flash: done." << std::endl;
  std::cout << "flashing: 0x" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(nextAddr/2)
            << " - 0x" << static_cast<unsigned>(endAddr/2) << std::endl;
  size_t blocks = 0;
  while (blockStart < endAddr) {
    bool blank = true;
    for (int pos = 0; pos < WRITE_FLASH_BLOCKSIZE; pos++, nextAddr++) {
      unsigned long addr = ih.currentAddress();
      uint8_t value = (pos&0x1) == 1 ? 0x3f : 0xff;
      if (addr == nextAddr && ih.getData(&value)) {
        ih.incrementAddress();
        blank = false;
      } else {
        skipped++;
      }
      buf[pos] = value;
      checkSum += ((uint16_t)value) << ((pos&0x1)*8);
    }
    if (!blank) {
      if (blocks == 0) {
        std::cout << std::endl << "0x" << std::hex << std::setfill('0') << std::setw(4)
                  << static_cast<unsigned>(blockStart/2) << " ";
      }
      if (writeFlash(fd, blockStart/2, WRITE_FLASH_BLOCKSIZE, buf, true) != 0) {
        // repeat once silently:
        if (writeFlash(fd, blockStart/2, WRITE_FLASH_BLOCKSIZE, buf) != 0) {
          std::cerr << "unable to write flash at 0x" << std::hex << std::setfill('0') << std::setw(4)
                    << static_cast<unsigned>(blockStart/2) << std::endl;
          return false;
        }
      }
      std::cout << ".";
      if (++blocks >= 64) {
        blocks = 0;
      }
      std::cout.flush();
    }
    blockStart += WRITE_FLASH_BLOCKSIZE;
  }
  std::cout << std::endl << "flashing finished." << std::endl;
  if (nextAddr-END_BOOT_BYTES != ih.size()+skipped) {
    std::cout << "unable to fully read file." << std::endl;
  }
  int picSum = calcChecksum(fd, startAddr/2, blockStart-startAddr);
  if (picSum < 0) {
    std::cout << "unable to read checksum." << std::endl;
    return false;
  }
  if (picSum != checkSum) {
    std::cout << "unexpected checksum." << std::endl;
    return false;
  }
  std::cout << "flashing succeeded." << std::endl;
  return true;
}

int readSettings(int fd, uint8_t* currentData = nullptr) {
  uint8_t mac[] = {0xae, 0xb0, 0x53, 0xef, 0xfe, 0xef};  // "Adapter-eBUS3" + (UserID or MUI)
  uint8_t ip[4] = {0, 0, 0, 0};
  bool useMUI = true;
  uint8_t maskLen = 0;
  uint8_t configData[8];
  if (readConfig(fd, 0x0000, 8, false, false, configData) != 0) {  // User ID
    return -1;
  }
  if (currentData) {
    memcpy(currentData, configData, sizeof(configData));
  }
  useMUI = (configData[1]&0x20) != 0;  // if highest bit is set, then use MUI. if cleared, use User ID
  maskLen = configData[1]&0x1f;
  uint8_t gw = configData[7]&0x3f;
  for (int i=0; i < 4; i++) {
    ip[i] = configData[i*2];
    if (!useMUI && i > 0) {
      mac[2+i] = configData[i*2];
    }
  }
  if (useMUI) {
    // read MUI to build uniqueMAC address
    // start with MUI6, end with MUI8 (MUI9 is reserved)
    uint8_t mui[8];
    readConfig(fd, 0x0106, 8, true, false, mui);  // MUI
    for (int i=0; i < 3; i++) {
      mac[3+i] = mui[i*2];
    }
  }
  std::cout << "MAC address:";
  for (int i=0; i < 6; i++) {
    std::cout << (i == 0?' ':':') << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(mac[i]);
  }
  std::cout << std::endl;
  if (maskLen == 0x1f || (ip[0]|ip[1]|ip[2]|ip[3]) == 0) {
    std::cout << "IP address: DHCP (default)" << std::endl;
  } else {
    std::cout << "IP address:";
    for (uint8_t pos = 0, maskRemain = maskLen; pos < 4; pos++, maskRemain -= maskRemain >= 8 ? 8 : maskRemain) {
      std::cout << (pos == 0?' ':'.') << std::dec << static_cast<unsigned>(ip[pos]);
      uint8_t mask = maskRemain >= 8 ? 255 : maskRemain == 0 ? 0 : (255 ^ ((1 << (8 - maskRemain)) - 1));
      ip[pos] &= mask;  // prepare for gateway
    }
    std::cout << "/" << std::dec << static_cast<unsigned>(maskLen) << ", gateway:";
    // build gateway
    if (gw == 0x3f) {
      // default: first address in network is used as gateway
      ip[3] |= 1;
    } else if (gw & 0x20) {
      // end of subnet
      // non-mask bits outside of |gw reach
      uint8_t mask = maskLen <= 24 ? 0 : (255^((1 << (8 - (maskLen-24))) - 1));
      ip[3] |= ((~mask)^0x1f) | (gw&0x1f);
      if (maskLen<24) {
        // more than just the last IP byte are affected: set non-mask bits to 1 as well in bytes 0-2
        for (uint8_t pos = 0, maskRemain = maskLen; pos < 3; pos++, maskRemain -= maskRemain >= 8 ? 8 : maskRemain) {
          mask = maskRemain >= 8 ? 255 : maskRemain == 0 ? 0 : (255^((1 << (8 - maskRemain)) - 1));
          ip[pos] |= ~mask;
        }
      }
    } else {
      // start of subnet
      ip[3] |= gw&0x1f;
    }
    for (int i=0; i < 4; i++) {
      std::cout << (i == 0?' ':'.') << std::dec << static_cast<unsigned>(ip[i]);
    }
    std::cout << std::endl;
  }
  uint16_t arbitrationDelay = configData[3]&0x3f;
  std::cout << "Arbitration delay: ";
  if (arbitrationDelay == 0x3f) {
    std::cout << "200 us (default)" << std::endl;
  } else {
    arbitrationDelay *= 10;  // steps of 10us
    std::cout << std::dec << static_cast<unsigned>(arbitrationDelay) << " us" << std::endl;
  }
  std::cout << "Visual ping: ";
  if (configData[5]&0x20) {
    std::cout << "on (default)" << std::endl;
  } else {
    std::cout << "off" << std::endl;
  }
  std::cout << "Variant: "; // since firmware 20221206
  if ((configData[5]&0x07)==0x07) {
    std::cout << "hard jumpers only (includes USB/RPI enhanced when no jumpers are set)" << std::endl;
  } else {
    switch (configData[5]&0x03) {
      case 3:
        std::cout << "USB/RPI";
        break;
      case 2:
        std::cout << "WIFI";
        break;
      case 1:
        std::cout << "Ethernet";
        break;
      default:
        std::cout << "non-enhanced ";
        if (maskLen) {
          std::cout << "Ethernet";
        } else {
          std::cout << "USB/RPI/WIFI";
        }
    }
    if (configData[5]&0x04) {
      std::cout << ", prefer hard jumpers";
    } else {
      std::cout << ", ignore hard jumpers";
    }
    std::cout << std::endl;
  }
  return 0;
}

bool writeSettings(int fd, uint8_t* currentData = nullptr) {
  std::cout << "Writing settings: ";
  uint8_t configData[] = {0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f};
  if (currentData) {
    memcpy(configData, currentData, sizeof(configData));
  }
  if (setMacFromIp) {
    configData[1] = (configData[1]&~0x20) | (setMacFromIpValue ? 0 : 0x20);  // set useMUI
  }
  if (setDhcp) {
    configData[1] |= 0x1f;
  } else if (setIp) {
    if (setMask) {
      configData[1] = (configData[1]&~0x1f) | (setMaskLen&0x1f);
    }
    for (int i = 0; i < 4; i++) {
      configData[i * 2] = setIpAddress[i];
    }
    if (setGateway) {
      configData[7] = setGatewayBits;
    }
  }
  if (setArbitrationDelay) {
    configData[3] = setArbitrationDelayMicros/10;
  }
  if (setVisualPing) {
    configData[5] = (configData[5]&0x1f) | (setVisualPingOn?0x20:0);
  }
  if (setSoftVariant) {
    configData[5] = (configData[5]&0x38) | (setSoftVariantForced?0:0x04) | (setSoftVariantValue&0x03);
    if (setSoftVariantValue==0) {
      configData[1] = (configData[1]&~0x1f); // set mask=0 to disable Ethernet
    }
  } else if (setHardVariant) {
    configData[5] = (configData[5]&0x38) | 0x07;
  }
  if (writeConfig(fd, 0x0000, 8, configData) != 0) {
    std::cerr << "failed" << std::endl;
    return false;
  }
  std::cout << "done." << std::endl;
  return true;
}

int run(int fd);

int main(int argc, char* argv[]) {
  struct argp aargp = { argpoptions, parse_opt, argpargsdoc, argpdoc, nullptr, nullptr, nullptr };
  int arg_index = -1;
  setenv("ARGP_HELP_FMT", "no-dup-args-note", 0);

  if (argp_parse(&aargp, argc, argv, ARGP_IN_ORDER, &arg_index, nullptr) != 0) {
    std::cerr << "invalid arguments" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (setIp != setMask || (setMacFromIp && !setIp)) {
    std::cerr << "incomplete IP arguments" << std::endl;
    arg_index = argc;  // force help output
  }
  if (argc-arg_index < 1) {
    if (flashFile) {
      printFileChecksum();
      exit(EXIT_SUCCESS);
    } else {
      argp_help(&aargp, stderr, ARGP_HELP_STD_ERR, const_cast<char*>("ebuspicloader"));
      exit(EXIT_FAILURE);
    }
  }
  std::string port = argv[arg_index];
  std::string::size_type pos = port.find('*');
  if (pos == std::string::npos || pos != port.length()-1) {
    int fd;
    pos = port.find(':');
    if (pos != std::string::npos) {
      string host = port.substr(0, pos);
      uint16_t portNum = 0;
      if (!parseShort(port.substr(pos+1).c_str(), 1, 65535, &portNum)) {
        exit(EXIT_FAILURE);
      }
      isSerial = false;
      timeoutFactor = 2;
      timeoutAddend = 100;
      fd = openNet(host, portNum);
    } else {
      fd = openSerial(port);
    }
    if (fd < 0) {
      exit(EXIT_FAILURE);
    }
    return run(fd);
  }

  std::string::size_type sep = port.find_last_of('/');
  std::string base = sep == std::string::npos ? "" : port.substr(0, sep);
  DIR* dir = opendir(base.c_str());
  if (!dir) {
    std::cerr << "Unable to open directory " << base << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string prefix = sep == std::string::npos ? port.substr(0, pos) : port.substr(sep + 1, pos - 1 - sep);
  struct dirent* ent;
  while ((ent = readdir(dir))) {
    if (std::string(ent->d_name).substr(0, prefix.length()) != prefix) {
      continue;
    }
    std::string name = base + "/" + ent->d_name;
    std::cout << "Trying " << name << "..." << std::endl;
    int fd = openSerial(name);
    if (fd < 0) {
      std::cerr << "Unable to open " << name << std::endl;
      continue;
    }
    run(fd);
    std::cout << std::endl;
  }
  return 0;
}

int run(int fd) {
  // read version
  if (readVersion(fd, verbose) != 0) {
    closeConnection(fd);
    return EXIT_FAILURE;
  }
  uint8_t data[0x10];
  if (verbose) {
    std::cout << "User ID:" << std::endl;
    readConfig(fd, 0x0000, 8);  // User ID
    std::cout << "Rev ID, Device ID:" << std::endl;
  }
  readConfig(fd, 0x0005, 4, false, verbose, data);  // Rev ID and Device ID
  std::cout << "Device revision: " << static_cast<unsigned>(((data[1]&0xf) << 2) | ((data[0]&0xc0)>>6))
            << "." << static_cast<unsigned>(data[0]&0x3f) << std::endl;
  if (verbose) {
    std::cout << "Configuration words:" << std::endl;
    readConfig(fd, 0x0007, 5*2);  // Configuration Words
    std::cout << "MUI:" << std::endl;
    readConfig(fd, 0x0100, 9*2, true);  // MUI
    std::cout << "EUI:"<< std::endl;
    readConfig(fd, 0x010a, 8*2);  // EUI
    readConfig(fd, 0x0116, 14, false, false, data);  // TSHR2...FVRC2X
    std::cout << "TSHR2: " << std::dec << static_cast<unsigned>(((data[1]&0xff) << 8) | (data[0]&0xff)) << std::endl;
    std::cout << "FVRA2X: " << static_cast<unsigned>(((data[7]&0xff) << 8) | (data[6]&0xff)) << std::endl;
    std::cout << "FVRC2X: " << static_cast<unsigned>(((data[13]&0xff) << 8) | (data[12]&0xff)) << std::endl;
  }
  if (verbose) {
    std::cout << "Flash:" << std::endl;
  }
  readFlash(fd, 0x0000, false, false, data);
  int bootloaderVersion = -1;
  if (data[0x2*2] == 0xab && data[0x2*2+1] == 0x34 && data[0x3*2+1] == 0x34) {
    bootloaderVersion = data[0x3*2];
    int picSum = calcChecksum(fd, 0x0000, END_BOOT_BYTES);
    std::cout
      << "Bootloader version: " << static_cast<unsigned>(bootloaderVersion)
      << " [" << std::hex << std::setw(4) << std::setfill('0') << static_cast<signed>(picSum) << "]" << std::endl;
  } else {
    std::cerr << "Bootloader version not found" << std::endl;
  }
  readFlash(fd, END_BOOT, false, false, data);
  int firmwareVersion = -1;
  if (data[0x2*2] == 0xae && data[0x2*2+1] == 0x34 && data[0x3*2+1] == 0x34) {
    firmwareVersion = data[0x3*2];
    int picSum = calcChecksum(fd, END_BOOT, END_FLASH_BYTES-END_BOOT_BYTES);
    std::cout
      << "Firmware version: " << static_cast<unsigned>(firmwareVersion)
      << " [" << std::hex << std::setw(4) << std::setfill('0') << static_cast<signed>(picSum) << "]" << std::endl;
  } else {
    std::cout << "Firmware version not found" << std::endl;
  }
  uint8_t currentConfigData[8];
  bool useCurrentConfigData = true;
  if (readSettings(fd, currentConfigData) != 0) {
    std::cerr << "Settings could not be retrieved" << std::endl;
    useCurrentConfigData = false;
  }
  std::cout << std::endl;
  bool success = true;
  if (flashFile) {
    printFileChecksum();
    if (!flashPic(fd)) {
      success = false;
    }
  }
  if (setMacFromIp || setIp || setDhcp || setArbitrationDelay || setVisualPing || setSoftVariant || setHardVariant) {
    if (writeSettings(fd, useCurrentConfigData ? currentConfigData : nullptr)) {
      std::cout << "Settings changed to:" << std::endl;
      readSettings(fd);
    } else {
      success = false;
    }
  }
  if (reset && success) {
    std::cout << "resetting device." << std::endl;
    resetDevice(fd);
  }

  closeConnection(fd);
  return 0;
}

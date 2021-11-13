cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

# CMP0075 Include file check macros honor CMAKE_REQUIRED_LIBRARIES
# For more information see: https://cmake.org/cmake/help/latest/policy/CMP0075.html
if(POLICY CMP0075)
    cmake_policy(SET CMP0075 NEW)
endif()

file(STRINGS "VERSION" VERSION)

project(ebusd)

set(PACKAGE ${CMAKE_PROJECT_NAME})
set(PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(PACKAGE_TARNAME ${CMAKE_PROJECT_NAME})
set(PACKAGE_VERSION ${VERSION})
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "ebusd@ebusd.eu")
set(PACKAGE_URL "https://github.com/john30/ebusd")
set(PACKAGE_PIDFILE "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/run/${PACKAGE}.pid")
set(PACKAGE_LOGFILE "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/${PACKAGE}.log")
execute_process(COMMAND echo -n ${VERSION}
                COMMAND sed "-e" "s/^\\([0-9]*\\.[0-9]*\\).*/\\1/" "-e" "s/\\.\\([0-9]\\)\$/0\\1/" "-e" "s/\\.//"
                OUTPUT_VARIABLE SCAN_VERSION)

execute_process(COMMAND git describe --always
                OUTPUT_VARIABLE REVISION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(NOT REVISION)
  execute_process(COMMAND date +p%Y%m%d
                  OUTPUT_VARIABLE REVISION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif(NOT REVISION)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_INSTALL_PREFIX "")
include(GNUInstallDirs)
include(CheckFunctionExists)
include(CheckCXXSourceRuns)
include(CheckIncludeFile)

add_definitions(-fpic -Wall -Wno-unused-function -Wextra)

check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(termios.h HAVE_TERMIOS_H)

set(CMAKE_REQUIRED_LIBRARIES pthread rt)
check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
check_function_exists(pselect HAVE_PSELECT)
check_function_exists(ppoll HAVE_PPOLL)
check_include_file(linux/serial.h HAVE_LINUX_SERIAL -DHAVE_LINUX_SERIAL=1)
check_include_file(dev/usb/uftdiio.h HAVE_FREEBSD_UFTDI -DHAVE_FREEBSD_UFTDI=1)
check_function_exists(argp_parse HAVE_ARGP)
if(NOT HAVE_ARGP)
  find_library(LIB_ARGP argp)
  if (NOT LIB_ARGP)
    message(FATAL_ERROR "argp library not available")
  endif(NOT LIB_ARGP)
  set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${LIB_ARGP}")
endif(NOT HAVE_ARGP)

option(coverage "enable code coverage tracking." OFF)
if(NOT coverage STREQUAL OFF)
  add_definitions(-g -O0 --coverage -Wall)
  link_libraries(gcov)
  message(STATUS "coverage enabled")
endif(NOT coverage STREQUAL OFF)
option(contrib "disable inclusion of contributed sources." ON)
if(contrib STREQUAL ON)
  set(HAVE_CONTRIB ON)
  message(STATUS "contrib enabled")
endif(contrib STREQUAL ON)
find_library(HAVE_MQTT mosquitto)
if(HAVE_MQTT)
  option(mqtt "disable support for MQTT handling." ON)
  if(mqtt STREQUAL ON)
    message(STATUS "MQTT enabled")
  else(mqtt STREQUAL ON)
    unset(HAVE_MQTT)
  endif(mqtt STREQUAL ON)
endif(HAVE_MQTT) 

check_cxx_source_runs("
#include <stdint.h>
int main() {
  union {
    uint32_t i;
    float f;
  } test;
  test.f = 0.15;
  return test.i == 0x3e19999a ? 0 : 1;
}
" HAVE_DIRECT_FLOAT_FORMAT)
if(NOT HAVE_DIRECT_FLOAT_FORMAT)
  check_cxx_source_runs("
#include <stdint.h>
int main() {
  union {
    uint32_t i;
    float f;
  } test;
  test.f = 0.15;
  return test.i == 0x9a99193e ? 0 : 1;
}
  " HAVE_DIRECT_FLOAT_FORMAT_REV)
  if(HAVE_DIRECT_FLOAT_FORMAT_REV)
    set(HAVE_DIRECT_FLOAT_FORMAT 2)
  endif(HAVE_DIRECT_FLOAT_FORMAT_REV)
endif(NOT HAVE_DIRECT_FLOAT_FORMAT)

add_definitions(-D_GNU_SOURCE -DHAVE_CONFIG_H -DSYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}" -DLOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${ebusd_SOURCE_DIR}/src)

if(BUILD_TESTING)
  enable_testing()
endif(BUILD_TESTING)

add_subdirectory(src/ebusd)
add_subdirectory(src/lib/utils)
add_subdirectory(src/lib/ebus)
add_subdirectory(src/tools)

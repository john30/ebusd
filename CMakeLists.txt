cmake_minimum_required(VERSION 3.7.1 FATAL_ERROR)

function(print_all_variables)
  message(STATUS "CMake Variables:")
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)
  foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
endfunction()

# CMP0075 Include file check macros honor CMAKE_REQUIRED_LIBRARIES
# For more information see: https://cmake.org/cmake/help/latest/policy/CMP0075.html
if(POLICY CMP0075)
    cmake_policy(SET CMP0075 NEW)
endif()

project(ebusd)

if(APPLE)
  add_compile_definitions(APPLE)
	file(COPY_FILE "VERSION" "VERSION_FILE")  
	file(STRINGS "VERSION_FILE" VERSION)
else()
  file(STRINGS "VERSION" VERSION)
endif()

include(GNUInstallDirs)
include(CheckFunctionExists)
include(CheckCXXSourceRuns)
include(CheckIncludeFile)

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
execute_process(COMMAND echo -n ${VERSION}
                COMMAND sed "-e" "s/^\\([0-9]*\\.[0-9]*\\).*/\\1/" "-e" "s/^\\([0-9]*\\)\\..*$/\\1/"
                OUTPUT_VARIABLE PACKAGE_VERSION_MAJOR)
execute_process(COMMAND echo -n ${VERSION}
                COMMAND sed "-e" "s/^\\([0-9]*\\.[0-9]*\\).*/\\1/" "-e" "s/^.*\\.\\([0-9]*\\)$/\\1/"
                OUTPUT_VARIABLE PACKAGE_VERSION_MINOR)

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
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "" CACHE PATH "..." FORCE)
endif()

add_definitions(-fpic -Wall -Wno-unused-function -Wextra)

set(BUILD_SHARED_LIBS OFF)

check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(syslog.h HAVE_SYSLOG_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(termios.h HAVE_TERMIOS_H)
check_function_exists(cfsetspeed HAVE_CFSETSPEED)
if(NOT APPLE)
set(CMAKE_REQUIRED_LIBRARIES pthread rt)
else()
set(CMAKE_REQUIRED_LIBRARIES pthread)
endif()

check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
check_function_exists(pselect HAVE_PSELECT)
check_function_exists(ppoll HAVE_PPOLL)
check_function_exists(timegm HAVE_TIMEGM)
check_include_file(linux/serial.h HAVE_LINUX_SERIAL -DHAVE_LINUX_SERIAL=1)
check_include_file(dev/usb/uftdiio.h HAVE_FREEBSD_UFTDI -DHAVE_FREEBSD_UFTDI=1)

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

option(knx "disable support for KNX handling." ON)
if(knx STREQUAL ON)
  message(STATUS "KNX enabled")
  set(HAVE_KNX 1)
else(knx STREQUAL ON)
  unset(HAVE_KNX)
endif(knx STREQUAL ON)

if(HAVE_KNX)
  find_library(HAVE_KNXD eibclient)
  if(HAVE_KNXD)
    option(knxd "enable support for KNX handling via knxd." OFF)
    if(knxd STREQUAL ON)
      message(STATUS "KNX via knxd enabled")
    else(knxd STREQUAL ON)
      set(HAVE_KNXD 0)
    endif(knxd STREQUAL ON)
  endif(HAVE_KNXD)
endif(HAVE_KNX)

find_library(HAVE_SSL ssl)
find_library(LIB_CRYPTO crypto)
if(HAVE_SSL)
  if(LIB_CRYPTO)
    option(ssl "disable support for SSL." ON)
    if(ssl STREQUAL ON)
      message(STATUS "SSL enabled")
      set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} ${HAVE_SSL} ${LIB_CRYPTO}")
    else(ssl STREQUAL ON)
      unset(HAVE_SSL)
    endif(ssl STREQUAL ON)
  else(LIB_CRYPTO)
    message(FATAL_ERROR "crypto library not available")
  endif(LIB_CRYPTO)
endif(HAVE_SSL)

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

add_definitions(-D_GNU_SOURCE -DHAVE_CONFIG_H)
configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${ebusd_SOURCE_DIR}/src)

if(BUILD_TESTING)
  enable_testing()
endif(BUILD_TESTING)

add_subdirectory(src/ebusd)
add_subdirectory(src/lib/utils)
add_subdirectory(src/lib/ebus)
add_subdirectory(src/lib/knx)
add_subdirectory(src/tools)

if(EXISTS "${ROOT}/etc/debian_version")
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/debian/default/ebusd DESTINATION /etc/default/)
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/debian/init.d/ebusd DESTINATION /etc/init.d/)
  install(FILES ${CMAKE_SOURCE_DIR}/contrib/debian/systemd/ebusd.service DESTINATION /lib/systemd/system/)
endif()
if(HAVE_MQTT)
  FILE(GLOB MQTT_CFG_FILES "${CMAKE_SOURCE_DIR}/contrib/etc/ebusd/mqtt-*.cfg")
  install(FILES ${MQTT_CFG_FILES} DESTINATION /etc/ebusd/)
endif(HAVE_MQTT)
if(HAVE_KNX)
  FILE(GLOB KNX_CFG_FILES "${CMAKE_SOURCE_DIR}/contrib/etc/ebusd/knx*.cfg")
  install(FILES ${KNX_CFG_FILES} DESTINATION /etc/ebusd/)
endif(HAVE_KNX)

print_all_variables()

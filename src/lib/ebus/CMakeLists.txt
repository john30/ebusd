add_definitions(-Wconversion -Wno-unused-parameter)

set(libebus_a_SOURCES
    result.cpp
    result.h
    symbol.cpp
    symbol.h
    filereader.h
    filereader.cpp
    datatype.cpp
    datatype.h
    data.cpp
    data.h
    device.cpp
    device.h
    message.cpp
    message.h
)

if(HAVE_CONTRIB)
  add_subdirectory(contrib)
endif(HAVE_CONTRIB)

add_library(ebus ${libebus_a_SOURCES})

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

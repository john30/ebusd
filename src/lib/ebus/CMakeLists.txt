add_definitions(-Wconversion -Wno-unused-parameter)

set(libebus_a_SOURCES
    result.h result.cpp
    symbol.h symbol.cpp
    filereader.h filereader.cpp
    datatype.h datatype.cpp
    data.h data.cpp
    device.h device.cpp
    message.h message.cpp
    stringhelper.h stringhelper.cpp
)

if(HAVE_CONTRIB)
  add_subdirectory(contrib)
endif(HAVE_CONTRIB)

add_library(ebus ${libebus_a_SOURCES})
target_link_libraries(ebus utils ${libebus_a_LIBS})

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

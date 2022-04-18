add_definitions(-Wno-unused-parameter)

set(libebuscontrib_a_SOURCES
    contrib.h contrib.cpp
    tem.h tem.cpp
)

include_directories(..)

add_library(ebuscontrib ${libebuscontrib_a_SOURCES})

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

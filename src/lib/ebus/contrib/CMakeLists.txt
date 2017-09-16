add_definitions(-Wno-unused-parameter)

set(libebuscontrib_a_SOURCES
    contrib.cpp
    contrib.h
    tem.cpp
    tem.h
)

include_directories(..)

add_library(ebuscontrib ${libebuscontrib_a_SOURCES})

if(BUILD_TESTING)
  add_subdirectory(test)
endif(BUILD_TESTING)

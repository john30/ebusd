add_compile_options(-Wno-unused-parameter)

set(libebuscontrib_a_SOURCES
    contrib.cpp
    contrib.h
    tem.cpp
    tem.h
)

include_directories(..)

add_library(ebuscontrib ${libebuscontrib_a_SOURCES})

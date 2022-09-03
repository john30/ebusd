add_definitions(-Wconversion -Wno-unused-parameter)

set(libknx_a_SOURCES
    knx.h knx.cpp
)

set(libknx_a_LIBS
)

if(HAVE_KNXD)
    set(libknx_a_SOURCES ${libknx_a_SOURCES} knxd.h)
    set(libknx_a_LIBS ${libknx_a_LIBS} eibclient)
endif(HAVE_KNXD)

add_library(knx ${libknx_a_SOURCES})

target_link_libraries(knx ${libknx_a_LIBS})
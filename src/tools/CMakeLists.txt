set(ebusctl_SOURCES ebusctl.cpp)
set(ebusfeed_SOURCES ebusfeed.cpp)

if(HAVE_CONTRIB)
  set(ebusfeed_LIBS ${ebusfeed_LIBS} ebuscontrib)
endif(HAVE_CONTRIB)

include_directories(../lib/ebus)
include_directories(../lib/utils)

add_executable(ebusctl ${ebusctl_SOURCES})
add_executable(ebusfeed ${ebusfeed_SOURCES})
target_link_libraries(ebusctl utils ebus ${LIB_ARGP} ${ebusctl_LIBS})
target_link_libraries(ebusfeed ebus ${LIB_ARGP} ${ebusfeed_LIBS})

install(TARGETS ebusctl EXPORT ebusd DESTINATION usr/bin)


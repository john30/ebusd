set(ebusctl_SOURCES ebusctl.cpp)
set(ebusfeed_SOURCES ebusfeed.cpp)
set(ebuspicloader_SOURCES ebuspicloader.cpp intelhex/intelhexclass.cpp)

if(HAVE_CONTRIB)
  set(ebusfeed_LIBS ${ebusfeed_LIBS} ebuscontrib)
endif(HAVE_CONTRIB)

include_directories(../lib/ebus)
include_directories(../lib/utils)
include_directories(intelhex)

add_executable(ebusctl ${ebusctl_SOURCES})
add_executable(ebusfeed ${ebusfeed_SOURCES})
add_executable(ebuspicloader ${ebuspicloader_SOURCES})
target_link_libraries(ebusctl utils ebus ${LIB_ARGP} ${ebusctl_LIBS})
target_link_libraries(ebusfeed ebus ${LIB_ARGP} ${ebusfeed_LIBS})
target_link_libraries(ebuspicloader ${LIB_ARGP})

install(TARGETS ebusctl ebuspicloader EXPORT ebusd DESTINATION usr/bin)

set(ebusctl_SOURCES ebusctl.cpp)
set(ebuspicloader_SOURCES ebuspicloader.cpp intelhex/intelhexclass.cpp)

include_directories(../lib/ebus)
include_directories(../lib/utils)
include_directories(intelhex)

add_executable(ebusctl ${ebusctl_SOURCES})
add_executable(ebuspicloader ${ebuspicloader_SOURCES})
target_link_libraries(ebusctl utils ebus ${LIB_ARGP} ${ebusctl_LIBS})
target_link_libraries(ebuspicloader utils ${LIB_ARGP} ${ebuspicloader_LIBS})

if(WITH_EBUSFEED)
  set(ebusfeed_SOURCES ebusfeed.cpp)
  add_executable(ebusfeed ${ebusfeed_SOURCES})
  target_link_libraries(ebusfeed ebus ${LIB_ARGP} ${ebusfeed_LIBS})
endif(WITH_EBUSFEED)

install(TARGETS ebusctl ebuspicloader EXPORT ebusd DESTINATION usr/bin)

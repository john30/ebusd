add_definitions(-Wno-unused-parameter)

set(ebusctl_SOURCES ebusctl.cpp)
set(ebuspicloader_SOURCES ebuspicloader.cpp intelhex/intelhexclass.cpp)

include_directories(../lib/ebus)
include_directories(../lib/utils)
include_directories(intelhex)

add_executable(ebusctl ${ebusctl_SOURCES})
add_executable(ebuspicloader ${ebuspicloader_SOURCES})
target_link_libraries(ebusctl utils ebus ${ebusctl_LIBS})
target_link_libraries(ebuspicloader utils ${ebuspicloader_LIBS})

install(TARGETS ebusctl ebuspicloader EXPORT ebusd DESTINATION usr/bin)

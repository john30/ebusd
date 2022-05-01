add_definitions(-Wconversion -Wno-unused-parameter)

set(ebusd_SOURCES
    bushandler.h bushandler.cpp
    datahandler.h datahandler.cpp
    network.h network.cpp
    mainloop.h mainloop.cpp
    main.h main.cpp
)

if(HAVE_MQTT)
  set(ebusd_SOURCES ${ebusd_SOURCES} mqtthandler.cpp mqtthandler.h)
  set(ebusd_LIBS ${ebusd_LIBS} mosquitto)
endif(HAVE_MQTT)

if(HAVE_KNX)
  set(ebusd_SOURCES ${ebusd_SOURCES} knxhandler.cpp knxhandler.h)
  set(ebusd_LIBS ${ebusd_LIBS} eibclient)
endif(HAVE_KNX)

if(HAVE_SSL)
  set(ebusd_LIBS ${ebusd_LIBS} ssl crypto)
endif(HAVE_SSL)

if(HAVE_CONTRIB)
  set(ebusd_LIBS ${ebusd_LIBS} ebuscontrib)
endif(HAVE_CONTRIB)

include_directories(../lib/ebus)
include_directories(../lib/utils)

add_executable(ebusd ${ebusd_SOURCES})
target_link_libraries(ebusd utils ebus pthread rt ${LIB_ARGP} ${ebusd_LIBS})

install(TARGETS ebusd EXPORT ebusd DESTINATION usr/bin)


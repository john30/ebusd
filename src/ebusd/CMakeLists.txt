add_compile_options(-Wconversion -Wno-unused-parameter)

set(ebusd_SOURCES
    bushandler.cpp
    bushandler.h
    datahandler.h
    datahandler.cpp
    network.cpp
    network.h
    mainloop.cpp
    mainloop.h
    main.h
    main.cpp
)

if(HAVE_MQTT)
  set(ebusd_SOURCES ${ebusd_SOURCES} mqtthandler.cpp mqtthandler.h)
  set(ebusd_LIBS ${ebusd_LIBS} mosquitto)
endif(HAVE_MQTT)

if(HAVE_CONTRIB)
  set(ebusd_LIBS ${ebusd_LIBS} ebuscontrib)
endif(HAVE_CONTRIB)

include_directories(../lib/ebus)
include_directories(../lib/utils)

add_executable(ebusd ${ebusd_SOURCES})
target_link_libraries(ebusd utils ebus pthread rt ${ebusd_LIBS})

add_definitions(-Wconversion -Wno-unused-parameter)

set(ebusd_SOURCES
    bushandler.h bushandler.cpp
    datahandler.h datahandler.cpp
    request.h request.cpp
    network.h network.cpp
    mainloop.h mainloop.cpp
    scan.h scan.cpp
    main.h main.cpp main_args.cpp
)

if(HAVE_MQTT)
  set(ebusd_SOURCES ${ebusd_SOURCES} mqtthandler.cpp mqtthandler.h mqttclient.cpp mqttclient.h)
  set(ebusd_SOURCES ${ebusd_SOURCES} mqttclient_mosquitto.cpp mqttclient_mosquitto.h)
  set(ebusd_LIBS ${ebusd_LIBS} mosquitto)
endif(HAVE_MQTT)

if(HAVE_KNX)
  set(ebusd_SOURCES ${ebusd_SOURCES} knxhandler.cpp knxhandler.h)
  set(ebusd_LIBS ${ebusd_LIBS} knx)
  include_directories(../lib/knx)
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
if(NOT APPLE)
target_link_libraries(ebusd utils ebus pthread rt ${ebusd_LIBS})
else()
target_link_libraries(ebusd utils ebus pthread ${ebusd_LIBS})
endif()
install(TARGETS ebusd EXPORT ebusd DESTINATION bin)
if(APPLE)
    add_custom_command(TARGET ebusd POST_BUILD COMMAND rm ${CMAKE_SOURCE_DIR}/VERSION_FILE)  
endif()

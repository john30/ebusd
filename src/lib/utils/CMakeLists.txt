add_definitions(-Wconversion)

set(libutils_a_SOURCES
    log.h log.cpp
    tcpsocket.h tcpsocket.cpp
    thread.h thread.cpp
    clock.h clock.cpp
    queue.h
    notify.h
    rotatefile.h rotatefile.cpp
    httpclient.h httpclient.cpp)

add_library(utils ${libutils_a_SOURCES})

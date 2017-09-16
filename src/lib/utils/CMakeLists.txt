add_definitions(-Wconversion)

set(libutils_a_SOURCES
    log.cpp
    log.h
    tcpsocket.cpp
    tcpsocket.h
    thread.cpp
    thread.h
    clock.cpp
    clock.h
    queue.h
    notify.h
    rotatefile.cpp
    rotatefile.h
)

add_library(utils ${libutils_a_SOURCES})

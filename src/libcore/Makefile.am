AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra

noinst_LIBRARIES = libcore.a

libcore_a_SOURCES = wqueue.h \
		    notify.h \
		    appl.cpp \
		    appl.h \
		    daemon.cpp \
		    daemon.h \
		    logger.cpp \
		    logger.h \
		    thread.cpp \
		    thread.h \
		    tcpsocket.cpp \
		    tcpsocket.h

distclean-local:
	-rm -f Makefile.in

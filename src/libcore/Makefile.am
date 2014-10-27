AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra

noinst_LIBRARIES = libcore.a

libcore_a_SOURCES = appl.cpp \
		    daemon.cpp \
		    logger.cpp \
		    thread.cpp \
		    tcpsocket.cpp

distclean-local:
	-rm -f Makefile.in

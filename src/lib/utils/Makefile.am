AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = appl.cpp \
		     appl.h \
		     daemon.cpp \
		     daemon.h \
		     logger.cpp \
		     logger.h \
		     tcpsocket.cpp \
		     tcpsocket.h \
		     thread.cpp \
		     thread.h \
		     wqueue.h \
		     notify.h

distclean-local:
	-rm -f Makefile.in

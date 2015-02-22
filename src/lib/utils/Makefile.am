AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wconversion

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = log.cpp \
		     log.h \
		     tcpsocket.cpp \
		     tcpsocket.h \
		     thread.cpp \
		     thread.h \
		     wqueue.h \
		     notify.h

distclean-local:
	-rm -f Makefile.in

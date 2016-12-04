AM_CXXFLAGS = -Wconversion

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = log.cpp \
		     log.h \
		     tcpsocket.cpp \
		     tcpsocket.h \
		     thread.cpp \
		     thread.h \
		     clock.h \
		     clock.cpp \
		     queue.h \
		     notify.h \
		     rotatefile.h \
		     rotatefile.cpp

distclean-local:
	-rm -f Makefile.in

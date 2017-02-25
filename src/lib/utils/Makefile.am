AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wconversion

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = log.cpp \
		     log.h \
		     tcpsocket.cpp \
		     tcpsocket.h \
		     thread.cpp \
		     thread.h \
		     clock.cpp \
		     clock.h \
		     queue.h \
		     notify.h \
		     rotatefile.cpp \
		     rotatefile.h

distclean-local:
	-rm -f Makefile.in

AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wconversion

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = \
		     arg.h arg.cpp \
		     log.h log.cpp \
		     tcpsocket.h tcpsocket.cpp \
		     thread.h thread.cpp \
		     clock.h clock.cpp \
		     queue.h \
		     notify.h \
		     rotatefile.h rotatefile.cpp \
		     httpclient.h httpclient.cpp

distclean-local:
	-rm -f Makefile.in

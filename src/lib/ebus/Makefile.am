AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -isystem$(top_srcdir)/src/lib/utils

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = result.cpp \
		    result.h \
		    symbol.cpp \
		    symbol.h \
		    data.cpp \
		    data.h \
		    port.cpp \
		    port.h \
		    message.cpp \
		    message.h

distclean-local:
	-rm -f Makefile.in

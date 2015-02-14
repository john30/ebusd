AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -Wno-unused-parameter

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = result.cpp \
		    result.h \
		    symbol.cpp \
		    symbol.h \
		    filereader.h \
		    data.cpp \
		    data.h \
		    device.cpp \
		    device.h \
		    message.cpp \
		    message.h

distclean-local:
	-rm -f Makefile.in

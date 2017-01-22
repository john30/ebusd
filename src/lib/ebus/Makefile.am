AM_CXXFLAGS = -Wconversion -Wno-unused-parameter

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = result.cpp \
		    result.h \
		    symbol.cpp \
		    symbol.h \
		    filereader.h \
		    datatype.cpp \
		    datatype.h \
		    data.cpp \
		    data.h \
		    device.cpp \
		    device.h \
		    message.cpp \
		    message.h

if CONTRIB
SUBDIRS = contrib
endif

distclean-local:
	-rm -f Makefile.in

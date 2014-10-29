AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = result.cpp \
		    result.h \
		    symbol.cpp \
		    symbol.h \
		    data.cpp \
		    data.h \
		    port.cpp \
		    port.h \
		    buscommand.cpp \
		    buscommand.h \
		    bus.cpp \
		    bus.h \
		    command.cpp \
		    command.h \
		    commands.cpp \
		    commands.h \
		    configfile.cpp \
		    configfile.h \
		    dump.cpp \
		    dump.h \
		    decode.cpp \
		    decode.h \
		    encode.cpp \
		    encode.h

distclean-local:
	-rm -f Makefile.in

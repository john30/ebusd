AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra

noinst_LIBRARIES = libebus.a

libebus_a_SOURCES = result.cpp \
		    symbol.cpp \
		    port.cpp \
		    bus.cpp \
		    command.cpp \
		    commands.cpp \
		    configfile.cpp \
		    dump.cpp \
		    decode.cpp \
		    encode.cpp

distclean-local:
	-rm -f Makefile.in

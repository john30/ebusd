AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/lib/ebus

noinst_PROGRAMS = test_port \
		  test_symbol \
		  test_data \
		  test_bus \
		  test_commands \
		  test_configfile \
		  test_decode \
		  test_encode

test_port_SOURCES = test_port.cpp
test_port_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_symbol_SOURCES = test_symbol.cpp
test_symbol_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_data_SOURCES = test_data.cpp
test_data_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_bus_SOURCES = test_bus.cpp
test_bus_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_commands_SOURCES = test_commands.cpp
test_commands_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_configfile_SOURCES = test_configfile.cpp
test_configfile_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_decode_SOURCES = test_decode.cpp
test_decode_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_encode_SOURCES = test_encode.cpp
test_encode_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

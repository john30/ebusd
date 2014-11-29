AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/lib/ebus \
	      -I$(top_srcdir)/src/lib/utils

noinst_PROGRAMS = test_port \
		  test_symbol \
		  test_data \
		  test_message

test_port_SOURCES = test_port.cpp
test_port_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
		  $(top_srcdir)/src/lib/ebus/libebus.a

test_symbol_SOURCES = test_symbol.cpp
test_symbol_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_data_SOURCES = test_data.cpp
test_data_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

test_message_SOURCES = test_message.cpp
test_message_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

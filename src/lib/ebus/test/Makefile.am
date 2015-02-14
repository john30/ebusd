AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -isystem$(top_srcdir)/src/lib/ebus

noinst_PROGRAMS = test_device \
		  test_symbol \
		  test_data \
		  test_message

test_device_SOURCES = test_device.cpp
test_device_LDADD = ../libebus.a

test_symbol_SOURCES = test_symbol.cpp
test_symbol_LDADD = ../libebus.a

test_data_SOURCES = test_data.cpp
test_data_LDADD = ../libebus.a

test_message_SOURCES = test_message.cpp
test_message_LDADD = ../libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

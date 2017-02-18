AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -Wno-unused-parameter

noinst_PROGRAMS = test_filereader \
		  test_device \
		  test_symbol \
		  test_data \
		  test_message

test_filereader_SOURCES = test_filereader.cpp
test_filereader_LDADD = ../libebus.a

test_device_SOURCES = test_device.cpp
test_device_LDADD = ../libebus.a

test_symbol_SOURCES = test_symbol.cpp
test_symbol_LDADD = ../libebus.a

test_data_SOURCES = test_data.cpp
test_data_LDADD = ../libebus.a

test_message_SOURCES = test_message.cpp
test_message_LDADD = ../libebus.a

if CONTRIB
test_device_LDADD += ../contrib/libebuscontrib.a
test_data_LDADD += ../contrib/libebuscontrib.a
test_message_LDADD += ../contrib/libebuscontrib.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

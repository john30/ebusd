AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wno-unused-parameter

noinst_PROGRAMS = test_filereader \
		  test_symbol \
		  test_data \
		  test_message

test_filereader_SOURCES = test_filereader.cpp
test_filereader_LDADD = ../libebus.a -lpthread

test_symbol_SOURCES = test_symbol.cpp
test_symbol_LDADD = ../libebus.a -lpthread

test_data_SOURCES = test_data.cpp
test_data_LDADD = -lpthread ../libebus.a

test_message_SOURCES = test_message.cpp
test_message_LDADD = ../libebus.a -lpthread

if CONTRIB
test_data_LDADD += ../contrib/libebuscontrib.a
test_message_LDADD += ../contrib/libebuscontrib.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

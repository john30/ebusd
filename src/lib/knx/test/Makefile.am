AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wno-unused-parameter

noinst_PROGRAMS = test_knxhandler

test_knxhandler_SOURCES = test_knxhandler.cpp
test_knxhandler_LDADD = ../../ebus/libebus.a ../../knx/libknx.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

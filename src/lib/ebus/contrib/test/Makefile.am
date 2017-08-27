AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wno-unused-parameter

noinst_PROGRAMS = test_contrib

test_contrib_SOURCES = test_tem.cpp
test_contrib_LDADD = ../../libebus.a ../libebuscontrib.a -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

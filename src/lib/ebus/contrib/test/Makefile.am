AM_CXXFLAGS = -I$(top_srcdir)/src \
	-Wno-unused-parameter

noinst_PROGRAMS = test_contrib

test_contrib_SOURCES = test_tem.cpp
test_contrib_LDADD = ../../libebus.a ../libebuscontrib.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

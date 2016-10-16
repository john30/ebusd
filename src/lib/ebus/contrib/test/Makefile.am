AM_CXXFLAGS = -Wno-unused-parameter \
	-isystem$(top_srcdir)/src/lib/ebus -isystem../

noinst_PROGRAMS = test_contrib

test_contrib_SOURCES = test_tem.cpp
test_contrib_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

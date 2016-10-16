AM_CXXFLAGS = -Wno-unused-parameter \
	-isystem$(top_srcdir)/src/lib/ebus -isystem../

noinst_PROGRAMS = test_tem

test_tem_SOURCES = test_tem.cpp
test_tem_LDADD = $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

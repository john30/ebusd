AM_CXXFLAGS = -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusctl \
	       ebusfeed

ebusctl_SOURCES = ebusctl.cpp
ebusctl_LDADD = ../lib/utils/libutils.a

ebusfeed_SOURCES = ebusfeed.cpp
ebusfeed_LDADD = ../lib/utils/libutils.a \
	         ../lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


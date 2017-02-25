AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir)

bin_PROGRAMS = ebusctl \
	       ebusfeed

ebusctl_SOURCES = ebusctl.cpp
ebusctl_LDADD = ../lib/utils/libutils.a

ebusfeed_SOURCES = ebusfeed.cpp
ebusfeed_LDADD = ../lib/utils/libutils.a \
	         ../lib/ebus/libebus.a

if CONTRIB
ebusfeed_LDADD += ../lib/ebus/contrib/libebuscontrib.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

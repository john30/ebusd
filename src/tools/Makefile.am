AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir)

bin_PROGRAMS = ebusctl \
	       ebuspicloader \
               ebusfeed

ebusctl_SOURCES = ebusctl.cpp
ebusctl_LDADD = ../lib/utils/libutils.a

ebuspicloader_SOURCES = ebuspicloader.cpp intelhex/intelhexclass.cpp

ebusfeed_SOURCES = ebusfeed.cpp
ebusfeed_LDADD = ../lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

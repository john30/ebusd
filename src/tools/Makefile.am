AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir)

bin_PROGRAMS = ebusctl \
	       ebuspicloader

ebusctl_SOURCES = ebusctl.cpp
ebusctl_LDADD = ../lib/utils/libutils.a

ebuspicloader_SOURCES = ebuspicloader.cpp intelhex/intelhexclass.cpp
ebuspicloader_LDADD = ../lib/utils/libutils.a

if WITH_EBUSFEED
bin_PROGRAMS += ebusfeed
ebusfeed_SOURCES = ebusfeed.cpp
ebusfeed_LDADD = ../lib/utils/libutils.a \
	         ../lib/ebus/libebus.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusctl \
	       ebusfeed

ebusctl_SOURCES = ebusctl.cpp

ebusctl_LDADD = $(top_srcdir)/src/lib/utils/libutils.a

ebusfeed_SOURCES = ebusfeed.cpp

ebusfeed_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
	         $(top_srcdir)/src/lib/ebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


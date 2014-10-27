AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/libcore \
	      -I$(top_srcdir)/src/libebus

bin_PROGRAMS = ebusctl

ebusctl_SOURCES = ebusctl.cpp

ebusctl_LDADD = $(top_srcdir)/src/libcore/libcore.a \
	        $(top_srcdir)/src/libebus/libebus.a

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


AM_CXXFLAGS = -Wno-unused-parameter \
	-isystem$(top_srcdir)/src/lib/ebus

noinst_LIBRARIES = libebuscontrib.a

libebuscontrib_a_SOURCES = contrib.h contrib.cpp tem.h tem.cpp

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

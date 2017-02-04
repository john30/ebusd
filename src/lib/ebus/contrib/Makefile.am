AM_CXXFLAGS = -I$(top_srcdir)/src \
	-Wno-unused-parameter

noinst_LIBRARIES = libebuscontrib.a

libebuscontrib_a_SOURCES = contrib.h contrib.cpp tem.h tem.cpp

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

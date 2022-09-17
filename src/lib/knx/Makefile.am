AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wno-conversion

noinst_LIBRARIES = libknx.a

libknx_a_SOURCES = \
		     knx.h knx.cpp

if KNXD
libknx_a_SOURCES += knxd.h
else
libknx_a_SOURCES += knxnet.h
endif

distclean-local:
	-rm -f Makefile.in

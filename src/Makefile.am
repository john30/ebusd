bin_PROGRAMS = ebusd

ebusd_SOURCES = ebusd.c ebusd.h utils.c utils.h
ebusd_LDADD = $(top_srcdir)/lib/libebus.a $(top_srcdir)/lib/liblog.a
ebusd_CFLAGS = -I$(top_srcdir)/lib

distclean-local:
	-rm -f Makefile.in
	

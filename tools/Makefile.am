bin_PROGRAMS = ebus_send check csv

ebus_send_SOURCES = ebus_send.c
ebus_send_LDADD = $(top_srcdir)/lib/libebus.a $(top_srcdir)/lib/liblog.a
ebus_send_CFLAGS = -I$(top_srcdir)/lib

check_SOURCES = check.c
check_LDADD = $(top_srcdir)/lib/libebus.a $(top_srcdir)/lib/liblog.a
check_CFLAGS = -I$(top_srcdir)/lib

csv_SOURCES = csv.c
csv_LDADD = $(top_srcdir)/lib/libebus.a $(top_srcdir)/lib/liblog.a
csv_CFLAGS = -I$(top_srcdir)/lib

distclean-local:
	-rm -f Makefile.in
	

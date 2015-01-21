AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusd

ebusd_SOURCES = bushandler.cpp \
		bushandler.h \
		network.cpp \
		network.h \
		baseloop.cpp \
		baseloop.h \
		ebusd.cpp

ebusd_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
              $(top_srcdir)/src/lib/ebus/libebus.a \
	      -lpthread \
	      -lrt

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

install-data-local:
	[ -d $(DESTDIR)$(sysconfdir)/ebusd ] || mkdir -p $(DESTDIR)$(sysconfdir)/ebusd

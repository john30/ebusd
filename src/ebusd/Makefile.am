AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/lib/utils \
	      -I$(top_srcdir)/src/lib/ebus

bin_PROGRAMS = ebusd

ebusd_SOURCES = connection.cpp \
	        connection.h \
		network.cpp \
		network.h \
		ebusloop.cpp \
		ebusloop.h \
		baseloop.cpp \
		baseloop.h \
		netmessage.h \
		ebusd.cpp

ebusd_LDADD = $(top_srcdir)/src/lib/utils/libutils.a \
              $(top_srcdir)/src/lib/ebus/libebus.a \
	      -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


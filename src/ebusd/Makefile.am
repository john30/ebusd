AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/libcore \
	      -I$(top_srcdir)/src/libebus

bin_PROGRAMS = ebusd

ebusd_SOURCES = connection.cpp \
		network.cpp \
		ebusloop.cpp \
		baseloop.cpp \
		main.cpp

ebusd_LDADD = $(top_srcdir)/src/libcore/libcore.a \
              $(top_srcdir)/src/libebus/libebus.a \
	      -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


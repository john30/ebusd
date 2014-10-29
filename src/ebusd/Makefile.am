AM_CXXFLAGS = -fpic \
	      -Wall \
	      -Wextra \
	      -I$(top_srcdir)/src/libcore \
	      -I$(top_srcdir)/src/libebus

bin_PROGRAMS = ebusd

ebusd_SOURCES = message.h \
	        connection.cpp \
	        connection.h \
		network.cpp \
		network.h \
		ebusloop.cpp \
		ebusloop.h \
		baseloop.cpp \
		baseloop.h \
		main.cpp

ebusd_LDADD = $(top_srcdir)/src/libcore/libcore.a \
              $(top_srcdir)/src/libebus/libebus.a \
	      -lpthread

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs


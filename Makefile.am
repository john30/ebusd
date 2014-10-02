AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(LIBEBUS_CFLAGS) -I$(top_srcdir)/lib -fpic -Wall -Wextra

bin_PROGRAMS = ebusd \
	       ebusctl

ebusd_SOURCES = lib/appl.cpp \
		lib/daemon.cpp \
		lib/tcpsocket.cpp \
		lib/thread.cpp \
		lib/logger.cpp \
		src/connection.cpp \
		src/network.cpp \
		src/ebusloop.cpp \
		src/cycdata.cpp \
		src/baseloop.cpp \
		src/main.cpp

ebusd_LDADD = $(LIBEBUS_LIBS) \
	      -lpthread

dist_noinst_SCRIPTS = autogen.sh


ebusctl_SOURCES = lib/appl.cpp \
		  lib/tcpsocket.cpp \
		  tools/ebusctl.cpp

ebusctl_LDADD = $(LIBEBUS_LIBS)


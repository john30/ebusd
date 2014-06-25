AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(LIBEBUS_CFLAGS) -I$(top_srcdir)/lib -fpic -Wall -Wextra

bin_PROGRAMS = ebusd \
	       ebusctl \
	       ebusdump

ebusd_SOURCES = src/main.cpp \
	        src/baseloop.cpp \
		src/ebusloop.cpp \
		src/cycdata.cpp \
		src/network.cpp \
		lib/thread.cpp \
		lib/logger.cpp \
		lib/tcpsocket.cpp \
		lib/daemon.cpp \
		lib/appl.cpp

ebusd_LDADD = $(LIBEBUS_LIBS) \
	      -lpthread

dist_noinst_SCRIPTS = autogen.sh


ebusctl_SOURCES = tools/ebusctl.cpp \
		  lib/appl.cpp \
		  lib/tcpsocket.cpp

ebusctl_LDADD = $(LIBEBUS_LIBS)


ebusdump_SOURCES = tools/ebusdump.cpp \
		   lib/appl.cpp

ebusdump_LDADD = $(LIBEBUS_LIBS)


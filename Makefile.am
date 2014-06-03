AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(LIBEBUS_CFLAGS) -I$(top_srcdir)/lib -fpic -Wall -Wextra

bin_PROGRAMS = ebusd \
	       ebusd_feed \
	       ebusd_send

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

ebusd_feed_SOURCES = tool/ebusd_feed.cpp \
		     lib/appl.cpp

ebusd_feed_LDADD = $(LIBEBUS_LIBS)

ebusd_send_SOURCES = tool/ebusd_send.cpp \
		     lib/appl.cpp \
		     lib/tcpsocket.cpp

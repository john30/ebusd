AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(LIBEBUS_CFLAGS) -I$(top_srcdir)/utils -fpic -Wall -Wextra

bin_PROGRAMS = ebus-daemon

ebus_daemon_SOURCES = src/main.cpp \
		      src/baseloop.cpp \
		      src/ebusloop.cpp \
		      src/cycdata.cpp \
		      src/network.cpp \
		      utils/thread.cpp \
		      utils/logger.cpp \
		      utils/tcpsocket.cpp \
		      utils/daemon.cpp \
		      utils/appl.cpp

ebus_daemon_LDADD = $(LIBEBUS_LIBS) \
		    -lpthread

dist_noinst_SCRIPTS = autogen.sh

noinst_PROGRAMS = feed

feed_SOURCES = test/feed.cpp \
	       utils/appl.cpp

feed_LDADD = $(LIBEBUS_LIBS)

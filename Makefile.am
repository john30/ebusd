AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(LIBEBUS_CFLAGS) -I$(top_srcdir)/utils -fpic -Wall -Wextra

bin_PROGRAMS = ebusd ebusd_feed

ebusd_SOURCES = src/main.cpp \
	        src/baseloop.cpp \
		src/ebusloop.cpp \
		src/cycdata.cpp \
		src/network.cpp \
		utils/thread.cpp \
		utils/logger.cpp \
		utils/tcpsocket.cpp \
		utils/daemon.cpp \
		utils/appl.cpp

ebusd_LDADD = $(LIBEBUS_LIBS) \
	      -lpthread

dist_noinst_SCRIPTS = autogen.sh

ebusd_feed_SOURCES = tools/ebusd_feed.cpp \
		     utils/appl.cpp

ebusd_feed_LDADD = $(LIBEBUS_LIBS)

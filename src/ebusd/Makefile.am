AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wconversion -Wno-unused-parameter \
	      -DSYSCONFDIR=\"$(sysconfdir)\" \
	      -DLOCALSTATEDIR=\"$(localstatedir)\"

bin_PROGRAMS = ebusd

ebusd_SOURCES = \
		bushandler.h bushandler.cpp \
		datahandler.h datahandler.cpp \
		network.h network.cpp \
		mainloop.h mainloop.cpp \
		main.h main.cpp

if MQTT
ebusd_SOURCES += mqtthandler.cpp mqtthandler.h
endif
if KNX
ebusd_SOURCES += knxhandler.cpp knxhandler.h
endif

ebusd_LDADD = ../lib/utils/libutils.a \
              ../lib/ebus/libebus.a \
	      -lpthread \
	      @EXTRA_LIBS@

if SSL
ebusd_LDADD += -lssl -lcrypto
endif

if CONTRIB
ebusd_LDADD += ../lib/ebus/contrib/libebuscontrib.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

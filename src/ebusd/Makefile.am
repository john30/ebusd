AM_CXXFLAGS = -I$(top_srcdir)/src \
	      -isystem$(top_srcdir) \
	      -Wconversion -Wno-unused-parameter

bin_PROGRAMS = ebusd

ebusd_SOURCES = \
		bushandler.h bushandler.cpp \
		datahandler.h datahandler.cpp \
		request.h request.cpp \
		network.h network.cpp \
		mainloop.h mainloop.cpp \
		scan.h scan.cpp \
		main.h main.cpp main_args.cpp


ebusd_LDADD = ../lib/ebus/libebus.a \
              ../lib/utils/libutils.a \
	      -lpthread \
	      @EXTRA_LIBS@

if MQTT
ebusd_SOURCES += mqtthandler.cpp mqtthandler.h mqttclient.cpp mqttclient.h
ebusd_SOURCES += mqttclient_mosquitto.cpp mqttclient_mosquitto.h
ebusd_LDADD += -lmosquitto
endif

if KNX
ebusd_SOURCES += knxhandler.cpp knxhandler.h
ebusd_LDADD += ../lib/knx/libknx.a
if KNXD
ebusd_LDADD += -leibclient
endif
endif

if SSL
ebusd_LDADD += -lssl -lcrypto
endif

if CONTRIB
ebusd_LDADD += ../lib/ebus/contrib/libebuscontrib.a
endif

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

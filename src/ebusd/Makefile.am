AM_CXXFLAGS = -Wconversion -Wunused-parameter \
	      -isystem$(top_srcdir)/src/lib/utils \
	      -isystem$(top_srcdir)/src/lib/ebus \
	      -DSYSCONFDIR=\"$(sysconfdir)\" \
	      -DLOCALSTATEDIR=\"$(localstatedir)\"

bin_PROGRAMS = ebusd

ebusd_SOURCES = bushandler.cpp \
		bushandler.h \
		datahandler.cpp \
		datahandler.h \
		network.cpp \
		network.h \
		mainloop.cpp \
		mainloop.h \
		main.h \
		main.cpp

if MQTT
ebusd_SOURCES += mqtthandler.cpp mqtthandler.h
endif

ebusd_LDADD = ../lib/utils/libutils.a \
              ../lib/ebus/libebus.a \
	      -lpthread \
	      @EXTRA_LIBS@

distclean-local:
	-rm -f Makefile.in
	-rm -rf .libs

install-data-local:
	[ -d $(DESTDIR)$(sysconfdir)/ebusd ] || mkdir -p $(DESTDIR)$(sysconfdir)/ebusd

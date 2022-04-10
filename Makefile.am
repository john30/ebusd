ACLOCAL_AMFLAGS = -I m4

SUBDIRS = docs \
	src/lib/utils \
	src/lib/ebus \
	src/ebusd \
	src/tools

EXTRA_DIST = LICENSE \
	ChangeLog.md \
	README.md \
	VERSION

if MQTT
MQTT_INST_ADD = mkdir -p $(DESTDIR)/etc/ebusd/; \
	cp -n $(srcdir)/contrib/etc/ebusd/mqtt-*.cfg $(DESTDIR)/etc/ebusd
endif

install-data-hook:
	@if [ -f "/etc/debian_version" ]; then \
	  mkdir -p $(DESTDIR)/etc/default/ $(DESTDIR)/etc/init.d/ $(DESTDIR)/lib/systemd/system/; \
	  cp -n $(srcdir)/contrib/debian/default/ebusd $(DESTDIR)/etc/default/; \
	  cp $(srcdir)/contrib/debian/init.d/ebusd $(DESTDIR)/etc/init.d/; \
	  cp $(srcdir)/contrib/debian/systemd/ebusd.service $(DESTDIR)/lib/systemd/system/; \
	fi \
	$(MQTT_INST_ADD)

test:
	$(MAKE) -C src/lib/ebus/test
if CONTRIB
	$(MAKE) -C src/lib/ebus/contrib/test
endif

distclean-local:
	-rm -rf autom4te.cache
	-rm -f aclocal.m4
	-rm -f config.h.in
	-rm -f configure
	-rm -f Makefile.in
	-rm -rf build

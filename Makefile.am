SUBDIRS = docs \
	  src/lib/utils \
	  src/lib/ebus \
	  src/ebusd \
	  src/tools

EXTRA_DIST = LICENSE \
	     ChangeLog.md \
	     README.md \
	     VERSION

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

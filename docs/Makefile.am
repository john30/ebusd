if HAVE_DOXYGEN

doxyfile.stamp:
	$(HAVE_DOXYGEN) Doxyfile
	echo Timestamp > doxyfile.stamp

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp

distclean-local:
	-rm -f Makefile.in
	-rm -rf html

endif

AC_PREREQ([2.63])
AC_INIT([ebusd], m4_esyscmd_s([cat VERSION]), [ebusd@ebusd.eu], [ebusd], [https://github.com/john30/ebusd])

AC_CONFIG_AUX_DIR([build])

if test -z $CXXFLAGS; then
	CXXFLAGS='-g -O2'
fi
AC_PROG_CXX

AC_CHECK_HEADERS([arpa/inet.h \
		  dirent.h \
		  fcntl.h \
		  netdb.h \
		  poll.h \
		  pthread.h \
		  sys/ioctl.h \
		  sys/select.h \
		  sys/time.h \
		  time.h \
		  termios.h])

AC_CHECK_LIB([pthread], [pthread_setname_np],
	AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], [Define to 1 if pthread has pthread_setname_np.]),
	AC_MSG_RESULT([Could not find pthread_setname_np in pthread.]))
RT_LIB=
AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"])
AC_SUBST(RT_LIB)
AC_CHECK_FUNC([pselect], [AC_DEFINE(HAVE_PSELECT, [1], [Define to 1 if pselect() is available.])])
AC_CHECK_FUNC([ppoll], [AC_DEFINE(HAVE_PPOLL, [1], [Define to 1 if ppoll() is available.])])

AC_ARG_WITH(argp-lib, AS_HELP_STRING([--with-argp-lib=PATH], [path to argp libraries]), [LDFLAGS+="-L$with_argp_lib"])
AC_ARG_WITH(argp-include, AS_HELP_STRING([--with-argp-include=PATH], [path to argp includes]), [CPPFLAGS+="-I$with_argp_include"])
AC_CHECK_FUNC([argp_parse], [have_argp=yes], AC_CHECK_LIB([argp], [argp_parse], [have_argp=yes; LIBS="-largp $LIBS"], [have_argp=no]))
if test "x$have_argp" = "xyes"; then
	AC_CHECK_HEADER([argp.h], AC_DEFINE([HAVE_ARGP_H], [1], [Define to 1 if argp.h is available.]), AC_MSG_ERROR([argp.h not found]))
else
	AC_MSG_ERROR([argp library not found, specify argp-standalone location in --with-argp-lib= and --with-argp-include= options.])
fi

AC_MSG_CHECKING([for direct float format conversion])
AC_TRY_RUN(
	[
#include <stdint.h>
int main() {
	union {
		uint32_t i;
		float f;
	} test;
	test.f = 0.15;
	return test.i == 0x3e19999a ? 0 : 1;
}
	],
	[direct_float=1],
	[direct_float=],
	[direct_float=no],
)
if [[ "x$direct_float" == "x" ]]; then
AC_TRY_RUN(
	[
#include <stdint.h>
int main() {
	union {
		uint32_t i;
		float f;
	} test;
	test.f = 0.15;
	return test.i == 0x9a99193e ? 0 : 1;
}
	],
	[direct_float=2],
	[direct_float=no],
	[direct_float=no],
)
fi
if [[ "x$direct_float" == "xno" ]]; then
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
	AC_DEFINE_UNQUOTED([HAVE_DIRECT_FLOAT_FORMAT], $direct_float, [Define for direct conversion from float to int (2 for swapped by order).])
fi

AC_CONFIG_SRCDIR([src/ebusd/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
		 docs/Makefile
		 src/lib/utils/Makefile
		 src/lib/ebus/Makefile
		 src/lib/ebus/test/Makefile
		 src/ebusd/Makefile
		 src/tools/Makefile])

AC_DEFINE_UNQUOTED(PACKAGE_PIDFILE, LOCALSTATEDIR "/run/" PACKAGE ".pid", [The path and name of the PID file.])
AC_DEFINE_UNQUOTED(PACKAGE_LOGFILE, LOCALSTATEDIR "/log/" PACKAGE ".log", [The path and name of the log file.])
AC_DEFINE_UNQUOTED(PACKAGE_CONFIGPATH, SYSCONFDIR "/" PACKAGE, [The default path of the configuration files.])
AC_DEFINE(SCAN_VERSION, "[m4_esyscmd_s([sed -e 's#^\([0-9]*\.[0-9]*\).*#\1#' -e 's#\.\([0-9]\)$#0\1#' -e 's#\.##' VERSION])]", [The version of the package formatted for the scan result.])
AC_DEFINE(REVISION, "[m4_esyscmd_s([git describe --always 2>/dev/null || (date +p%Y%m%d)])]", [The revision of the package.])

AC_CHECK_PROGS([HAVE_DOXYGEN], [doxygen], [])
if test -z "$HAVE_DOXYGEN"; then
	AC_MSG_WARN([Doxygen not found - continuing without Doxygen support.])
fi

AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$HAVE_DOXYGEN"])
	AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/Doxyfile])])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_RANLIB

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_OUTPUT

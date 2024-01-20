# Support for older autoconf without m4_esyscmd_s
m4_ifndef([m4_esyscmd_s], [m4_define([m4_chomp_all], [m4_format([[%.*s]], m4_bregexp(m4_translit([[$1]], [/], [/ ]), [/*$]), [$1])])])
m4_ifndef([m4_esyscmd_s], [m4_define([m4_esyscmd_s], [m4_chomp_all(m4_esyscmd([$1]))])])
AC_PREREQ([2.63])
AC_INIT([ebusd], m4_esyscmd_s([cat VERSION]), [ebusd@ebusd.eu], [ebusd], [https://github.com/john30/ebusd])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])
AC_GNU_SOURCE

if test -z "${CXXFLAGS}"; then
	CXXFLAGS="-fpic -Wall -Wno-unused-function -Wextra -g -O2"
fi
AC_PROG_CXX([g++-6 g++-5 g++-4.9 g++-4.8 g++])
AX_CXX_COMPILE_STDCXX([11], [ext], [mandatory])

AC_CHECK_HEADERS([arpa/inet.h \
	dirent.h \
	fcntl.h \
	netdb.h \
	poll.h \
	pthread.h \
	sys/ioctl.h \
	sys/select.h \
	sys/time.h \
	syslog.h \
	time.h \
	termios.h])

AC_CHECK_FUNC([timegm], [AC_DEFINE(HAVE_TIMEGM, [1], [Defined if timegm() is available.])])
AC_CHECK_FUNC([cfsetspeed], [AC_DEFINE(HAVE_CFSETSPEED, [1], [Defined if cfsetspeed() is available.])])

AC_CHECK_LIB([pthread], [pthread_setname_np],
	AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], [Defined if pthread_setname_np is available.]),
	AC_MSG_RESULT([Could not find pthread_setname_np in pthread.]))
EXTRA_LIBS=
AC_CHECK_LIB([rt], [clock_gettime], [EXTRA_LIBS+="-lrt"])
AC_SUBST(EXTRA_LIBS)

AC_CHECK_FUNC([pselect], [AC_DEFINE(HAVE_PSELECT, [1], [Defined if pselect() is available.])])
AC_CHECK_FUNC([ppoll], [AC_DEFINE(HAVE_PPOLL, [1], [Defined if ppoll() is available.])])
AC_CHECK_HEADER([linux/serial.h], [AC_DEFINE(HAVE_LINUX_SERIAL, [1], [Defined if linux/serial.h is available.])])
AC_CHECK_HEADER([dev/usb/uftdiio.h], [AC_DEFINE(HAVE_FREEBSD_UFTDI, [1], [Defined if dev/usb/uftdiio.h is available.])])

AC_ARG_ENABLE(coverage, AS_HELP_STRING([--enable-coverage], [enable code coverage tracking]), [CXXFLAGS+=" -coverage -O0"], [])
AC_ARG_WITH(contrib, AS_HELP_STRING([--without-contrib], [disable inclusion of contributed sources]), [], [with_contrib=yes])
if test "x$with_contrib" != "xno"; then
	AC_DEFINE_UNQUOTED(HAVE_CONTRIB, [1], [Defined if contributed sources are enabled.])
fi
AC_ARG_WITH(mqtt, AS_HELP_STRING([--without-mqtt], [disable support for MQTT handling]), [], [with_mqtt=yes])
if test "x$with_mqtt" != "xno"; then
	AC_CHECK_LIB([mosquitto], [mosquitto_lib_init],
		[AC_DEFINE_UNQUOTED(HAVE_MQTT, [1], [Defined if MQTT handling is enabled.])],
		[AC_MSG_RESULT([Could not find mosquitto_lib_init in libmosquitto.])
		with_mqtt="no"])
fi
AM_CONDITIONAL([MQTT], [test "x$with_mqtt" != "xno"])

AC_ARG_WITH(knx, AS_HELP_STRING([--without-knx], [disable support for KNX handling]), [], [with_knx=yes])
if test "x$with_knx" != "xno"; then
	AC_DEFINE_UNQUOTED(HAVE_KNX, [1], [Defined if KNX handling is enabled.])
	AC_ARG_WITH(knxd, AS_HELP_STRING([--with-knxd], [enable support for KNX handling via knxd]), [
		AS_IF([test "x$with_knxd" == "xyes"],
			[AC_CHECK_LIB([eibclient], [EIBSocketURL],
				[AC_DEFINE_UNQUOTED(HAVE_KNXD, [1], [Defined if KNX handling via knxd is enabled.])],
				[AC_MSG_RESULT([Could not find EIBSocketURL in libeibclient.])
				with_knxd="no"])
			])
	], [])
fi
AM_CONDITIONAL([KNX], [test "x$with_knx" != "xno"])
AM_CONDITIONAL([KNXD], [test "x$with_knxd" == "xyes"])

AC_ARG_WITH(ssl, AS_HELP_STRING([--without-ssl], [disable support for SSL]), [], [with_ssl=yes])
if test "x$with_ssl" != "xno"; then
	AC_CHECK_LIB([ssl], [OPENSSL_init_ssl],
		[AC_CHECK_LIB([crypto], [CRYPTO_new_ex_data],
		 	[AC_DEFINE_UNQUOTED(HAVE_SSL, [1], [Defined if SSL is enabled.])],
		 	[AC_MSG_RESULT([Could not find CRYPTO_new_ex_data in libcrypto.])
		 	with_ssl="no"])],
		[AC_MSG_RESULT([Could not find SSL_library_init in libssl.])
		with_ssl="no"])
fi
AM_CONDITIONAL([SSL], [test "x$with_ssl" != "xno"])

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
	AC_DEFINE_UNQUOTED([HAVE_DIRECT_FLOAT_FORMAT], $direct_float, [Defined if direct conversion from float to int is available (2 for swapped byte order).])
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
AM_CONDITIONAL([CONTRIB], [test "x$with_contrib" != "xno"])
AM_COND_IF([CONTRIB], [AC_CONFIG_FILES([
		src/lib/ebus/contrib/Makefile
		src/lib/ebus/contrib/test/Makefile
	])])
AM_COND_IF([KNX], [AC_CONFIG_FILES([
		src/lib/knx/Makefile
	])])

AC_DEFINE_UNQUOTED(PACKAGE_PIDFILE, "${localstatedir}/run/${PACKAGE_NAME}.pid", [The path and name of the PID file.])
AC_DEFINE_UNQUOTED(PACKAGE_LOGFILE, "${localstatedir}/log/${PACKAGE_NAME}.log", [The path and name of the log file.])
AC_DEFINE(SCAN_VERSION, "[m4_esyscmd_s([sed -e 's#^\([0-9]*\.[0-9]*\).*#\1#' -e 's#\.\([0-9]\)$#0\1#' -e 's#\.##' VERSION])]", [The version of the package formatted for the scan result.])
AC_DEFINE(PACKAGE_VERSION_MAJOR, [m4_esyscmd_s([sed -e 's#^\([0-9]*\)\..*$#\1#' VERSION])], [The major version of the package.])
AC_DEFINE(PACKAGE_VERSION_MINOR, [m4_esyscmd_s([sed -e 's#^.*\.\([0-9]*\)$#\1#' VERSION])], [The minor version of the package.])
AC_ARG_VAR(GIT_REVISION, [Predefined value to use for revision instead of running git describe.])
if test -n "$GIT_REVISION"; then
	AC_DEFINE(REVISION, "[m4_esyscmd_s([echo "$GIT_REVISION"])]", [The revision of the package.])
else
	AC_DEFINE(REVISION, "[m4_esyscmd_s([git describe --always 2>/dev/null || (date +p%Y%m%d)])]", [The revision of the package.])
fi
AC_CHECK_PROGS([HAVE_DOXYGEN], [doxygen], [])
if test -z "$HAVE_DOXYGEN"; then
	AC_MSG_WARN([Doxygen not found - continuing without Doxygen support.])
fi

AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$HAVE_DOXYGEN"])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/Doxyfile])])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_RANLIB

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_OUTPUT

AC_PREREQ([2.63])
AC_INIT([ebusd], [1.0.0], [ebusd@johnm.de], [ebusd], [https://github.com/john30/ebusd])

AC_CONFIG_AUX_DIR([build])

if test -z $CXXFLAGS;
	then CXXFLAGS='-g -O2'
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
	AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], ["Define to 1 if pthread has pthread_setname_np."]),
	AC_MSG_RESULT([Could not find pthread_setname_np in pthread.]))

AC_CHECK_FUNC([pselect], [AC_DEFINE(HAVE_PSELECT, [1], ["Define to 1 if pselect() is available."])])
AC_CHECK_FUNC([ppoll], [AC_DEFINE(HAVE_PPOLL, [1], ["Define to 1 if ppoll() is available."])])

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

AC_CHECK_PROGS([HAVE_DOXYGEN], [doxygen])
if test -z "$HAVE_DOXYGEN";
	then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support.])
fi

AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$HAVE_DOXYGEN"])
	AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/Doxyfile])])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_RANLIB

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_OUTPUT

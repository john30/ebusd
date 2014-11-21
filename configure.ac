AC_PREREQ([2.65])
AC_INIT([ebusd], [0.5.0], [ebusd@liwest.at], [ebusd], [https://github.com/yuhu-/ebusd])

AC_PROG_CXX

AC_CHECK_HEADERS([arpa/inet.h \
		  dirent.h \
		  fcntl.h \
		  netdb.h \
		  poll.h \
		  pthread.h \
		  sys/ioctl.h \
		  sys/select.h \
		  termios.h])

AC_CHECK_LIB([pthread], [pthread_setname_np],
	AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], ["Define to 1 if pthread has pthread_setname_np."]),
	AC_MSG_RESULT([Could not find pthread_setname_np in pthread.]))

AC_CHECK_FUNC([pselect], [AC_DEFINE(HAVE_PSELECT, [1], [Define to 1 if pselect() is available.])])
AC_CHECK_FUNC([ppoll], [AC_DEFINE(HAVE_PPOLL, [1], [Define to 1 if ppoll() is available.])])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_SRCDIR([src/ebusd/ebusd.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
		 src/lib/utils/Makefile
		 src/lib/ebus/Makefile
		 src/lib/ebus/test/Makefile
		 src/ebusd/Makefile
		 src/ebusctl/Makefile
		 docs/Makefile])

AC_CHECK_PROGS([HAVE_DOXYGEN], [doxygen])
if test -z "$HAVE_DOXYGEN";
	then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support.])
fi

AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$HAVE_DOXYGEN"])
	AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/Doxyfile])])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])

AM_PROG_AR
AC_PROG_RANLIB

AC_OUTPUT

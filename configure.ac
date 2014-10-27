AC_PREREQ([2.65])
AC_INIT([ebusd], [0.4.0], [roland.jax@liwest.at], [ebusd], [https://github.com/yuhu-/ebusd])

AC_PROG_CXX

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_SRCDIR([src/ebusd/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
		 src/libebus/Makefile
		 src/test/Makefile
		 src/libcore/Makefile
		 src/ebusd/Makefile
		 src/tools/Makefile])

AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])

AM_PROG_AR
AC_PROG_RANLIB

AC_OUTPUT

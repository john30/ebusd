AC_PREREQ([2.65])
AC_INIT([ebusd], [0.4.0], [roland.jax@liwest.at], [ebusd], [https://github.com/yuhu-/ebusd])

AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_AUX_DIR([build])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

PKG_CHECK_MODULES([LIBEBUS], [libebus-0.2 >= 0.2.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_PREREQ([2.65])
AC_INIT([ebus-daemon], [0.1], [roland.jax@liwest.at], [ebus-daemon], [https://github.com/yuhu-/ebus-daemon])

AM_INIT_AUTOMAKE([1.11 -Wall no-define])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_PROG_CXX

PKG_CHECK_MODULES([LIBEBUS], [libebus-0.1 >= 0.1])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

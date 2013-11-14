AC_PREREQ([2.61])
AC_INIT([ebusd], [0.1], [roland.jax@liwest.at])
AC_CONFIG_SRCDIR([src/ebusd.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.
AC_PROG_RANLIB

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([gettimeofday memset select socket strerror strrchr])

AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile tools/Makefile])

AC_OUTPUT

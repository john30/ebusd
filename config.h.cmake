/* Defined if contributed sources are enabled. */
#cmakedefine HAVE_CONTRIB

/* Defined if direct conversion from float to int is available (2 for swapped byte order). */
#cmakedefine HAVE_DIRECT_FLOAT_FORMAT ${HAVE_DIRECT_FLOAT_FORMAT}

/* Defined if MQTT handling is enabled. */
#cmakedefine HAVE_MQTT

/* Defined if KNX handling is enabled. */
#cmakedefine HAVE_KNX

/* Defined if KNX handling via knxd is enabled. */
#cmakedefine HAVE_KNXD

/* Defined if SSL is enabled. */
#cmakedefine HAVE_SSL

/* Defined if ppoll() is available. */
#cmakedefine HAVE_PPOLL

/* Defined if pselect() is available. */
#cmakedefine HAVE_PSELECT

/* Defined if linux/serial.h is available. */
#cmakedefine HAVE_LINUX_SERIAL

/* Defined if dev/usb/uftdiio.h is available. */
#cmakedefine HAVE_FREEBSD_UFTDI

/* Defined if pthread_setname_np is available. */
#cmakedefine HAVE_PTHREAD_SETNAME_NP

/* The name of package. */
#cmakedefine PACKAGE "${PACKAGE_NAME}"

/* The address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT "${PACKAGE_BUGREPORT}"

/* The path and name of the log file. */
#define PACKAGE_LOGFILE LOCALSTATEDIR "/log/" PACKAGE ".log"

/* The full name of this package. */
#cmakedefine PACKAGE_NAME "${PACKAGE_NAME}"

/* The path and name of the PID file. */
#define PACKAGE_PIDFILE LOCALSTATEDIR "/run/" PACKAGE ".pid"

/* The full name and version of this package. */
#cmakedefine PACKAGE_STRING "${PACKAGE_STRING}"

/* The tra name of this package. */
#cmakedefine PACKAGE_TARNAME "${PACKAGE_TARNAME}"

/* The home page for this package. */
#cmakedefine PACKAGE_URL "${PACKAGE_URL}"

/* The version of this package. */
#cmakedefine PACKAGE_VERSION "${PACKAGE_VERSION}"

/* The revision of the package. */
#cmakedefine REVISION "${REVISION}"

/* The version of the package formatted for the scan result. */
#cmakedefine SCAN_VERSION "${SCAN_VERSION}"

/* The version of the package. */
#cmakedefine VERSION "${VERSION}"

/* The major version of the package. */
#cmakedefine PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR}

/* The minor version of the package. */
#cmakedefine PACKAGE_VERSION_MINOR ${PACKAGE_VERSION_MINOR}

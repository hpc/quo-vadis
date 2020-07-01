#
# Copyright (c) 2013-2020 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

AC_PREREQ([2.60])

AC_INIT([quo-vadis],
        [0.0.1-devel],
        [], dnl TODO(skg) Add support email.
        [quo-vadis],
        []) dnl TODO(skg) Add project URL.

AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([src])

################################################################################
# Library versioning.
################################################################################
AX_QUO_VADIS_LIBTOOL_VERSION

################################################################################
# AC_CANONICAL_*
################################################################################
# For hwloc---must be in between AC_INIT and AM_INIT_AUTOMAKE.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

################################################################################
# Automake things
################################################################################
# Note: -Wall -Werror are automake options.  they are NOT compiler flags
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign])
# Build using AM_SILENT_RULES
AM_SILENT_RULES([yes])
AM_CONFIG_HEADER([config.h])

################################################################################
# C++ Language Support
################################################################################
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11([], [mandatory])
AC_PROG_CXXCPP
AC_PROG_CXX_C_O

################################################################################
# Libtool things
################################################################################
AM_PROG_AR

LT_PREREQ([2.4.2])
LT_INIT

################################################################################
# Checks for header files.
################################################################################
AC_CHECK_HEADERS([\
inttypes.h limits.h stdint.h stdlib.h string.h unistd.h stdbool.h time.h \
sched.h strings.h stdio.h errno.h math.h
])

################################################################################
# Checks for typedefs, structures, and compiler characteristics.
################################################################################
AC_TYPE_PID_T
AC_TYPE_SIZE_T

################################################################################
# Checks for library functions.
################################################################################
AC_CHECK_FUNCS([memset strerror strtoul mmap])

################################################################################
# hwloc stuff
################################################################################
# Set some hwloc configure options
hwloc_mode=embedded

# Use hwloc's internal XML support.
enable_libxml2=no

# Disable hwloc's plugin system.
enable_plugins=no

# These are for hwloc2 only, so save now to restore later.
save_enable_static=$enable_static
save_enable_shared=$enable_shared
enable_static=yes
enable_shared=no

# Disable more things we don't need.
enable_cairo=no
enable_gl=no
enable_opencl=no
enable_cuda=no
enable_nvml=no
enable_pci=no
enable_libudev=no

HWLOC_SET_SYMBOL_PREFIX(quo_vadis_internal_)

HWLOC_SETUP_CORE([src/core/hwloc],
                 [],
                 [AC_MSG_ERROR([*** hwloc configure failure. ***])],
                 [1])

HWLOC_DO_AM_CONDITIONALS

CPPFLAGS="$HWLOC_EMBEDDED_CPPFLAGS $CPPFLAGS"
LIBS="$HWLOC_EMBEDDED_LIBS $LIBS"

enable_static=$save_enable_static
enable_shared=$save_enable_shared

################################################################################
# For docs target support
################################################################################
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test "x$DOXYGEN" = "x"; then
    AC_MSG_WARN([Doxygen not found. Continuing without Doxygen support])
fi
AM_CONDITIONAL([QUO_VADIS_HAVE_DOXYGEN_DOCS_SUPPORT],
               [test "x$DOXYGEN" != "x"])

################################################################################
# For package information.
################################################################################
AC_ARG_VAR(USER,
           [ Username used in build info (Default: `id -u -n` output)])
if test "x$USER" = x; then
    USER=`id -u -n`
fi
AC_ARG_VAR(HOSTNAME,
           [ Hostname used in build info (Default: `hostname` output)])
if test "x$HOSTNAME" = x; then
    HOSTNAME=`hostname`
fi
AC_ARG_VAR(DATE,
           [ Date used in build info (Default: `date` output)])
if test "x$DATE" = x; then
    DATE=`date`
fi

AC_SUBST([CC])
AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([CXXFLAGS])
AC_SUBST([CXXCPPFLAGS])
AC_SUBST([USER])
AC_SUBST([HOSTNAME])
AC_SUBST([DATE])
AC_SUBST([FFLAGS])
AC_SUBST([FCFLAGS])
AC_SUBST([LDFLAGS])
AC_SUBST([LIBS])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/core/Makefile
                 src/daemon/Makefile])

AC_OUTPUT

################################################################################
# Emit configuration.
################################################################################
cat << EOF

+-------------------------------------------------------------------------------
| Configuration ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
+-------------------------------------------------------------------------------
| version     : ${VERSION}
| target      : ${target}
| CC          : ${CC}
| CFLAGS      : ${CFLAGS}
| CPP         : ${CPP}
| CPPFLAGS    : ${CPPFLAGS}
| CXX         : ${CXX}
| CXXFLAGS    : ${CXXFLAGS}
| CXXCPPFLAGS : ${CXXCPPFLAGS}
| LDFLAGS     : ${LDFLAGS}
| LIBS        : ${LIBS}
+-------------------------------------------------------------------------------
EOF

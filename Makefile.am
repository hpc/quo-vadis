#
# Copyright (c)      2020 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

ACLOCAL_AMFLAGS = -Iconfig -Isrc/core/hwloc/config

EXTRA_DIST = \
AUTHORS.txt \
README.md \
autogen

SUBDIRS = \
src

AM_CFLAGS = $(HWLOC_EMBEDDED_CFLAGS)

AM_CPPFLAGS = $(HWLOC_EMBEDDED_CPPFLAGS)

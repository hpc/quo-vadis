#
# Copyright (c) 2013-2019 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

SUBDIRS = \
hwloc

lib_LTLIBRARIES = \
libquo-vadis.la

include_HEADERS = \
quo-vadis.h

libquo_vadis_la_SOURCES = \
quo-vadis.cc

libquo_vadis_la_CFLAGS =
libquo_vadis_la_LDFLAGS = -version-info @QUO_VADIS_LIBVINFO@
libquo_vadis_la_LIBADD = $(HWLOC_EMBEDDED_LDADD) $(HWLOC_EMBEDDED_LIBS)

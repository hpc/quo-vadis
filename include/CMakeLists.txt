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

# Install
install(
    FILES
      quo-vadis.h
    DESTINATION
      include
)

install(
    DIRECTORY
      quo-vadis
    DESTINATION
      include
)

# vim: ts=4 sts=4 sw=4 expandtab

#
# Copyright (c)      2022 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Checks size of data types.
include(CheckTypeSize)

check_type_size(int QVI_SIZEOF_INT)
check_type_size(pid_t QVI_SIZEOF_PID_T)

# vim: ts=4 sts=4 sw=4 expandtab

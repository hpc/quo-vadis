#
# Copyright (c) 2020-2023 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

set(
    QV_SANITIZE_SANITIZERS
    "address"
)

# Keep in sync with QV_SANITIZE_SANITIZERS ordering
set(
    QV_SANITIZE_SANITIZERS_CFLAGS
    "-fsanitize=address"
)

# Keep in sync with QV_SANITIZE_SANITIZERS ordering
set(
    QV_SANITIZE_SANITIZERS_LDFLAGS
    "-fsanitize=address"
)

set(
    QV_SANITIZE
    "none" CACHE STRING
    "Available sanitizers: ${QV_SANITIZE_SANITIZERS} all"
)

message(CHECK_START "Determining desired sanitizers")
if(QV_SANITIZE STREQUAL "" OR QV_SANITIZE STREQUAL "none")
    message(CHECK_PASS "none")
else()
    # First remove duplicate values that might exist.
    list(REMOVE_DUPLICATES QV_SANITIZE)
    message(CHECK_PASS "${QV_SANITIZE}")

    set(QV_SANITIZE_CFLAGS "")
    set(QV_SANITIZE_LDFLAGS "")

    if ("none" IN_LIST QV_SANITIZE)
        return()
    elseif("all" IN_LIST QV_SANITIZE)
        list(APPEND QV_SANITIZE_CFLAGS ${QV_SANITIZE_SANITIZERS_CFLAGS})
        list(APPEND QV_SANITIZE_LDFLAGS ${QV_SANITIZE_SANITIZERS_LDFLAGS})
    else()
        # TODO(skg) FIXME We only support all or none at this point.
    endif()

    if (QV_SANITIZE_CFLAGS)
        # TODO(skg) FIXME -fno-omit-frame-pointer
        message(STATUS "Adding sanitizer flags: ${QV_SANITIZE_CFLAGS}")
        add_compile_options(${QV_SANITIZE_CFLAGS} -fno-omit-frame-pointer)
    endif()

    if (QV_SANITIZE_LDFLAGS)
        add_link_options(${QV_SANITIZE_LDFLAGS})
    endif()
endif()

# vim: ts=4 sts=4 sw=4 expandtab

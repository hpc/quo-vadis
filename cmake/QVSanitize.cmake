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
    "thread"
    "undefined"
)

# User parameter to set sanitizers.
set(
    QV_SANITIZE
    "none" CACHE STRING
    "Available sanitizers: ${QV_SANITIZE_SANITIZERS}"
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

    if("none" IN_LIST QV_SANITIZE)
        return()
    else()
        foreach(san ${QV_SANITIZE})
            if("${san}" IN_LIST QV_SANITIZE_SANITIZERS)
                list(APPEND QV_SANITIZE_CFLAGS "-fsanitize=${san}")
                list(APPEND QV_SANITIZE_LDFLAGS "-fsanitize=${san}")
            else()
                message(WARNING "Skipping unrecognized sanitizer: ${san}")
            endif()
        endforeach()
    endif()

    if(QV_SANITIZE_CFLAGS)
        message(STATUS "Adding sanitizer compile flags...")
        add_compile_options(${QV_SANITIZE_CFLAGS} -fno-omit-frame-pointer)
    endif()

    if(QV_SANITIZE_LDFLAGS)
        message(STATUS "Adding sanitizer link flags...")
        add_link_options(${QV_SANITIZE_LDFLAGS})
    endif()
endif()

# vim: ts=4 sts=4 sw=4 expandtab

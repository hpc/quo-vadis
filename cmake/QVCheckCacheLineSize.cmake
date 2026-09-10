#
# Copyright (c) 2026 Triad National Security, LLC
#                    All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Determines the L1 data cache line size (in bytes) at configure time and
# exposes it as QVI_L1_CACHE_LINESIZE for config.h. The value is detected
# natively via sysconf(_SC_LEVEL1_DCACHE_LINESIZE) when possible. A reasonable
# default of 64 bytes is used when cross-compiling or when detection fails.
# The value is a user-overridable cache variable so cross builds may specify
# an appropriate value.

# User-overridable default. 64 bytes is a common L1 cache line size.
set(QVI_L1_CACHE_LINESIZE 64 CACHE STRING
    "L1 data cache line size in bytes"
)

message(CHECK_START "Determining L1 data cache line size")

if(NOT CMAKE_CROSSCOMPILING)
    set(_qvi_l1_source "default (detection failed)")
    set(_qvi_l1_probe "${CMAKE_CURRENT_BINARY_DIR}/qvi_l1_cache_linesize.c")
    file(WRITE "${_qvi_l1_probe}" "
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdio.h>
int main(void) {
    long sz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    /* Signal failure so we fall back to the default. */
    if (sz <= 0) return 1;
    printf(\"%ld\", sz);
    return 0;
}
")
    try_run(
        _qvi_l1_run_result _qvi_l1_compile_result
        "${CMAKE_CURRENT_BINARY_DIR}/qvi_l1_cache_linesize"
        "${_qvi_l1_probe}"
        RUN_OUTPUT_VARIABLE _qvi_l1_run_output
    )
    if(_qvi_l1_compile_result AND (_qvi_l1_run_result EQUAL 0)
       AND (_qvi_l1_run_output MATCHES "^[1-9][0-9]*$"))
        set(QVI_L1_CACHE_LINESIZE ${_qvi_l1_run_output} CACHE STRING
            "L1 data cache line size in bytes" FORCE
        )
        set(_qvi_l1_source "detected")
    endif()
    unset(_qvi_l1_probe)
    unset(_qvi_l1_run_result)
    unset(_qvi_l1_compile_result)
    unset(_qvi_l1_run_output)
else()
    set(_qvi_l1_source "default (cross-compiling)")
endif()

message(CHECK_PASS "${QVI_L1_CACHE_LINESIZE} bytes (${_qvi_l1_source})")
unset(_qvi_l1_source)

# vim: ts=4 sts=4 sw=4 expandtab

/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common.h
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// We use the constants in errno for internal error codes.
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_THREADS_H
#include <threads.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

/* ////////////////////////////////////////////////////////////////////////// */
/* Internal convenience macros                                                */
/* ////////////////////////////////////////////////////////////////////////// */
#define QVI_STRINGIFY(x) #x
#define QVI_TOSTRING(x)  QVI_STRINGIFY(x)

#define QVI_ERR_AT       __FILE__ ":" QVI_TOSTRING(__LINE__) ""
#define QVI_ERR_PREFIX   "[" PACKAGE_NAME " ERROR at " QVI_ERR_AT "] "
#define QVI_PANIC_PREFIX "[" PACKAGE_NAME " PANIC at " QVI_ERR_AT "] "
#define QVI_WARN_PREFIX  "[" PACKAGE_NAME " WARNING at " QVI_ERR_AT "] "

#define qvi_panic(msg)                                                         \
do {                                                                           \
    fprintf(stderr, QVI_PANIC_PREFIX "%s failed: %s.\n", __func__, (msg));     \
    fflush(stderr);                                                            \
    _exit(EXIT_FAILURE);                                                       \
} while(0)

static inline char *
qvi_strerr(int ec)
{
    static thread_local char sb[4096];
    return strerror_r(ec, sb, sizeof(sb));
}

static inline char *
qvi_msg(const char *fmt, ...)
{
    static thread_local char sb[4096];
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(sb, sizeof(sb), fmt, args);
    return sb;
}

#ifdef __cplusplus
}
#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

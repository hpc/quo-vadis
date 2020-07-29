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

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Internal convenience macros                                                //
////////////////////////////////////////////////////////////////////////////////
#define QVI_STRINGIFY(x) #x
#define QVI_TOSTRING(x)  QVI_STRINGIFY(x)

static inline char *
qvi_strerr(int ec)
{
    static thread_local char sb[4096];
    return strerror_r(ec, sb, sizeof(sb));
}

#ifdef __cplusplus
}
#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

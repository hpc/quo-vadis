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

#ifndef QVI_CORE_COMMON_H_INCLUDED
#define QVI_CORE_COMMON_H_INCLUDED

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

#include <cstdlib>
#include <cstdio>

/* ////////////////////////////////////////////////////////////////////////// */
/* Internal convenience macros                                                */
/* ////////////////////////////////////////////////////////////////////////// */
#define QVI_STRINGIFY(x) #x
#define QVI_TOSTRING(x)  QVI_STRINGIFY(x)

#define QVI_ERR_AT       __FILE__ ":" QVI_TOSTRING(__LINE__) ""
#define QVI_ERR_PREFIX   "[" PACKAGE_NAME " ERROR @ " QVI_ERR_AT "] "
#define QVI_PANIC_PREFIX "[" PACKAGE_NAME " PANIC @ " QVI_ERR_AT "] "
#define QVI_WARN_PREFIX  "[" PACKAGE_NAME " WARNING @ " QVI_ERR_AT "] "

#define qvi_panic(msg)                                                         \
do {                                                                           \
    fprintf(stderr, QVI_PANIC_PREFIX "%s failed: %s.\n", __func__, (msg));     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while(0)

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

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
 * @file qvi-utils.h
 */

#ifndef QVI_UTILS_H
#define QVI_UTILS_H

#include "private/qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
static inline char *
qvi_strerr(int ec)
{
    static thread_local char sb[4096];
    return strerror_r(ec, sb, sizeof(sb));
}

/**
 *
 */
static inline pid_t
qvi_gettid(void) {
    return (pid_t)syscall(SYS_gettid);
}

/**
 *
 */
double
qvi_time(void);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

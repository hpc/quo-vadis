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
 * @file qvi-common.h
 */

#ifndef QVI_COMMON_H
#define QVI_COMMON_H

#include "quo-vadis/config.h"
#include "quo-vadis/qv-rc.h"

#include "private/qvi-macros.h"

#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <iostream>

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

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

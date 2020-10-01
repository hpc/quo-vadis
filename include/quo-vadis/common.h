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

#ifndef QUO_VADIS_COMMON_H
#define QUO_VADIS_COMMON_H

#include "quo-vadis/config.h"
#include "quo-vadis/rc.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>

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

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
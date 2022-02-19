/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
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
#include "qvi-macros.h"
#include "quo-vadis.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include "qvi-log.h"

#include <chrono>
#include <map>
#include <new>
#include <set>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#endif

typedef char const* cstr_t;
typedef uint8_t byte_t;

/**
 *
 */
int
qvi_context_create(
    qv_context_t **ctx
);

/**
 *
 */
void
qvi_context_free(
    qv_context_t **ctx
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

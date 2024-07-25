/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

// IWYU pragma: begin_keep
#include "quo-vadis/config.h"
#include "qvi-macros.h"
#include "quo-vadis.h"

#include "hwloc.h"

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
#include <mutex>
#include <new>
#include <numeric>
#include <set>
#include <stack>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#endif
// IWYU pragma: end_keep

// Internal type aliases.
typedef uint8_t byte_t;
typedef char const * cstr_t;
typedef unsigned int uint_t;

// Forward declarations.
struct qvi_hwloc_s;
typedef struct qvi_hwloc_s qvi_hwloc_t;

struct qvi_hwpool_s;

struct qvi_rmi_client_s;
typedef struct qvi_rmi_client_s qvi_rmi_client_t;

struct qvi_bbuff_s;
typedef struct qvi_bbuff_s qvi_bbuff_t;

struct qvi_task_s;
typedef struct qvi_task_s qvi_task_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

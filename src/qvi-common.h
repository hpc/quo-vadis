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

#include "quo-vadis/config.h" // IWYU pragma: keep
#include "qvi-macros.h" // IWYU pragma: keep
#include "quo-vadis.h" // IWYU pragma: keep

#include <assert.h> // IWYU pragma: keep
#include <errno.h> // IWYU pragma: keep
#include <fcntl.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h> // IWYU pragma: keep
#include <pthread.h>
#include <stdarg.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include "qvi-log.h" // IWYU pragma: keep
 // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#include <map> // IWYU pragma: keep
#include <new> // IWYU pragma: keep
#include <set> // IWYU pragma: keep
#include <stack> // IWYU pragma: keep
#include <stdexcept> // IWYU pragma: keep
#include <thread> // IWYU pragma: keep
#include <unordered_map> // IWYU pragma: keep
#include <unordered_set> // IWYU pragma: keep
#include <vector> // IWYU pragma: keep
#endif

typedef uint8_t byte_t;
typedef char const* cstr_t;
typedef unsigned int uint_t;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

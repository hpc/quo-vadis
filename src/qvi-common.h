/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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
#include <ftw.h>
#include <getopt.h>
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
#include <sys/wait.h>
#include <unistd.h>

#ifdef __cplusplus
#include "qvi-log.h"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <list>
#include <map>
#include <mutex>
#include <condition_variable>
#include <new>
#include <numeric>
#include <random>
#include <regex>
#include <set>
#include <stack>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cereal/access.hpp"
#endif
// IWYU pragma: end_keep

// Internal type aliases.
using byte_t = uint8_t;
using cstr_t = char const *;
using uint_t = unsigned int;

// Forward declarations.
struct qvi_bbuff;
struct qvi_hwloc;
struct qvi_rmi_client;
struct qvi_hwpool;
struct qvi_task;

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-test-utils.h
 *
 * Common test infrastructure.
 */

#ifndef COMMON_TEST_UTILS_H
#define COMMON_TEST_UTILS_H

// Avoid including internal QV headers here so
// we mimic real application usage in our tests.
#include "quo-vadis.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// IWYU pragma: begin_keep
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
// IWYU pragma: end_keep

#define CTU_STRINGIFY(x) #x
#define CTU_TOSTRING(x)  CTU_STRINGIFY(x)

#define ctu_unused(x)                                                          \
do {                                                                           \
    (void)(x);                                                                 \
} while (0)

#define ctu_panic(...)                                                         \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

#define ctu_assert(condition, ...)                                             \
do {                                                                           \
    if (!(condition)) {                                                        \
        ctu_panic(__VA_ARGS__);                                                \
    }                                                                          \
} while (0)

/**
 * Asserts that a quo-vadis call returned QV_SUCCESS, aborting with the
 * qv_strerr() string otherwise.
 */
#define ctu_check(rc, what)                                                    \
do {                                                                           \
    const int ctu_rc = (rc);                                                   \
    if (ctu_rc != QV_SUCCESS) {                                                \
        ctu_panic("%s failed (rc=%s)", (what), qv_strerr(ctu_rc));             \
    }                                                                          \
} while (0)

/**
 * Asserts that an MPI call returned MPI_SUCCESS. On failure it prints the MPI
 * error string (when available) and aborts. Defined as a macro so callers that
 * never touch MPI (e.g. the process/thread suites) need not include <mpi.h>.
 */
#define ctu_mpi_check(rc, what)                                                \
do {                                                                           \
    const int ctu_mpirc = (rc);                                                \
    if (ctu_mpirc != MPI_SUCCESS) {                                            \
        char ctu_mpiestr[MPI_MAX_ERROR_STRING] = {0};                          \
        int ctu_elen = 0;                                                      \
        if (MPI_Error_string(ctu_mpirc, ctu_mpiestr, &ctu_elen) !=             \
            MPI_SUCCESS) {                                                     \
            ctu_mpiestr[0] = '\0';                                             \
        }                                                                      \
        ctu_panic("%s failed (rc=%d: %s)", (what), ctu_mpirc, ctu_mpiestr);    \
    }                                                                          \
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTU_SCOPE_KIND_PROCESS = 0,
    CTU_SCOPE_KIND_THREAD,
    CTU_SCOPE_KIND_MPI
} ctu_scope_kind_t;

// Returns a human-readable name for a scope kind (used for labeling output).
static inline const char *
ctu_scope_kind_name(
    ctu_scope_kind_t kind
) {
    switch (kind) {
        case CTU_SCOPE_KIND_PROCESS: return "process";
        case CTU_SCOPE_KIND_THREAD:  return "thread";
        case CTU_SCOPE_KIND_MPI:     return "mpi";
        default:                     return "?";
    }
}

typedef struct {
    char const *name;
    qv_hw_type_t type;
} ctu_hw_obj_name_to_type_t;

typedef struct {
    char const *name;
    qv_device_id_type_t devid;
} ctu_devid_name_to_id_t;

// Maps QV object type names to their underlying values.
static const ctu_hw_obj_name_to_type_t ctu_hw_obj_name_to_type_tab[] = {
    {CTU_TOSTRING(QV_HW_MACHINE),  QV_HW_MACHINE},
    {CTU_TOSTRING(QV_HW_PACKAGE),  QV_HW_PACKAGE},
    {CTU_TOSTRING(QV_HW_NUMANODE), QV_HW_NUMANODE},
    {CTU_TOSTRING(QV_HW_CORE),     QV_HW_CORE},
    {CTU_TOSTRING(QV_HW_PU),       QV_HW_PU},
    {CTU_TOSTRING(QV_HW_L1CACHE),  QV_HW_L1CACHE},
    {CTU_TOSTRING(QV_HW_L2CACHE),  QV_HW_L2CACHE},
    {CTU_TOSTRING(QV_HW_L3CACHE),  QV_HW_L3CACHE},
    {CTU_TOSTRING(QV_HW_L4CACHE),  QV_HW_L4CACHE},
    {CTU_TOSTRING(QV_HW_L5CACHE),  QV_HW_L5CACHE}
};

static const size_t ctu_hw_obj_name_to_type_tab_size =
    sizeof(ctu_hw_obj_name_to_type_tab) / sizeof(ctu_hw_obj_name_to_type_t);

// Maps QV device ID type names to their underlying values.
static const ctu_devid_name_to_id_t ctu_devid_name_to_id_tab[] = {
    {CTU_TOSTRING(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {CTU_TOSTRING(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {CTU_TOSTRING(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

static const size_t ctu_devid_name_to_id_tab_size =
    sizeof(ctu_devid_name_to_id_tab) / sizeof(ctu_devid_name_to_id_t);


static inline const char *
ctu_obj_name(
    qv_hw_type_t type
) {
    switch(type) {
        case QV_HW_MACHINE:  return CTU_TOSTRING(QV_HW_MACHINE);
        case QV_HW_PACKAGE:  return CTU_TOSTRING(QV_HW_PACKAGE);
        case QV_HW_CORE:     return CTU_TOSTRING(QV_HW_CORE);
        case QV_HW_PU:       return CTU_TOSTRING(QV_HW_PU);
        case QV_HW_L1CACHE:  return CTU_TOSTRING(QV_HW_L1CACHE);
        case QV_HW_L2CACHE:  return CTU_TOSTRING(QV_HW_L2CACHE);
        case QV_HW_L3CACHE:  return CTU_TOSTRING(QV_HW_L3CACHE);
        case QV_HW_L4CACHE:  return CTU_TOSTRING(QV_HW_L4CACHE);
        case QV_HW_L5CACHE:  return CTU_TOSTRING(QV_HW_L5CACHE);
        case QV_HW_NUMANODE: return CTU_TOSTRING(QV_HW_NUMANODE);
        /** Device types. */
        case QV_HW_GPU:      return CTU_TOSTRING(QV_HW_GPU);
        case QV_HW_NIC:      return CTU_TOSTRING(QV_HW_NIC);
        default: return "?";
    }
}

static inline pid_t
ctu_gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

struct ctu_str_s;
typedef struct ctu_str_s ctu_str_t;

ctu_str_t *
ctu_str_new(void);

void
ctu_str_del(
    ctu_str_t *str
);

const char *
ctu_str_cstr(
    ctu_str_t *str
);

void
ctu_str_appendf(
    ctu_str_t *str,
    const char *format,
    ...
);

void
ctu_logf(
    const char *format,
    ...
);

void
ctu_emit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *format,
    ...
);

void
ctu_pemit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    bool pred,
    const char *format,
    ...
);

void
ctu_emit_task_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

void
ctu_emit_host_hw_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *scope_name
);

void
ctu_emit_device_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    qv_hw_type_t dev_type,
    const char *scope_name
);

void
ctu_emit_scope_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *const scope_name
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

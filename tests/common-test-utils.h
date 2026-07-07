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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTU_SCOPE_KIND_PROCESS = 0,
    CTU_SCOPE_KIND_PTHREAD,
    CTU_SCOPE_KIND_OPENMP,
    CTU_SCOPE_KIND_MPI
} ctu_scope_kind_t;

typedef struct {
    char const *name;
    qv_hw_obj_type_t type;
} ctu_hw_obj_name_to_type_t;

typedef struct {
    char const *name;
    qv_device_id_type_t devid;
} ctu_devid_name_to_id_t;

// Maps QV object type names to their underlying values.
static const ctu_hw_obj_name_to_type_t ctu_hw_obj_name_to_type_tab[] = {
    {CTU_TOSTRING(QV_HW_OBJ_MACHINE),  QV_HW_OBJ_MACHINE},
    {CTU_TOSTRING(QV_HW_OBJ_PACKAGE),  QV_HW_OBJ_PACKAGE},
    {CTU_TOSTRING(QV_HW_OBJ_NUMANODE), QV_HW_OBJ_NUMANODE},
    {CTU_TOSTRING(QV_HW_OBJ_CORE),     QV_HW_OBJ_CORE},
    {CTU_TOSTRING(QV_HW_OBJ_PU),       QV_HW_OBJ_PU},
    {CTU_TOSTRING(QV_HW_OBJ_L1CACHE),  QV_HW_OBJ_L1CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L2CACHE),  QV_HW_OBJ_L2CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L3CACHE),  QV_HW_OBJ_L3CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L4CACHE),  QV_HW_OBJ_L4CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L5CACHE),  QV_HW_OBJ_L5CACHE}
};

static const size_t ctu_hw_obj_name_to_type_tab_size =
    sizeof(ctu_hw_obj_name_to_type_tab) / sizeof(ctu_hw_obj_name_to_type_t);

static inline const char *
ctu_obj_name(
    qv_hw_obj_type_t type
) {
    switch(type) {
        case QV_HW_OBJ_MACHINE:  return CTU_TOSTRING(QV_HW_OBJ_MACHINE);
        case QV_HW_OBJ_PACKAGE:  return CTU_TOSTRING(QV_HW_OBJ_PACKAGE);
        case QV_HW_OBJ_CORE:     return CTU_TOSTRING(QV_HW_OBJ_CORE);
        case QV_HW_OBJ_PU:       return CTU_TOSTRING(QV_HW_OBJ_PU);
        case QV_HW_OBJ_L1CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L1CACHE);
        case QV_HW_OBJ_L2CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L2CACHE);
        case QV_HW_OBJ_L3CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L3CACHE);
        case QV_HW_OBJ_L4CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L4CACHE);
        case QV_HW_OBJ_L5CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L5CACHE);
        case QV_HW_OBJ_NUMANODE: return CTU_TOSTRING(QV_HW_OBJ_NUMANODE);
        /** Device types. */
        case QV_HW_OBJ_GPU:      return CTU_TOSTRING(QV_HW_OBJ_GPU);
        case QV_HW_OBJ_NIC:      return CTU_TOSTRING(QV_HW_OBJ_NIC);
        default: return "?";
    }
}

// Maps QV device ID type names to their underlying values.
static const ctu_devid_name_to_id_t ctu_devid_name_to_id_tab[] = {
    {CTU_TOSTRING(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {CTU_TOSTRING(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {CTU_TOSTRING(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

static const size_t ctu_devid_name_to_id_tab_size =
    sizeof(ctu_devid_name_to_id_tab) / sizeof(ctu_devid_name_to_id_t);

static inline pid_t
ctu_gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

// A deferred printf implementation.
void
ctu_dprintf(
    const char *format,
    ...
);

// Outputs the stored content filled by calls to ctu_dprintf() to stdout.
void
ctu_dflush(void);

void
ctu_emit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *format,
    ...
);

void
ctu_emit_task_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

/**
 * A verbose version of qv_bind_push().
 */
void
ctu_bind_push(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

/**
 * A verbose version of qv_bind_pop().
 */
void
ctu_bind_pop(
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
    qv_hw_obj_type_t dev_type,
    const char *scope_name
);

void
ctu_hostres_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

void
ctu_cpuset_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

void
ctu_scope_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *const scope_name
);

/**
 * Collective call over the provided scope that tests pushing and popping of
 * binding policies.
 */
void
ctu_change_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

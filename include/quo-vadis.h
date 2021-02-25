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
 * @file quo-vadis.h
 */

#ifndef QUO_VADIS_H
#define QUO_VADIS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Convenience definition. */
#define QUO_VADIS 1

/** Opaque quo-vadis context. */
struct qv_context_s;
typedef struct qv_context_s qv_context_t;

/** Opaque quo-vadis task. */
struct qv_task_s;
typedef struct qv_task_s qv_task_t;

/** Opaque quo-vadis group. */
struct qv_group_s;
typedef struct qv_group_s qv_group_t;

/** Opaque quo-vadis scope. */
struct qv_scope_s;
typedef struct qv_scope_s qv_scope_t;

/* If this changes, please update the order and contents of qvi_rc_strerrs. */
/** Return codes. */
enum {
    QV_SUCCESS = 0,
    QV_SUCCESS_ALREADY_DONE,
    QV_ERR,
    QV_ERR_ENV,
    QV_ERR_INTERNAL,
    QV_ERR_SYS,
    QV_ERR_OOR,
    QV_ERR_INVLD_ARG,
    QV_ERR_CALL_BEFORE_INIT,
    QV_ERR_HWLOC,
    QV_ERR_MPI,
    QV_ERR_MSG,
    QV_ERR_RPC,
    QV_ERR_NOT_SUPPORTED,
    QV_ERR_POP,
    QV_ERR_PMI,
    QV_ERR_NOT_FOUND,
    /** Sentinel value. */
    QV_RC_LAST
};

/** Intrinsic scopes. */
typedef enum qv_scope_intrinsic_e {
    QV_SCOPE_EMPTY = 0,
    QV_SCOPE_SYSTEM,
    QV_SCOPE_USER,
    QV_SCOPE_JOB,
    QV_SCOPE_PROCESS
} qv_scope_intrinsic_t;

// TODO(skg) Rename? Remove hwloc prefix?
typedef enum qv_hwloc_obj_type_e {
    QV_HWLOC_OBJ_MACHINE,
    QV_HWLOC_OBJ_PACKAGE,
    QV_HWLOC_OBJ_CORE,
    QV_HWLOC_OBJ_PU,
    QV_HWLOC_OBJ_L1CACHE,
    QV_HWLOC_OBJ_L2CACHE,
    QV_HWLOC_OBJ_L3CACHE,
    QV_HWLOC_OBJ_L4CACHE,
    QV_HWLOC_OBJ_L5CACHE,
    QV_HWLOC_OBJ_NUMANODE,
    // TODO(skg) Consider just providing things like GPU
    QV_HWLOC_OBJ_OS_DEVICE
} qv_hwloc_obj_type_t;

/**
 *
 */
int
qv_create(
    qv_context_t **ctx
);

/**
 *
 */
int
qv_free(
    qv_context_t *ctx
);

/**
 *
 */
int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
);

/**
 *
 */
int
qv_scope_free(
    qv_context_t *ctx,
    qv_scope_t *scope
);

/**
 *
 */
const char *
qv_strerr(
    int ec
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

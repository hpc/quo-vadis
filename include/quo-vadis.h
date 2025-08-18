/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
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

/**
 * This number is updated to (X<<16)+(Y<<8)+Z
 * when a release X.Y.Z modifies the API.
 *
 * In python: print(f'{(X<<16)+(Y<<8)+Z:#010x}')
 */
#define QUO_VADIS_API_VERSION 0x00000001

/** Opaque quo-vadis scope. */
struct qv_scope;
typedef struct qv_scope qv_scope_t;

/**
 * Return codes.
 */
enum {
    QV_SUCCESS = 0,
    QV_SUCCESS_ALREADY_DONE,
    QV_SUCCESS_SHUTDOWN,
    QV_ERR,
    QV_ERR_ENV,
    QV_ERR_INTERNAL,
    QV_ERR_FILE_IO,
    QV_ERR_SYS,
    QV_ERR_OOR,
    QV_ERR_INVLD_ARG,
    QV_ERR_HWLOC,
    QV_ERR_MPI,
    QV_ERR_MSG,
    QV_ERR_RPC,
    QV_ERR_NOT_SUPPORTED,
    QV_ERR_NOT_FOUND,
    QV_ERR_SPLIT,
    /** Resources unavailable. */
    QV_RES_UNAVAILABLE
};

/**
 * Intrinsic scope types.
 */
typedef enum {
    /**
     * Include all system resources, including those disallowed by mechanisms
     * such as Linux cgroups. Use this type with care.
     */
    QV_SCOPE_SYSTEM = 0,
    /**
     * Includes all allowed resources on the system. For example, if cgroups are
     * used, using this type will result in a scope containing only resources
     * allowed in the encompassing cgroup.
     */
    QV_SCOPE_USER,
    /**
     * Includes the union of resources available to the calling processes. For
     * example, if the calling processes are bound to a strict subset of the
     * available system resources, then the resulting scope will include
     * strictly that subset.
     */
    QV_SCOPE_JOB,
    /**
     * Like QV_SCOPE_USER, but the group comprises only the calling process.
     */
    QV_SCOPE_PROCESS
} qv_scope_intrinsic_t;

/**
 * Intrinsic scope flags that invluence *_scope_get() behavior.
 *
 * Note to developers: long long is guaranteed to be at least 64 bits,
 * which should be plenty for our needs.
 */
typedef long long qv_scope_flags_t;

/**
 * Empty flags, get scope with default behavior.
 */
const qv_scope_flags_t QV_SCOPE_FLAG_NONE   = (0LL);

/**
 * Disable use of SMT.
 */
const qv_scope_flags_t QV_SCOPE_FLAG_NO_SMT = (1LL<<0);

/**
 * Hardware object types.
 */
typedef enum {
    QV_HW_OBJ_MACHINE = 0,
    QV_HW_OBJ_PACKAGE,
    QV_HW_OBJ_CORE,
    QV_HW_OBJ_PU,
    QV_HW_OBJ_L1CACHE,
    QV_HW_OBJ_L2CACHE,
    QV_HW_OBJ_L3CACHE,
    QV_HW_OBJ_L4CACHE,
    QV_HW_OBJ_L5CACHE,
    QV_HW_OBJ_NUMANODE,
    /** Device types */
    QV_HW_OBJ_GPU,
    /** Sentinel value. */
    QV_HW_OBJ_LAST
} qv_hw_obj_type_t;

/**
 * Binding string representation format flags.
 */
typedef int qv_bind_string_flags_t;

/**
 * Output the logical binding.
 */
const qv_bind_string_flags_t QV_BIND_STRING_LOGICAL = (1<<0);

/**
 * Output the physical (OS) binding.
 */
const qv_bind_string_flags_t QV_BIND_STRING_PHYSICAL = (1<<1);

/**
 * Automatic grouping options for qv_scope_split(). The following values can be
 * used instead of group_id to influence how automatic task grouping is
 * accomplished.
 */

/**
 * Constant used to indicate undefined or unknown integer value. This means that
 * the caller will not be considered in the split, and therefore receive an
 * empty scope.
 */
const int QV_SCOPE_SPLIT_UNDEFINED = -1;
/**
 * Split the provided group by attempting to preserve tasks' current affinities
 * (at time of the split call) as much as possible.
 */
const int QV_SCOPE_SPLIT_AFFINITY_PRESERVING = -2;
/**
 *
 */
const int QV_SCOPE_SPLIT_PACKED = -3;
/**
 *
 */
const int QV_SCOPE_SPLIT_SPREAD = -4;

/**
 *
 */
typedef enum {
    // TODO(skg) Enumerate all actual values.
    // TODO(skg) Add to Fortran interface.
    QV_SCOPE_CREATE_HINT_NONE      = 0,
    QV_SCOPE_CREATE_HINT_EXCLUSIVE = 1<<0,
    QV_SCOPE_CREATE_HINT_CLOSE     = 1<<1
} qv_scope_create_hints_t;

/**
 * Device identifier types.
 */
typedef enum {
    QV_DEVICE_ID_UUID = 0,
    QV_DEVICE_ID_PCI_BUS_ID,
    QV_DEVICE_ID_ORDINAL
} qv_device_id_type_t;

/**
 * Version query function.
 *
 * @param[out] major Major version.
 *
 * @param[out] minor Minor version.
 *
 * @param[out] patch Patch version.
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_version(
    int *major,
    int *minor,
    int *patch
);

/**
 * Returns a pointer to a string describing the error code passed in the argument ec.
 */
const char *
qv_strerr(
    int ec
);

/**
 * Creates a process context.
 */
int
qv_process_scope_get(
    qv_scope_intrinsic_t iscope,
    qv_scope_flags_t flags,
    qv_scope_t **scope
);

/**
 *
 */
// TODO(skg) Add to Fortran interface.
int
qv_scope_create(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t hint,
    qv_scope_t **subscope
);

/**
 *
 */
int
qv_scope_group_rank(
    qv_scope_t *scope,
    int *rank
);

/**
 *
 */
int
qv_scope_group_size(
    qv_scope_t *scope,
    int *group_size
);

/**
 *
 */
int
qv_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *nobjs
);

/**
 *
 */
int
qv_scope_device_id_get(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int dev_index,
    qv_device_id_type_t id_type,
    char **dev_id
);

/**
 *
 */
int
qv_scope_barrier(
    qv_scope_t *scope
);

/**
 *
 */
int
qv_scope_split(
    qv_scope_t *scope,
    int npieces,
    int group_id,
    qv_scope_t **subscope
);

/**
 *
 */
int
qv_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **subscope
);

/**
 *
 */
int
qv_scope_bind_push(
    qv_scope_t *scope
);

/**
 *
 */
int
qv_scope_bind_pop(
    qv_scope_t *scope
);

/**
 *
 */
int
qv_scope_bind_string(
    qv_scope_t *scope,
    qv_bind_string_flags_t flags,
    char **str
);

/**
 * Releases resources associated with the provided scope.
 */
int
qv_scope_free(
    qv_scope_t *scope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

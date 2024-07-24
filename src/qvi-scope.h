/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-scope.h
 */

#ifndef QVI_SCOPE_H
#define QVI_SCOPE_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-group.h"
#include "qvi-hwloc.h"

/**
 * Frees a scope.
 */
void
qvi_scope_free(
    qv_scope_t **scope
);

/**
 * Frees scope resources and container created by qvi_scope_thsplit*.
 */
void
qvi_scope_thfree(
    qv_scope_t ***kscopes,
    uint_t k
);

/**
 * Returns the caller's group rank in the provided scope.
 */
int
qvi_scope_group_rank(
    qv_scope_t *scope,
    int *taskid
);

/**
 *
 */
int
qvi_scope_group_size(
    qv_scope_t *scope,
    int *ntasks
);

/**
 *
 */
int
qvi_scope_barrier(
    qv_scope_t *scope
);

int
qvi_scope_bind_push(
    qv_scope_t *scope
);

int
qvi_scope_bind_pop(
    qv_scope_t *scope
);

int
qvi_scope_bind_string(
    qv_scope_t *scope,
    qv_bind_string_format_t format,
    char **str
);

/**
 * Returns a new intrinsic scope.
 */
int
qvi_scope_get(
    qvi_group_t *group,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
);

/**
 * Returns a pointer to the scope's underlying group.
 */
qvi_group_t *
qvi_scope_group_get(
    qv_scope_t *scope
);

/**
 * Returns a const reference to the provided scope's cpuset.
 */
const qvi_hwloc_bitmap_s &
qvi_scope_cpuset_get(
    qv_scope_t *scope
);

int
qvi_scope_thsplit(
    qv_scope_t *parent,
    uint_t npieces,
    int *kcolors,
    uint_t k,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t ***kchildren
);

int
qvi_scope_thsplit_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int *kgroup_ids,
    uint_t k,
    qv_scope_t ***kchildren
);

/**
 *
 */
int
qvi_scope_split(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_hw_obj_type_t maybe_obj_type,
    qv_scope_t **child
);

/**
 *
 */
int
qvi_scope_split_at(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **child
);

/**
 *
 */
int
qvi_scope_create(
    qv_scope_t *parent,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t hints,
    qv_scope_t **child
);

/**
 *
 */
int
qvi_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *n
);

int
qvi_scope_get_device_id(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t id_type,
    char **dev_id
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

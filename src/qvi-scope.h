/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
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

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-hwpool.h"

struct qv_scope {
private:
    /** Task group associated with this scope instance. */
    qvi_group *m_group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool *m_hwpool = nullptr;
public:
    /** Constructor */
    qv_scope(void) = delete;
    /** Constructor */
    qv_scope(
        qvi_group *group,
        qvi_hwpool *hwpool
    );
    /** Takes the provided group and creates a new intrinsic scope from it. */
    static int
    make_intrinsic(
        qvi_group *group,
        qv_scope_intrinsic_t iscope,
        qv_scope_t **scope
    );
    /**
     * Creates a new scope based on the specified hardware
     * type, number of resources, and creation hints.
     */
    int
    create(
        qv_hw_obj_type_t type,
        int nobjs,
        qv_scope_create_hints_t hints,
        qv_scope_t **child
    );
    /** Destructor */
    ~qv_scope(void);
    /** Destroys a scope. */
    static void
    destroy(
        qv_scope_t **scope
    );
    /** Destroys scopes created by thread_split*. */
    static void
    thread_destroy(
        qv_scope_t ***kscopes,
        uint_t k
    );
    /** Returns a pointer to the scope's underlying group. */
    qvi_group *
    group(void) const;
    /** Returns a pointer to the scope's underlying hardware pool. */
    qvi_hwpool *
    hwpool(void) const;
    /** Returns the scope's group size. */
    int
    group_size(void) const;
    /** Returns the caller's group rank in the provided scope. */
    int
    group_rank(void) const;
    /** Performs a barrier across all members in the scope. */
    int
    group_barrier(void);
    /** Returns the number of hardware objects in the provided scope. */
    int
    hwpool_nobjects(
        qv_hw_obj_type_t obj
    ) const;
    /**
     * Returns the device ID string according to the ID
     * type for the requested device type and index.
     */
    int
    device_id(
        qv_hw_obj_type_t dev_type,
        int dev_index,
        qv_device_id_type_t format,
        char **result
    ) const;

    int
    bind_push(void);

    int
    bind_pop(void);

    int
    bind_string(
        qv_bind_string_flags_t flags,
        char **result
    );

    int
    thread_split(
        uint_t npieces,
        int *kcolors,
        uint_t k,
        qv_hw_obj_type_t maybe_obj_type,
        qv_scope_t ***kchildren
    );

    int
    thread_split_at(
        qv_hw_obj_type_t type,
        int *kcolors,
        uint_t k,
        qv_scope_t ***kchildren
    );

    int
    split(
        int ncolors,
        int color,
        qv_hw_obj_type_t maybe_obj_type,
        qv_scope_t **child
    );

    int
    split_at(
        qv_hw_obj_type_t type,
        int group_id,
        qv_scope_t **child
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

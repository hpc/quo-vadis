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

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-hwpool.h"

struct qv_scope_s {
private:
    /** Task group associated with this scope instance. */
    qvi_group_t *m_group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_s *m_hwpool = nullptr;
public:
    /** Constructor */
    qv_scope_s(void) = delete;
    /** Constructor */
    qv_scope_s(
        qvi_group_t *group,
        qvi_hwpool_s *hwpool
    );
    /** Destructor */
    ~qv_scope_s(void);
    /** Destroys a scope. */
    static void
    del(
        qv_scope_t **scope
    );
    /** Frees scope resources and container created by thsplit*. */
    static void
    thdel(
        qv_scope_t ***kscopes,
        uint_t k
    );
    /** Takes the provided group and creates a new intrinsic scope from it. */
    static int
    make_intrinsic(
        qvi_group_t *group,
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
    /** Returns a pointer to the scope's underlying group. */
    qvi_group_t *
    group(void) const;
    /** Returns a pointer to the scope's underlying hardware pool. */
    qvi_hwpool_s *
    hwpool(void) const;
    /** Returns the scope's group size. */
    int
    group_size(void) const;
    /** Returns the caller's group rank in the provided scope. */
    int
    group_rank(void) const;
    /** Returns the number of hardware objects in the provided scope. */
    int
    nobjects(
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
    /** Performs a scope-level barrier. */
    int
    barrier(void);

    int
    bind_push(void);

    int
    bind_pop(void);

    int
    bind_string(
        qv_bind_string_format_t format,
        char **result
    );

    int
    thsplit(
        uint_t npieces,
        int *kcolors,
        uint_t k,
        qv_hw_obj_type_t maybe_obj_type,
        qv_scope_t ***kchildren
    );

    int
    thsplit_at(
        qv_hw_obj_type_t type,
        int *kgroup_ids,
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

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
 * @file qvi-scope.cc
 */

#include "qvi-common.h"
#include "qvi-scope.h"

// Type definition
struct qv_scope_s {
    /** Bitmap associated with this scope instance. */
    hwloc_bitmap_t bitmap = nullptr;
};

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t;
    if (!scope) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_bitmap_alloc(&iscope->bitmap);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) qvi_scope_free(&iscope);
    *scope = iscope;
    return rc;
}

void
qvi_scope_free(
    qv_scope_t **scope
) {
    qv_scope_t *iscope = *scope;
    if (!iscope) return;
    hwloc_bitmap_free(iscope->bitmap);
    delete iscope;
    *scope = nullptr;
}

hwloc_bitmap_t
qvi_scope_bitmap_get(
    qv_scope_t *scope
) {
    return scope->bitmap;
}

int
qvi_scope_bitmap_set(
    qv_scope_t *scope,
    hwloc_const_bitmap_t bitmap
) {
    if (hwloc_bitmap_copy(scope->bitmap, bitmap) != 0) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

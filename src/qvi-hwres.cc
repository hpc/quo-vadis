/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwres.cc
 *
 * Hardware Resource
 */

#include "qvi-common.h"
#include "qvi-hwres.h"

struct qvi_hwres_s {
    /** The resource bitmap. */
    hwloc_bitmap_t rmap = nullptr;
    /** The resource type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_MACHINE;
};

int
qvi_hwres_new(
    qvi_hwres_t **hwres,
    qv_hw_obj_type_t type
) {
    int rc = QV_SUCCESS;

    qvi_hwres_t *ihwres = qvi_new qvi_hwres_t;
    if (!ihwres) {
        rc = QV_ERR_OOR;
        goto out;
    }
    rc = qvi_hwloc_bitmap_calloc(&ihwres->rmap);
    if (rc != QV_SUCCESS) goto out;

    ihwres->type = type;
out:
    if (rc != QV_SUCCESS) qvi_hwres_free(&ihwres);
    *hwres = ihwres;
    return rc;
}

void
qvi_hwres_free(
    qvi_hwres_t **hwres
) {
    if (!hwres) return;
    qvi_hwres_t *ihwres = *hwres;
    if (!ihwres) goto out;
    hwloc_bitmap_free(ihwres->rmap);
    delete ihwres;
out:
    *hwres = nullptr;
}

qv_hw_obj_type_t
qvi_hwres_type(
    qvi_hwres_t *hwres
) {
    return hwres->type;
}

int
qvi_hwres_add_from_bitmap(
    qvi_hwres_t *hwres,
    hwloc_const_cpuset_t rmap
) {
    int rc = hwloc_bitmap_or(hwres->rmap, hwres->rmap, rmap);
    if (rc != 0) return QV_ERR_HWLOC;
    return QV_SUCCESS;
}

int
qvi_hwres_add(
    qvi_hwres_t *to,
    qvi_hwres_t *from
) {
    return qvi_hwres_add_from_bitmap(to, from->rmap);
}

int
qvi_hwres_remove_from_bitmap(
    qvi_hwres_t *hwres,
    hwloc_const_cpuset_t rmap
) {
    int rc = hwloc_bitmap_andnot(hwres->rmap, hwres->rmap, rmap);
    if (rc != 0) return QV_ERR_HWLOC;
    return QV_SUCCESS;
}

int
qvi_hwres_remove(
    qvi_hwres_t *from,
    qvi_hwres_t *what
) {
    return qvi_hwres_remove_from_bitmap(from, what->rmap);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qv-hwloc.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-hwloc.h"
#include "private/qvi-logger.h"

// Type definition
struct qvi_hwloc_s {
    /** The cached node topology. */
    hwloc_topology_t topo;
};

int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    qvi_hwloc_t *ihwl = (qvi_hwloc_t *)calloc(1, sizeof(*ihwl));
    if (!ihwl) {
        qvi_log_error("calloc() failed");
        return QV_ERR_OOR;
    }
    *hwl = ihwl;
    return QV_SUCCESS;
}

void
qvi_hwloc_destruct(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return;

    hwloc_topology_destroy(hwl->topo);
    free(hwl);
}

int
qvi_hwloc_init(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = hwloc_topology_init(&hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_init() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_load(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    /* Set flags that influence hwloc's behavior. */
    static const unsigned int flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
    int rc = hwloc_topology_set_flags(hwl->topo, flags);
    if (rc != 0) {
        ers = "hwloc_topology_set_flags() failed";
        goto out;
    }

    rc = hwloc_topology_set_all_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_ALL
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_all_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_set_io_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_io_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_load(hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_load() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_bitmap_asprintf(
    char **result,
    hwloc_const_bitmap_t bitmap
) {
    if (!bitmap || !result) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    // Caller is responsible for freeing returned resources.
    char *iresult = nullptr;
    (void)hwloc_bitmap_asprintf(&iresult, bitmap);
    if (!iresult) {
        qvi_log_error("hwloc_bitmap_asprintf() failed");
        rc = QV_ERR_OOR;
    }
    *result = iresult;
    return rc;
}

int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
) {
    if (!hwl || !out_bitmap) return QV_ERR_INVLD_ARG;

    int qrc = QV_SUCCESS, rc = 0;

    hwloc_bitmap_t cur_bind = hwloc_bitmap_alloc();
    if (!cur_bind) {
        qvi_log_error("hwloc_bitmap_alloc() failed");
        qrc = QV_ERR_OOR;
        goto out;
    }
    // TODO(skg) Add another routine to also support getting TIDs.
    rc = hwloc_get_proc_cpubind(
            hwl->topo,
            who,
            cur_bind,
            HWLOC_CPUBIND_PROCESS
    );
    if (rc) {
        char const *ers = "hwloc_get_proc_cpubind() failed";
        qvi_log_error("{} with rc={}", ers, rc);
        qrc = QV_ERR_HWLOC;
        goto out;
    }
    *out_bitmap = cur_bind;
out:
    /* Cleanup on failure */
    if (qrc != QV_SUCCESS) {
        if (cur_bind) hwloc_bitmap_free(cur_bind);
        *out_bitmap = nullptr;
    }
    return qrc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

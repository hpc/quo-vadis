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
 * @file qvi-hwloc.cc
 */

#include "qvi-common.h"
#include "qvi-hwloc.h"

typedef enum qvi_hwloc_task_xop_obj_e {
    QVI_HWLOC_TASK_INTERSECTS_OBJ = 0,
    QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ
} qvi_hwloc_task_xop_obj_t;

static inline int
obj_type_from_external(
    qv_hwloc_obj_type_t external,
    hwloc_obj_type_t *internal
) {
    switch(external) {
        case(QV_HWLOC_OBJ_MACHINE):
            *internal = HWLOC_OBJ_MACHINE;
            break;
        case(QV_HWLOC_OBJ_PACKAGE):
            *internal = HWLOC_OBJ_PACKAGE;
            break;
        case(QV_HWLOC_OBJ_CORE):
            *internal = HWLOC_OBJ_CORE;
            break;
        case(QV_HWLOC_OBJ_PU):
            *internal = HWLOC_OBJ_PU;
            break;
        case(QV_HWLOC_OBJ_L1CACHE):
            *internal = HWLOC_OBJ_L1CACHE;
            break;
        case(QV_HWLOC_OBJ_L2CACHE):
            *internal = HWLOC_OBJ_L2CACHE;
            break;
        case(QV_HWLOC_OBJ_L3CACHE):
            *internal = HWLOC_OBJ_L3CACHE;
            break;
        case(QV_HWLOC_OBJ_L4CACHE):
            *internal = HWLOC_OBJ_L4CACHE;
            break;
        case(QV_HWLOC_OBJ_L5CACHE):
            *internal = HWLOC_OBJ_L5CACHE;
            break;
        case(QV_HWLOC_OBJ_NUMANODE):
            *internal = HWLOC_OBJ_NUMANODE;
            break;
        case(QV_HWLOC_OBJ_OS_DEVICE):
            *internal = HWLOC_OBJ_OS_DEVICE;
            break;
        default:
            return QV_ERR_INVLD_ARG;
    }
    return QV_SUCCESS;
}

static int
obj_get_by_type(
    qvi_hwloc_t *hwloc,
    qv_hwloc_obj_type_t type,
    unsigned type_index,
    hwloc_obj_t *out_obj
) {
    hwloc_obj_type_t real_type;
    int rc = obj_type_from_external(type, &real_type);
    if (rc != QV_SUCCESS) return rc;

    *out_obj = hwloc_get_obj_by_type(hwloc->topo, real_type, type_index);
    if (!*out_obj) {
        // There are a couple of reasons why target_obj may be NULL. If this
        // ever happens and the specified type and obj index are valid, then
        // improve this code.
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

static int
obj_type_depth(
    qvi_hwloc_t *hwloc,
    qv_hwloc_obj_type_t type,
    int *depth
) {
    hwloc_obj_type_t real_type;
    int rc = obj_type_from_external(type, &real_type);
    if (rc != QV_SUCCESS) return rc;

    int d = hwloc_get_type_depth(hwloc->topo, real_type);
    if (d == HWLOC_TYPE_DEPTH_UNKNOWN) {
        *depth = 0;
    }
    *depth = d;
    return QV_SUCCESS;
}

int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_hwloc_t *ihwl = qvi_new qvi_hwloc_t;
    if (!ihwl) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (ers) {
        qvi_hwloc_destruct(&ihwl);
    }
    *hwl = ihwl;
    return QV_SUCCESS;
}

void
qvi_hwloc_destruct(
    qvi_hwloc_t **hwl
) {
    qvi_hwloc_t *ihwl = *hwl;
    if (!ihwl) return;
    if (ihwl->topo) hwloc_topology_destroy(ihwl->topo);
    delete ihwl;
    *hwl = nullptr;
}

/**
 *
 */
static int
topology_init(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    cstr ers = nullptr;

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

    cstr ers = nullptr;

    int rc = topology_init(hwl);
    if (rc != QV_SUCCESS) {
        return rc;
    }
    // Set flags that influence hwloc's behavior.
    static const unsigned int flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
    rc = hwloc_topology_set_flags(hwl->topo, flags);
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
qvi_hwloc_get_nobjs_by_type(
   qvi_hwloc_t *hwloc,
   qv_hwloc_obj_type_t target_type,
   int *out_nobjs
) {
    if (!hwloc || !out_nobjs) return QV_ERR_INVLD_ARG;

    int depth;
    int rc = obj_type_depth(hwloc, target_type, &depth);
    if (rc != QV_SUCCESS) return rc;

    *out_nobjs = hwloc_get_nbobjs_by_depth(hwloc->topo, depth);
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
        cstr ers = "hwloc_get_proc_cpubind() failed";
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

/**
 *
 */
static int
task_obj_xop_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hwloc_obj_type_t type,
    pid_t who,
    unsigned type_index,
    qvi_hwloc_task_xop_obj_t opid,
    int *result
) {
    hwloc_obj_t obj;
    int rc = obj_get_by_type(hwl, type, type_index, &obj);
    if (rc != QV_SUCCESS) return rc;

    hwloc_cpuset_t cur_bind;
    rc = qvi_hwloc_task_get_cpubind(hwl, who, &cur_bind);
    if (rc != QV_SUCCESS) return rc;

    switch (opid) {
        case QVI_HWLOC_TASK_INTERSECTS_OBJ: {
            *result = hwloc_bitmap_intersects(cur_bind, obj->cpuset);
            break;
        }
        case QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ: {
            *result = hwloc_bitmap_isincluded(cur_bind, obj->cpuset);
            break;
        }
    }

    hwloc_bitmap_free(cur_bind);
    return QV_SUCCESS;
}

int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hwloc_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
) {
    if (!hwl || !result) return QV_ERR_INVLD_ARG;

    return task_obj_xop_by_type_id(
        hwl,
        type,
        who,
        type_index,
        QVI_HWLOC_TASK_INTERSECTS_OBJ,
        result
    );
}

int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hwloc_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
) {
    if (!hwl || !result) return QV_ERR_INVLD_ARG;

    return task_obj_xop_by_type_id(
        hwl,
        type,
        who,
        type_index,
        QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ,
        result
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.cc
 *
 * Hardware Resource Pool
 */

// TODOs
// * Resource reference counting.
// * Need to deal with resource unavailability.
// * Split and attach devices properly.

#include "qvi-common.h"
#include "qvi-hwpool.h"
#include "qvi-hwloc.h"

using qvi_device_ids_t = std::unordered_set<qvi_device_id_t>;
using qvi_device_tab_t = std::unordered_map<qv_hw_obj_type_t, qvi_device_ids_t>;

struct qvi_hwpool_s {
    /** The cpuset of this resource pool. */
    hwloc_bitmap_t cpuset = nullptr;
    /** Device table */
    qvi_device_tab_t *devtab = nullptr;
};

int
qvi_hwpool_new(
    qvi_hwpool_t **rpool
) {
    int rc = QV_SUCCESS;
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();

    qvi_hwpool_t *irpool = qvi_new qvi_hwpool_t;
    if (!irpool) {
        rc = QV_ERR_OOR;
        goto out;
    }

    irpool->devtab = qvi_new qvi_device_tab_t;
    if (!irpool) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_bitmap_calloc(&irpool->cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Initialize key space so we can catch some errors later.
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        const qv_hw_obj_type_t type = devts[i];
        // insert().second returns whether or not item insertion took place. If
        // true, this is a unique device type.
        const bool itp = irpool->devtab->insert(
            {type, qvi_device_ids_t()}
        ).second;
        if (!itp) {
            qvi_log_debug("Duplicate device type found: {}", type);
            rc = QV_ERR_INTERNAL;
            break;
        }
    }
out:
    if (rc != QV_SUCCESS) qvi_hwpool_free(&irpool);
    *rpool = irpool;
    return rc;
}

int
qvi_hwpool_new_from_line(
    qvi_line_hwpool_t *line,
    qvi_hwpool_t **rpool
) {
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();

    qvi_hwpool_t *irpool = nullptr;
    int rc = qvi_hwpool_new(&irpool);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwloc_bitmap_copy(line->cpuset, irpool->cpuset);
    if (rc != QV_SUCCESS) goto out;

    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        // Get array of device IDs.
        const qvi_device_id_t *dids = line->device_tab[i];
        for (int j = 0; dids[j] != qvi_line_hwpool_devid_last; ++j) {
            rc = qvi_hwpool_add_device(irpool, devts[i], dids[j]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) break;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&irpool);
    }
    *rpool = irpool;
    return rc;
}

int
qvi_hwpool_new_line_from_hwpool(
    const qvi_hwpool_t *rpool,
    qvi_line_hwpool_t **line
) {
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    const int ndevts = qvi_hwloc_n_supported_devices();

    qvi_line_hwpool_t *iline = nullptr;
    int rc = qvi_line_hwpool_new(&iline);
    if (rc != QV_SUCCESS) goto out;
    // Initialize and copy the cpuset.
    rc = qvi_hwloc_bitmap_calloc(&iline->cpuset);
    if (rc != QV_SUCCESS) goto out;
    rc = qvi_hwloc_bitmap_copy(rpool->cpuset, iline->cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Initialize and fill in the device information.
    iline->device_tab = (int **)calloc(ndevts, sizeof(int *));
    if (!iline->device_tab) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Iterate over the device types and populate their ID arrays.
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        const qv_hw_obj_type_t type = devts[i];
        auto ftype = rpool->devtab->find(type);
        // Make sure the data are sensible.
        if (ftype == rpool->devtab->end()) {
            qvi_log_debug("Key not found: {}", type);
            rc = QV_ERR_INTERNAL;
            break;
        }
        // Allocate the device ID array. Add one spot for the sentinel value.
        const int nids = ftype->second.size() + 1;
        iline->device_tab[i] = (int *)calloc(nids, sizeof(int));
        if (!iline->device_tab[i]) {
            rc = QV_ERR_OOR;
            break;
        }
        // Initialize the ID array. The order doesn't matter for the IDs, as
        // they will be inserted in their 'natural' order in the non-line type.
        int j = 0;
        for (const auto id : ftype->second) {
            iline->device_tab[i][j++] = id;
        }
        // Cap off the array with the sentinel value.
        iline->device_tab[i][j] = qvi_line_hwpool_devid_last;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_line_hwpool_free(&iline);
    }
    *line = iline;
    return rc;
}

void
qvi_hwpool_free(
    qvi_hwpool_t **rpool
) {
    if (!rpool) return;
    qvi_hwpool_t *irpool = *rpool;
    if (!irpool) goto out;
    if (irpool->devtab) {
        delete irpool->devtab;
    }
    if (irpool->cpuset) {
        hwloc_bitmap_free(irpool->cpuset);
    }
    delete irpool;
out:
    *rpool = nullptr;
}

int
qvi_hwpool_init(
    qvi_hwpool_t *rpool,
    hwloc_const_bitmap_t cpuset
) {
    return qvi_hwloc_bitmap_copy(cpuset, rpool->cpuset);
}

int
qvi_hwpool_add_device(
    qvi_hwpool_t *rpool,
    qv_hw_obj_type_t type,
    qvi_device_id_t id
) {
    auto ftype = rpool->devtab->find(type);
    if (ftype == rpool->devtab->end()) {
        qvi_log_debug("Key not found: {}", type);
        return QV_ERR_INTERNAL;
    }
    // insert().second returns whether or not item insertion took place. If
    // true, this is a unique ID.
    bool itp = ftype->second.insert(id).second;
    if (!itp) {
        qvi_log_debug("Duplicate ID ({}) found for type ({})", id, type);
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *rpool
) {
    return rpool->cpuset;
}

/**
 *
 */
static int
split_cpuset_by_group(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int n,
    int group_id,
    hwloc_cpuset_t *result
) {
    int npus = 0;
    int rc = qvi_hwloc_get_nobjs_in_cpuset(
        hwl,
        QV_HW_OBJ_PU,
        cpuset,
        &npus
    );
    if (rc != QV_SUCCESS) return rc;
    // We use PUs to split resources. Each set bit represents a PU. The number
    // of bits set represents the number of PUs present on the system. The
    // least-significant (right-most) bit represents logical ID 0.
    int pu_depth;
    rc = qvi_hwloc_obj_type_depth(
        hwl,
        QV_HW_OBJ_PU,
        &pu_depth
    );
    if (rc != QV_SUCCESS) return rc;

    const int chunk = npus / n;
    // This happens when n > npus. We can't support that split.
    if (chunk == 0) {
        return QV_ERR_SPLIT;
    }
    // Group IDs must be < n: 0, 1, ... , n-1.
    if (group_id >= n) {
        return QV_ERR_SPLIT;
    }
    // Calculate base and extent of split.
    const int base = chunk * group_id;
    const int extent = base + chunk;
    // Allocate and zero-out a new bitmap that will encode the split.
    hwloc_bitmap_t bitm;
    rc = qvi_hwloc_bitmap_calloc(&bitm);
    if (rc != QV_SUCCESS) return rc;
    for (int i = base; i < extent; ++i) {
        hwloc_obj_t dobj;
        rc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            hwl,
            cpuset,
            pu_depth,
            i,
            &dobj
        );
        if (rc != QV_SUCCESS) goto out;

        rc = hwloc_bitmap_or(bitm, bitm, dobj->cpuset);
        if (rc != 0) {
            rc = QV_ERR_HWLOC;
            goto out;
        }
    }
out:
    if (rc != QV_SUCCESS) {
        hwloc_bitmap_free(bitm);
        bitm = nullptr;
    }
    *result = bitm;
    return rc;
}

int
qvi_hwpool_obtain_by_cpuset(
    qvi_hwpool_t *pool,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_t **opool
) {
    qvi_hwpool_t *ipool = nullptr;
    int rc = qvi_hwpool_new(&ipool);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_hwpool_init(ipool, cpuset);
    // TODO(skg) Do the device stuff.
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&ipool);
    }
    *opool = ipool;
    return rc;
}

int
qvi_hwpool_obtain_split_by_group(
    qvi_hwloc_t *hwloc,
    qvi_hwpool_t *pool,
    int n,
    int group_id,
    qvi_hwpool_t **opool
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = split_cpuset_by_group(
        hwloc,
        pool->cpuset,
        n,
        group_id,
        &cpuset
    );
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_hwpool_obtain_by_cpuset(
        pool,
        cpuset,
        opool
    );
    hwloc_bitmap_free(cpuset);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rmi-txrx.cc
 *
 * RMI line types and functions for sending and receiving data.
 */

#include "qvi-common.h"
#include "qvi-rmi-txrx.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-utils.h"

int
qvi_rmi_config_new(
    qvi_rmi_config_t **config
) {
    int rc = QV_SUCCESS;
    qvi_rmi_config_t *ic = (qvi_rmi_config_t *)calloc(1, sizeof(*ic));
    if (!ic) rc = QV_ERR_OOR;
    if (rc != QV_SUCCESS) qvi_rmi_config_free(&ic);
    *config = ic;
    return rc;
}

void
qvi_rmi_config_free(
    qvi_rmi_config_t **config
) {
    if (!config) return;
    qvi_rmi_config_t *ic = *config;
    if (!ic) goto out;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    free(ic);
out:
    *config = nullptr;
}

int
qvi_rmi_config_cp(
    qvi_rmi_config_t *from,
    qvi_rmi_config_t *to
) {
    to->hwloc = from->hwloc;
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) return QV_ERR_OOR;
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) return QV_ERR_OOR;
    return QV_SUCCESS;
}

int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t *buff
) {
    int rc = qvi_bbuff_rmi_sprintf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        config->url,
        config->hwtopo_path
    );
    return rc;
}

int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t **config
) {
    int rc = qvi_rmi_config_new(config);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_data_rmi_sscanf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
    return rc;
}

int
qvi_rmi_hwresources_new(
    qvi_rmi_hwresources_t **hwres
) {
    int rc = QV_SUCCESS;

    qvi_rmi_hwresources_t *ir = (qvi_rmi_hwresources_t *)calloc(1, sizeof(*ir));
    if (!ir) {
        rc = QV_ERR_OOR;
        goto out;
    }

    for (int rtype = 0; rtype < qvi_rmi_hwresources_t::LAST; ++rtype) {
        rc = qvi_hwloc_bitmap_calloc(&ir->rtab[rtype].rmap);
        if (rc != QV_SUCCESS) break;

        switch (rtype) {
            case qvi_rmi_hwresources_t::CPU:
                ir->rtab[rtype].type = QV_HW_OBJ_MACHINE;
                break;
            case qvi_rmi_hwresources_t::GPU:
                ir->rtab[rtype].type = QV_HW_OBJ_GPU;
                break;
            default:
                rc = QV_ERR_NOT_SUPPORTED;
                break;
        }
    }
out:
    if (rc != QV_SUCCESS) qvi_rmi_hwresources_free(&ir);
    *hwres = ir;
    return rc;
}

void
qvi_rmi_hwresources_free(
    qvi_rmi_hwresources_t **hwres
) {
    if (!hwres) return;
    qvi_rmi_hwresources_t *ir = *hwres;
    if (!ir) goto out;
    for (int rtype = 0; rtype < qvi_rmi_hwresources_t::LAST; ++rtype) {
        hwloc_bitmap_free(ir->rtab[rtype].rmap);
    }
    free(ir);
out:
    *hwres = nullptr;
}

int
qvi_rmi_hwresources_cp(
    qvi_rmi_hwresources_t *from,
    qvi_rmi_hwresources_t *to
) {
    int rc = QV_SUCCESS;

    for (int rtype = 0; rtype < qvi_rmi_hwresources_t::LAST; ++rtype) {
        to->rtab[rtype].type = from->rtab[rtype].type;
        rc = qvi_hwloc_bitmap_copy(
            from->rtab[rtype].rmap, to->rtab[rtype].rmap
        );
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

int
qvi_rmi_hwresources_pack(
    qvi_rmi_hwresources_t *config,
    qvi_bbuff_t *buff
) {
#if 0
    int rc = qvi_bbuff_rmi_sprintf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        config->url,
        config->hwtopo_path
    );
    return rc;
#endif
    return QV_SUCCESS;
}

int
qvi_rmi_hwresources_unpack(
    void *buff,
    qvi_rmi_hwresources_t **config
) {
#if 0
    int rc = qvi_rmi_hwresources_new(config);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_data_rmi_sscanf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
    return rc;
#endif
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

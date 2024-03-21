/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-line.cc
 *
 * Line types and functions for sending and receiving data over the network.
 * More generally, they are types that can be easily serialized.
 */

#include "qvi-line.h"
#include "qvi-bbuff-rmi.h"

int
qvi_line_config_new(
    qvi_line_config_t **config
) {
    int rc = QV_SUCCESS;
    qvi_line_config_t *ic = qvi_new qvi_line_config_t();
    if (!ic) rc = QV_ERR_OOR;
    // Do minimal initialization here because other routines will do the rest.
    if (rc != QV_SUCCESS) {
        qvi_line_config_free(&ic);
    }
    *config = ic;
    return rc;
}

void
qvi_line_config_free(
    qvi_line_config_t **config
) {
    if (!config) return;
    qvi_line_config_t *ic = *config;
    if (!ic) goto out;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    delete ic;
out:
    *config = nullptr;
}

int
qvi_line_config_cp(
    qvi_line_config_t *from,
    qvi_line_config_t *to
) {
    to->hwloc = from->hwloc;
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) {
        to->url = nullptr;
        return QV_ERR_OOR;
    }
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) {
        to->hwtopo_path = nullptr;
        return QV_ERR_OOR;
    }
    return QV_SUCCESS;
}

int
qvi_line_config_pack(
    qvi_line_config_t *config,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_pack(
        buff, config->url,
        config->hwtopo_path
    );
}

int
qvi_line_config_unpack(
    void *buff,
    qvi_line_config_t **config
) {
    int rc = qvi_line_config_new(config);
    if (rc != QV_SUCCESS) return rc;

    return qvi_bbuff_rmi_unpack(
        buff, &(*config)->url,
        &(*config)->hwtopo_path
    );
}

void
qvi_line_devinfo_free(
    qvi_line_devinfo_t *devinfo
) {
    if (!devinfo) return;
    qvi_hwloc_bitmap_free(&devinfo->affinity);
    free(devinfo->pci_bus_id);
    free(devinfo->uuid);
}

int
qvi_line_hwpool_new(
    qvi_line_hwpool_t **hwp
) {
    int rc = QV_SUCCESS;

    qvi_line_hwpool_t *ihwp = qvi_new qvi_line_hwpool_t();
    if (!ihwp) rc = QV_ERR_OOR;
    // Do minimal initialization here because other routines will do the rest.
    if (rc != QV_SUCCESS) qvi_line_hwpool_free(&ihwp);
    *hwp = ihwp;
    return rc;
}

void
qvi_line_hwpool_free(
    qvi_line_hwpool_t **hwp
) {
    if (!hwp) return;
    qvi_line_hwpool_t *ihwp = *hwp;
    if (!ihwp) goto out;
    qvi_hwloc_bitmap_free(&ihwp->cpuset);
    if (ihwp->devinfos) {
        for (int i = 0; i < ihwp->ndevinfos; ++i) {
            qvi_line_devinfo_free(&ihwp->devinfos[i]);
        }
        delete[] ihwp->devinfos;
    }
    delete ihwp;
out:
    *hwp = nullptr;
}

int
qvi_line_hwpool_pack(
    qvi_line_hwpool_t *hwp,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_pack(buff, hwp);
}

int
qvi_line_hwpool_unpack(
    void *buff,
    qvi_line_hwpool_t **hwp
) {
    return qvi_bbuff_rmi_unpack(buff, hwp);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

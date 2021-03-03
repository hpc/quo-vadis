/*
 * Copyright (c)      2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-config.cc
 */

#include "qvi-common.h"
#include "qvi-config.h"
#include "qvi-bbuff.h"

int
qvi_config_rmi_new(
    qvi_config_rmi_t **config
) {
    int rc = QV_SUCCESS;
    qvi_config_rmi_t *ic = (qvi_config_rmi_t *)calloc(1, sizeof(*ic));
    if (!ic) rc = QV_ERR_OOR;
    if (rc != QV_SUCCESS) qvi_config_rmi_free(&ic);
    *config = ic;
    return rc;
}

void
qvi_config_rmi_free(
    qvi_config_rmi_t **config
) {
    qvi_config_rmi_t *ic = *config;
    if (!ic) return;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    free(ic);
    *config = nullptr;
}

int
qvi_config_rmi_cp(
    qvi_config_rmi_t *from,
    qvi_config_rmi_t *to
) {
    to->hwloc = from->hwloc;
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) return QV_ERR_OOR;
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) return QV_ERR_OOR;
    return QV_SUCCESS;
}

int
qvi_config_rmi_pack(
    qvi_config_rmi_t *config,
    qvi_bbuff_t *buff
) {
    int rc = qvi_bbuff_asprintf(
        buff,
        QVI_CONFIG_RMI_PICTURE,
        config->url,
        config->hwtopo_path
    );
    return rc;
}

int
qvi_config_rmi_unpack(
    void *buff,
    qvi_config_rmi_t **config
) {
    int rc = qvi_config_rmi_new(config);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_data_sscanf(
        buff,
        QVI_CONFIG_RMI_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

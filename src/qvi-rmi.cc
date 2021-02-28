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
 * @file qvi-rmi.cc
 *
 * Resource Management and Inquiry
 */

#include "qvi-common.h"
#include "qvi-rmi.h"
#include "qvi-rpc.h"

// Keep in sync with qvi_rmi_config_t structure.
#define QVI_RMI_CONFIG_PICTURE "ss"

struct qvi_rmi_server_s {
    qvi_rmi_config_t *config = nullptr;
    qvi_rpc_server_t *rpcserv = nullptr;
};

struct qvi_rmi_client_s {
    qvi_rmi_config_t *config = nullptr;
    qvi_rpc_client_t *rcpcli = nullptr;
};

static int
rmi_config_cp(
    qvi_rmi_config_t *from,
    qvi_rmi_config_t *to
) {
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) return QV_ERR_OOR;
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) return QV_ERR_OOR;
    return QV_SUCCESS;
}

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
    qvi_rmi_config_t *ic = *config;
    if (!ic) return;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    free(ic);
    *config = nullptr;
}

int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t **packed
) {
    qvi_bbuff_t *ibuff;
    int rc = qvi_bbuff_new(&ibuff);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_bbuff_asprintf(
        ibuff,
        QVI_RMI_CONFIG_PICTURE,
        config->url,
        config->hwtopo_path
    );
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&ibuff);
    }
    *packed = ibuff;
    return rc;
}

int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t **config
) {
    int rc = qvi_rmi_config_new(config);
    if (rc != QV_SUCCESS) return rc;
    rc = qvi_data_sscanf(
        buff,
        QVI_RMI_CONFIG_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
    return rc;
}

int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_rmi_server_t *iserver = qvi_new qvi_rmi_server_t;
    if (!iserver) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_rmi_config_new(&iserver->config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_config_new() failed";
        goto out;
    }

    rc = qvi_rpc_server_new(&iserver->rpcserv);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_new() failed";
        goto out;
    }

out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_server_free(&iserver);
    }
    *server = iserver;
    return rc;
}

void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
) {
    qvi_rmi_server_t *iserver = *server;
    if (!iserver) return;
    qvi_rpc_server_free(&iserver->rpcserv);
    qvi_rmi_config_free(&iserver->config);
    delete iserver;
    *server = nullptr;
}

int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_t *config
) {
    return rmi_config_cp(config, server->config);
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    rc = qvi_rpc_server_start(
        server->rpcserv,
        server->config->url
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_start() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

int
qvi_rmi_client_new(
    qvi_rmi_client_t **client
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qvi_rmi_client_t *icli = qvi_new qvi_rmi_client_t;
    if (!icli) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_rpc_client_new(&icli->rcpcli);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_new() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_client_free(&icli);
    }
    *client = icli;
    return rc;
}

void
qvi_rmi_client_free(
    qvi_rmi_client_t **client
) {
    qvi_rmi_client_t *iclient = *client;
    if (!iclient) return;
    qvi_rpc_client_free(&iclient->rcpcli);
    delete iclient;
    *client = nullptr;
}

int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
) {
    return qvi_rpc_client_connect(client->rcpcli, url);
}

int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
) {
    int rc = QV_SUCCESS;
    cstr ers = nullptr;
    hwloc_bitmap_t bitmap = hwloc_bitmap_alloc();

    rc = qvi_rpc_req(
        client->rcpcli,
        RPC_FID_TASK_GET_CPUBIND,
        "i",
        who
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_req() failed";
        goto out;
    }

    int rpcrc;
    char *bitmaps;
    rc = qvi_rpc_rep(
        client->rcpcli,
        "is",
        &rpcrc,
        &bitmaps
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_rep() failed";
        goto out;
    }
    hwloc_bitmap_sscanf(bitmap, bitmaps);
    *out_bitmap = bitmap;
    free(bitmaps);
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

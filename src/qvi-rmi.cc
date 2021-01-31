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
 * @file qvi-rmi.cc
 *
 * Resource Management and Inquiry
 */

#include "private/qvi-common.h"
#include "private/qvi-rmi.h"
#include "private/qvi-log.h"

struct qvi_rmi_server_s {
    qvi_rpc_server_t *rpcserv;
};

struct qvi_rmi_client_s {
    qvi_rpc_client_t *rcpcli;
};

int
qvi_rmi_server_construct(
    qvi_rmi_server_t **server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rmi_server_t *iserver = (qvi_rmi_server_t *)calloc(1, sizeof(*iserver));
    if (!iserver) {
        qvi_log_error("calloc() failed");
        return QV_ERR_OOR;
    }

    rc = qvi_rpc_server_construct(&iserver->rpcserv);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_server_destruct(iserver);
        *server = nullptr;
        return rc;
    }
    *server = iserver;
    return QV_SUCCESS;
}

void
qvi_rmi_server_destruct(
    qvi_rmi_server_t *server
) {
    if (!server) return;

    qvi_rpc_server_destruct(server->rpcserv);
    free(server);
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    const char *url
) {
    if (!server) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = qvi_rpc_server_start(server->rpcserv, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_start() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_server_destruct(server);
    }
    return rc;
}

int
qvi_rmi_client_construct(
    qvi_rmi_client_t **client
) {
    if (!client) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rmi_client_t *icli = (qvi_rmi_client_t *)calloc(1, sizeof(*icli));
    if (!icli) {
        qvi_log_error("calloc() failed");
        return QV_ERR_OOR;
    }

    rc = qvi_rpc_client_construct(&icli->rcpcli);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_client_destruct(icli);
        *client = nullptr;
        return rc;
    }
    *client = icli;
    return QV_SUCCESS;
}

void
qvi_rmi_client_destruct(
    qvi_rmi_client_t *client
) {
    if (!client) return;

    qvi_rpc_client_destruct(client->rcpcli);
    free(client);
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
    if (!client) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    hwloc_bitmap_t bitmap = hwloc_bitmap_alloc();

    qvi_rpc_argv_t args = 0;
    qvi_rpc_argv_pack(&args, who, out_bitmap);

    rc = qvi_rpc_client_req(
        client->rcpcli,
        QV_TASK_GET_CPUBIND,
        args,
        who,
        out_bitmap
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_req() failed";
        goto out;
    }

    qvi_rpc_fun_data_t fun_data;
    rc = qvi_rpc_client_rep(
        client->rcpcli,
        &fun_data
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_rep() failed";
        goto out;
    }
    hwloc_bitmap_sscanf(bitmap, fun_data.bitm_args[0]);
    *out_bitmap = bitmap;
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

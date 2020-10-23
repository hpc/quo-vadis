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
 * @file rmi.cc
 *
 * Resource Management and Inquiry
 */

#include "private/common.h"
#include "private/rmi.h"
#include "private/rpc.h"
#include "private/logger.h"

#include "quo-vadis/hw-loc.h"

struct qvi_rmi_server_s {
    qvi_rpc_server_t *rpcserv;
    qv_hwloc_t *hwloc;
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
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    rc = qv_hwloc_construct(&iserver->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_construct() failed";
        goto out;
    }

    rc = qvi_rpc_server_construct(&iserver->rpcserv);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_construct() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
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

    qv_hwloc_destruct(server->hwloc);
    qvi_rpc_server_destruct(server->rpcserv);
    free(server);
}

/**
 *
 */
static int
server_hwloc_init(
    qvi_rmi_server_t *server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = qv_hwloc_init(server->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_init() failed";
        goto out;
    }

    rc = qv_hwloc_topo_load(server->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_topo_load() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    const char *url
) {
    if (!server) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    // TODO(skg) Get this value by some nice means.
    const uint16_t qdepth = 10;
    rc = qvi_rpc_server_start(server->rpcserv, url, qdepth);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_start() failed";
        goto out;
    }

    rc = server_hwloc_init(server);
    if (rc != QV_SUCCESS) {
        ers = "server_hwloc_init() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
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
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    rc = qvi_rpc_client_construct(&icli->rcpcli);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_construct() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
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
    qv_bitmap_t *out_bitmap
) {
    if (!client) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    int a = 0, c = -505;
    char const *b = "|can you see me..?|";

    qvi_rpc_argv_t args = 0;
    qvi_rpc_argv_pack(&args, 0, a, b, c);

    rc = qvi_rpc_client_req(client->rcpcli, TASK_GET_CPUBIND, args, a, b, c);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_req() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

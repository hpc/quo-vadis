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
 * @file msg.cc
 */

#include "private/common.h"
#include "private/msg.h"
#include "private/logger.h"

#include "nng/supplemental/util/platform.h"

struct qvi_msg_server_s {
    char url[4096];
    nng_socket sock;
    int qdepth;
    // TODO(skg) Change name
    qvi_msg_t **work;
};

struct qvi_msg_client_s {
    char url[4096];
    nng_socket sock;
};

static void
server_cb(
    void *arg
) {
    qvi_msg_t *work = (qvi_msg_t *)arg;
    nng_msg *msg;
    int rv;
    uint32_t when;

    switch (work->state) {
    case qvi_msg_t::INIT:
        work->state = qvi_msg_t::RECV;
        nng_recv_aio(work->sock, work->aio);
        break;
    case qvi_msg_t::RECV:
        if ((rv = nng_aio_result(work->aio)) != 0) {
            // TODO(skg) deal with error
        }
        msg = nng_aio_get_msg(work->aio);
        if ((rv = nng_msg_trim_u32(msg, &when)) != 0) {
            // Bad message, just ignore it.
            nng_msg_free(msg);
            nng_recv_aio(work->sock, work->aio);
            return;
        }
        work->msg = msg;
        work->state = qvi_msg_t::WAIT;
        nng_sleep_aio(when, work->aio);
        break;
    case qvi_msg_t::WAIT:
        // We could add more data to the message here.
        nng_aio_set_msg(work->aio, work->msg);
        work->msg = NULL;
        work->state = qvi_msg_t::SEND;
        nng_send_aio(work->sock, work->aio);
        break;
    case qvi_msg_t::SEND:
        if ((rv = nng_aio_result(work->aio)) != 0) {
            nng_msg_free(work->msg);
            // TODO(skg) deal with error
        }
        work->state = qvi_msg_t::RECV;
        nng_recv_aio(work->sock, work->aio);
        break;
    default:
        // TODO(skg) deal with error
        break;
    }
}

static int
alloc_work(
    nng_socket sock,
    qvi_msg_t **work
) {
    qvi_msg_t *iwork;

    if ((iwork = (qvi_msg_t *)nng_alloc(sizeof(*iwork))) == NULL) {
        QVI_LOG_ERROR("nng_alloc() failed");
        return QV_ERR_OOR;
    }
    int rc = nng_aio_alloc(&iwork->aio, server_cb, iwork);
    if (rc != 0) {
        QVI_LOG_ERROR(
            "nng_aio_alloc() failed with rc={} ({})",
            rc,
            nng_strerror(rc)
        );
        return QV_ERR_MSG;
    }
    iwork->state = qvi_msg_t::INIT;
    iwork->sock  = sock;

    *work = iwork;
    return QV_SUCCESS;
}

int
qvi_msg_server_construct(
    qvi_msg_server_t **server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    qvi_msg_server_t *iserver = (qvi_msg_server_t *)calloc(1, sizeof(*iserver));
    if (!iserver) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    *server = iserver;
    return QV_SUCCESS;
}

void
qvi_msg_server_destruct(
    qvi_msg_server_t *server
) {
    if (!server) return;

    free(server);
}

int
qvi_msg_server_start(
    qvi_msg_server_t *server,
    const char *url,
    int qdepth
) {
    if (!server || !url) return QV_ERR_INVLD_ARG;

    if (qdepth < 0) {
        QVI_LOG_ERROR("Negative queue depths not supported");
        return QV_ERR_INVLD_ARG;
    }
    server->qdepth = qdepth;

    server->work = (qvi_msg_t **)calloc(
        server->qdepth,
        sizeof(*(server->work))
    );
    if (!server->work) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    // TODO(skg) Check snprintf return.
    snprintf(server->url, sizeof(server->url) - 1, "%s", url);

    char const *ers = nullptr;

    int rc = nng_rep0_open_raw(&server->sock);
    if (rc != 0) {
        ers = "nng_rep0_open_raw() failed";
        goto out;
    }

    for (int i = 0; i < server->qdepth; ++i) {
        rc = alloc_work(server->sock, &(server->work[i]));
        if (rc != QV_SUCCESS) {
            // TODO(skg) FIXME rc goto
            ers = "alloc_work() failed";
            goto out;
        }
    }

    rc = nng_listen(server->sock, server->url, NULL, 0);
    if (rc != 0) {
        ers = "nng_listen() failed";
        goto out;
    }

    for (int i = 0; i < server->qdepth; ++i) {
        // This starts them (INIT state)
        // TODO(skg) Check for errors here
        server_cb(server->work[i]);
    }
    // TODO(skg) Add proper shutdown
    while (true) {
        nng_msleep(3600000);
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_msg_client_construct(
    qvi_msg_client_t **client
) {
    if (!client) return QV_ERR_INVLD_ARG;

    qvi_msg_client_t *iclient = (qvi_msg_client_t *)calloc(1, sizeof(*iclient));
    if (!iclient) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    *client = iclient;
    return QV_SUCCESS;
}

void
qvi_msg_client_destruct(
    qvi_msg_client_t *client
) {
    if (!client) return;

    free(client);
}

int
qvi_msg_client_send(
    qvi_msg_client_t *client,
    const char *url,
    const char *msecstr
) {
    int        rv;
    nng_msg *  msg;
    nng_time   start;
    nng_time   end;
    unsigned   msec;

    msec = atoi(msecstr) * 1000;

    if ((rv = nng_req0_open(&client->sock)) != 0) {
    }

    if ((rv = nng_dial(client->sock, url, NULL, 0)) != 0) {
    }

    start = nng_clock();

    if ((rv = nng_msg_alloc(&msg, 0)) != 0) {
    }
    if ((rv = nng_msg_append_u32(msg, msec)) != 0) {
    }

    if ((rv = nng_sendmsg(client->sock, msg, 0)) != 0) {
    }

    if ((rv = nng_recvmsg(client->sock, &msg, 0)) != 0) {
    }
    end = nng_clock();
    nng_msg_free(msg);
    nng_close(client->sock);

    printf("Request took %u milliseconds.\n", (uint32_t)(end - start));
    return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

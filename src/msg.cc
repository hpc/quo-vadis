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

// TODO(skg) Bind the server to some subset of hardware resources because it
// spawns threads.

#include "private/common.h"
#include "private/msg.h"
#include "private/logger.h"

#include "nng/supplemental/util/platform.h"

// This should be more than plenty for our use case.
#define URL_MAX_LEN 1024

struct qvi_msg_server_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
    int qdepth;
    qvi_msg_t **messages;
};

struct qvi_msg_client_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
};

static void
server_cb(
    void *arg
) {
    qvi_msg_t *msg = (qvi_msg_t *)arg;
    nng_msg *payload;
    int rv;
    uint32_t when;

    switch (msg->state) {
    case qvi_msg_t::INIT:
        msg->state = qvi_msg_t::RECV;
        nng_recv_aio(msg->sock, msg->aio);
        break;
    case qvi_msg_t::RECV:
        if (nng_aio_result(msg->aio) != 0) {
            return;
        }
        payload = nng_aio_get_msg(msg->aio);
        if ((rv = nng_msg_trim_u32(payload, &when)) != 0) {
            // Bad message, just ignore it.
            nng_msg_free(payload);
            nng_recv_aio(msg->sock, msg->aio);
            return;
        }
        msg->payload = payload;
        msg->state = qvi_msg_t::WAIT;
        nng_sleep_aio(when, msg->aio);
        break;
    case qvi_msg_t::WAIT:
        // We could add more data to the message here.
        nng_aio_set_msg(msg->aio, msg->payload);
        msg->payload = nullptr;
        msg->state = qvi_msg_t::SEND;
        nng_send_aio(msg->sock, msg->aio);
        break;
    case qvi_msg_t::SEND:
        if ((rv = nng_aio_result(msg->aio)) != 0) {
            nng_msg_free(msg->payload);
            // TODO(skg) deal with error
        }
        msg->state = qvi_msg_t::RECV;
        nng_recv_aio(msg->sock, msg->aio);
        break;
    default:
        QVI_LOG_ERROR("Invalid state entered: {}", msg->state);
        break;
    }
}

static int
server_allocate_msg_queue(
    qvi_msg_server_t *server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    server->messages = (qvi_msg_t **)calloc(
        server->qdepth,
        sizeof(*(server->messages))
    );
    if (!server->messages) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    for (int i = 0; i < server->qdepth; ++i) {
        qvi_msg_t *imsg = nullptr;
        if (!(imsg = (qvi_msg_t *)nng_alloc(sizeof(*imsg)))) {
            QVI_LOG_ERROR("nng_alloc() failed");
            return QV_ERR_OOR;
        }
        int rc = nng_aio_alloc(&imsg->aio, server_cb, imsg);
        if (rc != 0) {
            QVI_LOG_ERROR(
                "nng_aio_alloc() failed with rc={} ({})",
                rc,
                nng_strerror(rc)
            );
            return QV_ERR_MSG;
        }
        imsg->state = qvi_msg_t::INIT;
        imsg->sock  = server->sock;
        server->messages[i] = imsg;
    }
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
    // Close the socket before we free any other resources.
    int rc = nng_close(server->sock);
    if (rc != 0) {
        char const *ers = "nng_close() failed";
        QVI_LOG_WARN("{} with rc={} ({})", ers, rc, nng_strerror(rc));
    }

    if (server->messages) {
        for (int i = 0; i < server->qdepth; ++i) {
            nng_aio_free(server->messages[i]->aio);
            nng_free(server->messages[i], sizeof(qvi_msg_t));
        }
        free(server->messages);
    }
    free(server);
}

static void
server_register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
}

static int
server_setup(
    qvi_msg_server_t *server,
    const char *url,
    int qdepth
) {
    // Register cleanup function at exit.
    server_register_atexit();
    // Queue depth sanity
    if (qdepth < 0) {
        QVI_LOG_ERROR("Negative queue depths not supported");
        return QV_ERR_INVLD_ARG;
    }
    server->qdepth = qdepth;

    const int nwritten = snprintf(server->url, sizeof(server->url), "%s", url);
    if (nwritten >= URL_MAX_LEN) {
        QVI_LOG_ERROR("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

static int
server_open_commchan(
    qvi_msg_server_t *server
) {
    int rc = nng_rep0_open_raw(&server->sock);
    if (rc != 0) {
        char const *ers = "nng_rep0_open_raw() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

static int
server_listen(
    qvi_msg_server_t *server
) {
    char const *ers = nullptr;

    int rc = nng_listen(server->sock, server->url, NULL, 0);
    if (rc != 0) {
        ers = "nng_listen() failed";
        goto out;
    }
    for (int i = 0; i < server->qdepth; ++i) {
        // This start the state machine.
        server_cb(server->messages[i]);
    }
    nng_msleep(10000);
#if 0
    // TODO(skg) Add proper shutdown
    while (true) {
        nng_msleep(3600000);
    }
#endif
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_msg_server_start(
    qvi_msg_server_t *server,
    const char *url,
    int qdepth
) {
    if (!server || !url) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = server_setup(server, url, qdepth);
    if (rc != QV_SUCCESS) {
        ers = "server_setup() failed";
        goto out;
    }

    rc = server_open_commchan(server);
    if (rc != QV_SUCCESS) {
        ers = "server_open_commchan() failed";
        goto out;
    }

    rc = server_allocate_msg_queue(server);
    if (rc != QV_SUCCESS) {
        ers = "server_allocate_msg_queue() failed";
        goto out;
    }

    rc = server_listen(server);
    if (rc != QV_SUCCESS) {
        ers = "server_listen() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }
    return QV_SUCCESS;
}

static void
client_register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
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
    client_register_atexit();
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
    int rv;
    nng_msg *msg;
    nng_time start;
    nng_time end;
    uint32_t msec;

    msec = atoi(msecstr) * 100;

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

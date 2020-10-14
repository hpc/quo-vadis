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
 * @file test-msg.c
 */

#include "quo-vadis.h"
#include "private/msg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
server(
    const char *url
) {
    char const *ers = NULL;

    qvi_msg_server_t *server = NULL;
    int rc = qvi_msg_server_construct(&server);
    if (rc != QV_SUCCESS) {
        ers = "qvi_msg_server_construct() failed";
        goto out;
    }

    rc = qvi_msg_server_start(server, url, 10);
    if (rc != QV_SUCCESS) {
        ers = "qvi_msg_server_start() failed";
        goto out;
    }
    sleep(10);
out:
    qvi_msg_server_destruct(server);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        exit(EXIT_FAILURE);
    }
    return 0;
}

// TODO(skg) Add timeout to terminate when the server fails, etc.
int
client(
    const char *url,
    const char *msecstr
) {
    char const *ers = NULL;

    qvi_msg_client_t *client = NULL;
    int rc = qvi_msg_client_construct(&client);
    if (rc != QV_SUCCESS) {
        ers = "qvi_msg_client_construct() failed";
        goto out;
    }
    rc = qvi_msg_client_send(client, url, msecstr);
    if (rc != QV_SUCCESS) {
        ers = "qvi_msg_client_send() failed";
        goto out;
    }
out:
    qvi_msg_client_destruct(client);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        exit(EXIT_FAILURE);
    }
    return 0;
}

int
main(
    int argc,
    char **argv
) {
    printf("# Starting msg test\n");

    int rc;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <url> [-s|<secs>]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[2], "-s") == 0) {
        rc = server(argv[1]);
    }
    else {
        rc = client(argv[1], argv[2]);
    }
    exit(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

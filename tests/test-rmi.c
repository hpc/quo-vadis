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
 * @file test-rmi.c
 */

#include "quo-vadis.h"

#include "private/qvi-rmi.h"
#include "private/qvi-utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int
server(
    const char *url
) {
    printf("# [%d] Starting Server (%s)\n", getpid(), url);

    char const *ers = NULL;
    double start = qvi_time(), end;

    qvi_rmi_server_t *server = NULL;
    int rc = qvi_rmi_server_construct(&server);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_construct() failed";
        goto out;
    }

    rc = qvi_rmi_server_start(server, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    end = qvi_time();
    printf("# [%d] Server Start Time %lf seconds\n", getpid(), end - start);
    // Let the main thread take a snooze while the server does its thing.
    sleep(10);
out:
    qvi_rmi_server_destruct(server);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static int
client(
    const char *url
) {
    printf("# [%d] Starting Client (%s)\n", getpid(), url);

    char const *ers = NULL;

    qvi_rmi_client_t *client = NULL;
    int rc = qvi_rmi_client_construct(&client);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_construct() failed";
        goto out;
    }

    rc = qvi_rmi_client_connect(client, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_connect() failed";
        goto out;
    }

    pid_t mypid = getpid();
    qv_hwloc_bitmap_t bitmap;
    rc = qvi_rmi_task_get_cpubind(client, mypid, &bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_task_get_cpubind() failed";
        goto out;
    }
    char *res;
    qv_hwloc_bitmap_asprintf(bitmap, &res);
    printf("# [%d] cpubind = %s\n", mypid, res);
    qv_hwloc_bitmap_free(bitmap);
    free(res);
out:
    qvi_rmi_client_destruct(client);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static void
usage(const char *appn)
{
    fprintf(stderr, "Usage: %s URL -s|-c\n", appn);
}

int
main(
    int argc,
    char **argv
) {
    int rc = 0;

    setbuf(stdout, NULL);

    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[2], "-s") == 0) {
        rc = server(argv[1]);
    }
    else if (strcmp(argv[2], "-c") == 0) {
        rc = client(argv[1]);
    }
    else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

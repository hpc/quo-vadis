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
 * @file test-hw-server.c
 */

#include "quo-vadis.h"
#include "quo-vadis/hw-server.h"

#include <stdlib.h>
#include <stdio.h>

int
main(void)
{
    printf("# Starting hw-server test\n");

    char const *ers = NULL;
    qv_hw_server_t *hws;

    int rc = qv_hw_server_construct(&hws);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_server_construct() failed";
        goto out;
    }

    rc = qv_hw_server_init(hws);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_server_init() failed";
        goto out;
    }

    rc = qv_hw_server_finalize(hws);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_server_finalize() failed";
        goto out;
    }
out:
    qv_hw_server_destruct(hws);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return EXIT_FAILURE;
    }
    printf("# Done\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

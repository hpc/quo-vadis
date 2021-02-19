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
 * @file test-pmi.c
 */

#include "qvi-pmi.h"
#include "qvi-utils.h"

#include "quo-vadis.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(
    int argc,
    char **argv
) {
    int rc = QV_SUCCESS;
    char const *ers = NULL;

    qvi_pmi_t *pmi = NULL;

    uint32_t gid = 0;
    uint32_t lid = 0;
    uint32_t usize = 0;

    rc = qvi_pmi_construct(&pmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_pmi_construct() failed";
        goto out;
    }

    rc = qvi_pmi_init(pmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_pmi_init() failed";
        goto out;
    }

    gid = qvi_pmi_gid(pmi);
    lid = qvi_pmi_lid(pmi);
    usize = qvi_pmi_usize(pmi);

    printf(
        "Hello from gid=%"PRIu32" (lid=%"PRIu32") of usize=%"PRIu32"\n",
        gid,
        lid,
        usize
    );

    rc = qvi_pmi_finalize(pmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_pmi_finalize() failed";
        goto out;
    }
out:
    qvi_pmi_destruct(pmi);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

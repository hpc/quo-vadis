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

#include "private/qvi-pmi.h"
#include "private/qvi-utils.h"

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

    rc = qvi_pmi_construct(&pmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_pmi_construct() failed";
        goto out;
    }

    rc = qvi_pmi_load(pmi);
    if (rc != QV_SUCCESS) {
        printf("FAIL\n");
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

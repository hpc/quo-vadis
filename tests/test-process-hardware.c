/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-process-scopes.c
 */

#include "quo-vadis.h"
#include "common-test-utils.h"

typedef struct {
    char *name;
    qv_scope_flags_t flags;
} setup_name_to_flags_t;

int
main(void)
{
    const setup_name_to_flags_t setup_tab[] = {
        {CTU_TOSTRING(QV_SCOPE_FLAG_NONE),   QV_SCOPE_FLAG_NONE},
        {CTU_TOSTRING(QV_SCOPE_FLAG_NO_SMT), QV_SCOPE_FLAG_NO_SMT}
    };

    const size_t n_setups = sizeof(setup_tab) / sizeof(setup_name_to_flags_t);
    for (size_t i = 0; i < n_setups; i++) {
        char const *ers = NULL;

        qv_scope_t *base_scope;
        int rc = qv_process_scope_get(
            QV_SCOPE_USER,
            setup_tab[i].flags,
            &base_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_get(QV_SCOPE_USER) failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        ctu_emit_host_hw_info(base_scope, setup_tab[i].name);

        ctu_emit_gpu_info(base_scope, setup_tab[i].name);

        rc = qv_scope_free(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

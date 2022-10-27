/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-scopes-mpi.c
 */

#include "quo-vadis-mpi.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

static int
get_group_id(
    int taskid,
    int ntask,
    int npieces
) {
    if (npieces != 2) {
        panic("This test requires npieces=2");
    }
    const int nchunk = (ntask + (ntask % npieces)) / npieces;
    return taskid / nchunk;
}

static void
scope_report(
    qv_context_t *ctx,
    int wrank,
    qv_scope_t *scope,
    const char *scope_name
) {
    char const *ers = NULL;

    int taskid;
    int rc = qv_scope_taskid(ctx, scope, &taskid);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ntasks;
    rc = qv_scope_ntasks(ctx, scope, &ntasks);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf("[%d] %s taskid is %d\n", wrank, scope_name, taskid);
    printf("[%d] %s ntasks is %d\n", wrank, scope_name, ntasks);

    rc = qv_scope_barrier(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

static void
change_bind(
    qv_context_t *ctx,
    int wrank,
    qv_scope_t *scope
) {
    char const *ers = NULL;

    if (getenv("HWLOC_XMLFILE")) {
        if (wrank == 0) {
            printf("*** Using synthetic topology. "
                   "Skipping change_bind tests. ***\n");
        }
        return;
    }

    int rc = qv_bind_push(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind1s;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is     %s\n", wrank, bind1s);
    free(bind1s);

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind2s;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped cpubind is  %s\n", wrank, bind2s);
    free(bind2s);
    // TODO(skg) Add test to make popped is same as original.
    rc = qv_scope_barrier(ctx, scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    setbuf(stdout, NULL);

    qv_context_t *ctx;
    rc = qv_mpi_context_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *self_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_PROCESS,
        &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scope_report(ctx, wrank, self_scope, "self_scope");

    rc = qv_scope_free(ctx, self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_USER,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    scope_report(ctx, wrank, base_scope, "base_scope");

    int base_scope_ntasks;
    rc = qv_scope_ntasks(
        ctx,
        base_scope,
        &base_scope_ntasks
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_ntasks() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_id;
    rc = qv_scope_taskid(
        ctx,
        base_scope,
        &base_scope_id
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_taskid() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_pu;
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
        QV_HW_OBJ_PU,
        &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in base_scope is %d\n", wrank, n_pu);

    const int npieces = 2;
    const int gid = get_group_id(
        base_scope_id,
        base_scope_ntasks,
        npieces
    );
    printf("[%d] base GID is %d\n", wrank, gid);

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        npieces,
        gid,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_PU,
        &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in sub_scope is %d\n", wrank, n_pu);

    scope_report(ctx, wrank, sub_scope, "sub_scope");
    change_bind(ctx, wrank, sub_scope);

    if (base_scope_id == 0) {
        qv_scope_t *create_scope;
        rc = qv_scope_create(
            ctx, sub_scope, QV_HW_OBJ_CORE,
            1, 0, &create_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_create() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        int n_core;
        rc = qv_scope_nobjs(
            ctx,
            create_scope,
            QV_HW_OBJ_PU,
            &n_core
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_nobjs() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        printf("[%d] Number of PUs in create_scope is %d\n", wrank, n_core);

        scope_report(ctx, wrank, create_scope, "create_scope");

        rc = qv_scope_free(ctx, create_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    qv_scope_t *sub_sub_scope;
    rc = qv_scope_split(
        ctx,
        sub_scope,
        npieces,
        gid,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_sub_scope,
        QV_HW_OBJ_PU,
        &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in sub_sub_scope is %d\n", wrank, n_pu);

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, sub_sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
out:
    if (qv_mpi_context_free(ctx) != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        panic("%s", ers);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

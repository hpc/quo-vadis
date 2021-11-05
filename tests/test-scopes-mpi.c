/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
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
#include "qvi-rmi.h"

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

static void
scope_report(
    qv_context_t *ctx,
    int wrank,
    qv_scope_t *scope,
    char *scope_name
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
    rc = qv_bind_get_as_string(ctx, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] New cpubind is %s\n", wrank, bind1s);
    free(bind1s);

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind2s;
    rc = qv_bind_get_as_string(ctx, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped cpubind is %s\n", wrank, bind2s);
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
    rc = qv_mpi_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_create() failed";
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

    int n_cores;
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in base_scope is %d\n", wrank, n_cores);

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        2,
        wrank,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in sub_scope is %d\n", wrank, n_cores);

    scope_report(ctx, wrank, sub_scope, "sub_scope");

    char *binds;
    rc = qv_bind_get_as_string(ctx, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Current cpubind is %s\n", wrank, binds);
    free(binds);

    change_bind(ctx, wrank, sub_scope);

    qv_scope_t *sub_sub_scope;
    rc = qv_scope_split(
        ctx,
        sub_scope,
        2,
        wrank,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_nobjs(
        ctx,
        sub_sub_scope,
        QV_HW_OBJ_NUMANODE,
        &n_cores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of NUMA in sub_sub_scope is %d\n", wrank, n_cores);

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

    rc = qv_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
out:
    if (qv_mpi_free(ctx) != QV_SUCCESS) {
        ers = "qvi_free() failed";
        panic("%s", ers);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

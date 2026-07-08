/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-scopes-mpi.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

static int
get_group_id(
    int taskid,
    int ntask,
    int npieces
) {
    if (npieces != 2) {
        ctu_panic("This test requires npieces=2");
    }
    const int nchunk = (ntask + (ntask % npieces)) / npieces;
    return taskid / nchunk;
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
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }
    // Self scope test
    qv_scope_t *self_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_PROCESS,
        QV_SCOPE_FLAG_NONE,
        &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < wsize; ++i) {
        if (wrank == i) {
            ctu_emit_scope_report(self_scope, CTU_SCOPE_KIND_MPI, "self_scope");
        }
        else {
            ctu_emit(self_scope, CTU_SCOPE_KIND_MPI, "");
        }
        // Barrier for nicer output.
        rc = MPI_Barrier(MPI_COMM_WORLD);
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Barrier() failed";
            ctu_panic("%s (rc=%d)", ers, rc);
        }
    }

    rc = qv_scope_free(self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Base scope test
    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_JOB,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_pemit(base_scope, CTU_SCOPE_KIND_MPI, wrank == 0, "\n");
    ctu_emit_scope_report(base_scope, CTU_SCOPE_KIND_MPI, "base_scope");

    int base_scope_sgsize;
    rc = qv_scope_group_size(
        base_scope,
        &base_scope_sgsize
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_rank;
    rc = qv_scope_group_rank(
        base_scope,
        &base_scope_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    const int npieces = 2;
    const int gid = get_group_id(
        base_scope_rank,
        base_scope_sgsize,
        npieces
    );

    // Barrier for nicer output.
    rc = MPI_Barrier(MPI_COMM_WORLD);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Barrier() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope,
        npieces,
        QV_SCOPE_SPLIT_PACKED,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_pemit(sub_scope, CTU_SCOPE_KIND_MPI, wrank == 0, "\n");
    ctu_emit_scope_report(sub_scope, CTU_SCOPE_KIND_MPI, "sub_scope");

    if (base_scope_rank == 0) {
        qv_scope_t *create_scope;
        rc = qv_scope_create(
            sub_scope, QV_HW_OBJ_CORE,
            1, 0, &create_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_create() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        int n_core;
        rc = qv_scope_hw_obj_count(
            create_scope,
            QV_HW_OBJ_PU,
            &n_core
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_hw_obj_count() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        printf("[%d] Number of PUs in create_scope is %d\n", wrank, n_core);

        ctu_emit_scope_report(create_scope, CTU_SCOPE_KIND_MPI, "create_scope");

        rc = qv_scope_free(create_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    qv_scope_t *sub_sub_scope;
    rc = qv_scope_split(
        sub_scope,
        npieces,
        gid,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_pu;
    rc = qv_scope_hw_obj_count(
        sub_sub_scope,
        QV_HW_OBJ_PU,
        &n_pu
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Number of PUs in sub_sub_scope is %d\n", wrank, n_pu);

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

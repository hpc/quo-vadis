/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-scopes-mpi.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    const int npieces = 2;

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
            ctu_emit_scope_report(
                self_scope, CTU_SCOPE_KIND_MPI, "   self_scope"
            );
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

    // Get my base_scope's size and my rank.
    int base_scope_size;
    rc = qv_scope_group_size(
        base_scope,
        &base_scope_size
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

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                base_scope, CTU_SCOPE_KIND_MPI, "   base_scope"
            );
        }
        else {
            ctu_emit(base_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    int base_scope_sgsize;
    rc = qv_scope_group_size(
        base_scope,
        &base_scope_sgsize
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
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

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                sub_scope, CTU_SCOPE_KIND_MPI, "    sub_scope"
            );
        }
        else {
            ctu_emit(sub_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    if (base_scope_rank == 0) {
        qv_scope_t *create_scope;
        rc = qv_scope_create(
            sub_scope,
            QV_SCOPE_FLAG_NONE,
            QV_HW_OBJ_CORE,
            1,
            &create_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_create() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        ctu_emit_scope_report(
            create_scope, CTU_SCOPE_KIND_MPI, " create_scope"
        );

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
        QV_SCOPE_SPLIT_SPREAD,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < base_scope_size; ++i) {
        if (base_scope_rank == i) {
            ctu_emit_scope_report(
                sub_sub_scope, CTU_SCOPE_KIND_MPI, "sub_sub_scope"
            );
        }
        else {
            ctu_emit(sub_sub_scope, CTU_SCOPE_KIND_MPI, "");
        }
        rc = qv_scope_barrier(base_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

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

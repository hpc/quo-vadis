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

    // Self scope test.
    qv_scope_t *self_scope;
    rc = qv_mpi_scope(
        comm,
        QV_SCOPE_PROCESS,
        QV_SCOPE_FLAG_NONE,
        &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        self_scope, CTU_SCOPE_KIND_MPI, "   self_scope"
    );

    rc = qv_free(self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Base scope test
    qv_scope_t *base_scope;
    rc = qv_mpi_scope(
        comm,
        QV_SCOPE_JOB,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int base_scope_rank;
    rc = qv_group_rank(
        base_scope,
        &base_scope_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "   base_scope"
    );

    qv_scope_t *sub_scope;
    rc = qv_split(
        base_scope,
        npieces,
        QV_SPLIT_PACKED,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        sub_scope, CTU_SCOPE_KIND_MPI, "    sub_scope"
    );

    if (base_scope_rank == 0) {
        qv_scope_t *create_scope;
        rc = qv_create_scope(
            sub_scope,
            QV_SCOPE_FLAG_NONE,
            QV_HW_OBJ_CORE,
            1,
            &create_scope
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_create_scope() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        ctu_emit_scope_report(
            create_scope, CTU_SCOPE_KIND_MPI, " create_scope"
        );

        rc = qv_free(create_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_free() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    else {
        // Matching emit to avoid hangs.
        ctu_emit(NULL, CTU_SCOPE_KIND_MPI, "");
    }

    qv_scope_t *sub_sub_scope;
    rc = qv_split(
        sub_scope,
        npieces,
        QV_SPLIT_SPREAD,
        &sub_sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        sub_sub_scope, CTU_SCOPE_KIND_MPI, "sub_sub_scope"
    );

    rc = qv_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(sub_sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

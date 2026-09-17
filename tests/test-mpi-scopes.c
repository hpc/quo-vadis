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

    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv));

    // Self scope test.
    qv_scope_t *self_scope;
    ctu_check(
        qv_mpi_scope(
            comm,
            QV_SCOPE_PROCESS,
            QV_SCOPE_FLAG_NONE,
            &self_scope
        )
    );

    ctu_emit_scope_report(
        self_scope, CTU_SCOPE_KIND_MPI, "   self_scope"
    );

    ctu_check(qv_free(self_scope));

    // Base scope test
    qv_scope_t *base_scope;
    ctu_check(
        qv_mpi_scope(comm, QV_SCOPE_JOB, QV_SCOPE_FLAG_NONE, &base_scope)
    );

    int base_scope_rank;
    ctu_check(qv_group_rank(base_scope, &base_scope_rank));

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "   base_scope"
    );

    qv_scope_t *sub_scope;
    ctu_check(qv_split(base_scope, npieces, QV_SPLIT_PACKED, &sub_scope));

    ctu_emit_scope_report(
        sub_scope, CTU_SCOPE_KIND_MPI, "    sub_scope"
    );

    if (base_scope_rank == 0) {
        qv_scope_t *create_scope;
        ctu_check(
            qv_create_scope(
                sub_scope,
                QV_SCOPE_FLAG_NONE,
                QV_HW_CORE,
                1,
                &create_scope
            )
        );

        ctu_emit_scope_report(
            create_scope, CTU_SCOPE_KIND_MPI, " create_scope"
        );

        ctu_check(qv_free(create_scope));
    }
    else {
        // Matching emit to avoid hangs.
        ctu_emit(NULL, CTU_SCOPE_KIND_MPI, "");
    }

    qv_scope_t *sub_sub_scope;
    ctu_check(qv_split(sub_scope, npieces, QV_SPLIT_SPREAD, &sub_sub_scope));

    ctu_emit_scope_report(
        sub_sub_scope, CTU_SCOPE_KIND_MPI, "sub_sub_scope"
    );

    ctu_check(qv_free(base_scope));

    ctu_check(qv_free(sub_scope));

    ctu_check(qv_free(sub_sub_scope));

    ctu_mpi_check(MPI_Finalize());

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-scope-create.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

static qv_scope_t *
test_create_scope(
    qv_scope_t *scope_to_test,
    int ncores,
    bool free_scope
) {
    char *scope_name = NULL;

    int np = asprintf(
        &scope_name, "# %02d-core scope (release=%s)",
        ncores, free_scope ? "yes" : "no"
    );
    if (np == -1) {
        ctu_panic("OOR!");
    }

    qv_scope_t *core_scope;
    ctu_check(
        qv_create_scope(
            scope_to_test,
            QV_SCOPE_FLAG_NONE,
            QV_HW_CORE,
            ncores,
            &core_scope
        ),
        "qv_create_scope"
    );

    ctu_emit_scope_report(
        core_scope, CTU_SCOPE_KIND_MPI, scope_name
    );

    free(scope_name);

    if (free_scope) {
        ctu_check(qv_free(core_scope), "qv_free");
        core_scope = NULL;
    }
    return core_scope;
}

int
main(
    int argc,
    char **argv
) {
    MPI_Comm comm = MPI_COMM_WORLD;

    ctu_mpi_check(MPI_Init(&argc, &argv), "MPI_Init");

    qv_scope_t *base_scope;
    ctu_check(
        qv_mpi_scope(
            comm,
            QV_SCOPE_USER,
            QV_SCOPE_FLAG_NONE,
            &base_scope
        ),
        "qv_mpi_scope"
    );

    int base_scope_size;
    ctu_check(
        qv_group_size(
            base_scope,
            &base_scope_size
        ),
        "qv_group_size"
    );

    int base_scope_rank;
    ctu_check(
        qv_group_rank(
            base_scope,
            &base_scope_rank
        ),
        "qv_group_rank"
    );

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "base_scope"
    );

    // Phase 1: Split base scope
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
        "\n# Scope Split Test\n"
    );
    // Split the base scope evenly across workers.
    qv_scope_t *sub_scope;
    ctu_check(
        qv_split(
            base_scope,
            base_scope_size,
            base_scope_rank,
            &sub_scope
        ),
        "qv_split"
    );

    ctu_emit_scope_report(
        sub_scope, CTU_SCOPE_KIND_MPI, " sub_scope"
    );

    // Phase 2: Create core scopes.
    const int n_core_scopes = 4;
    qv_scope_t *core_scopes[n_core_scopes];

    int ncore1 = 1;
    int ncore2 = 10;
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
        "\n# Asking and not releasing %d-core and %d-core scopes\n",
        ncore1, ncore2
    );
    core_scopes[0] = test_create_scope(sub_scope, ncore1, false);
    core_scopes[1] = test_create_scope(sub_scope, ncore2, false);

    ncore1 = 5;
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
        "\n# Asking and releasing %d-core scopes\n",
        ncore1
    );
    core_scopes[2] = test_create_scope(sub_scope, ncore1, true);
    core_scopes[3] = test_create_scope(sub_scope, ncore1, true);

    for (int i = 0; i < n_core_scopes; ++i) {
        ctu_check(qv_free(core_scopes[i]), "qv_free");
    }

    ctu_check(qv_free(sub_scope), "qv_free");

    ctu_check(qv_free(base_scope), "qv_free");

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

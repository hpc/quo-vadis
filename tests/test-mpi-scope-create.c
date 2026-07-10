/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-mpi-scope-create.c
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

static void
scope_report(
    qv_scope_t *base_scope,
    qv_scope_t *sub_scope,
    char *sub_scope_name
) {
    char const *ers = NULL;
    // Get my base_scope's size and my rank.
    int base_scope_size;
    int rc = qv_scope_group_size(
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
                sub_scope, CTU_SCOPE_KIND_MPI, sub_scope_name
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
}

static qv_scope_t *
test_create_scope(
    qv_scope_t *base_scope,
    qv_scope_t *scope_to_test,
    int ncores,
    bool free_scope
) {
    char const *ers = NULL;
    char *scope_name = NULL;

    int np = asprintf(
        &scope_name, "# %02d-core scope (release=%s)",
        ncores, free_scope ? "yes" : "no"
    );
    if (np == -1) {
        ctu_panic("OOR!");
    }

    qv_scope_t *core_scope;
    int rc = qv_scope_create(
        scope_to_test,
        QV_SCOPE_FLAG_NONE,
        QV_HW_OBJ_CORE,
        ncores,
        &core_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_create() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    scope_report(base_scope, core_scope, scope_name);
    free(scope_name);

    if (free_scope) {
        rc = qv_scope_free(core_scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        core_scope = NULL;
    }
    return core_scope;
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

    qv_scope_t *base_scope;
    rc = qv_mpi_scope_get(
        comm,
        QV_SCOPE_USER,
        QV_SCOPE_FLAG_NONE,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_scope_get() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

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

    scope_report(base_scope, base_scope, "base_scope");
    // Phase 1: Split base scope
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
        "\n# Scope Split Test\n"
    );
    // Split the base scope evenly across workers.
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope,
        base_scope_size,
        base_scope_rank,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    scope_report(base_scope, sub_scope, "sub_scope");

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
    core_scopes[0] = test_create_scope(base_scope, sub_scope, ncore1, false);
    core_scopes[1] = test_create_scope(base_scope, sub_scope, ncore2, false);

    ncore1 = 5;
    ctu_pemit(
        base_scope, CTU_SCOPE_KIND_MPI, base_scope_rank == 0,
        "\n# Asking and releasing %d-core scopes\n",
        ncore1
    );
    core_scopes[2] = test_create_scope(base_scope, sub_scope, ncore1, true);
    core_scopes[3] = test_create_scope(base_scope, sub_scope, ncore1, true);

    for (int i = 0; i < n_core_scopes; ++i) {
        rc = qv_scope_free(core_scopes[i]);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_free() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(base_scope);
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

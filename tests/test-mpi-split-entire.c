/**
 * @file test-mpi-split-entire.c
 *
 * Tests the QV_SPLIT_ENTIRE automatic grouping option. Unlike the other
 * QV_SPLIT_* options, which honor the requested number of pieces verbatim,
 * QV_SPLIT_ENTIRE guarantees that all the resources in the parent scope are
 * distributed across the children. When the group size is smaller than the
 * requested number of pieces, the split size is reduced to match the group
 * size so that no parent resource is left unused.
 */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

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

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_MPI, "     base_scope"
    );

    // The number of members participating in the split.
    int group_size;
    rc = qv_group_size(base_scope, &group_size);
    if (rc != QV_SUCCESS) {
        ers = "qv_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // The total number of PUs available in the parent scope.
    int parent_npus;
    rc = qv_hw_obj_count(base_scope, QV_HW_OBJ_PU, &parent_npus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Deliberately request more pieces than there are members. With the other
    // QV_SPLIT_* options this would carve the parent's resources into
    // |npieces| pieces and leave the pieces beyond the group size unused. With
    // QV_SPLIT_ENTIRE the split size is clamped to the group size so that all
    // parent resources end up distributed across the children.
    const int npieces = group_size + 2;

    qv_scope_t *sub_scope;
    rc = qv_split(
        base_scope,
        npieces,
        QV_SPLIT_ENTIRE,
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_split(QV_SPLIT_ENTIRE) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        sub_scope, CTU_SCOPE_KIND_MPI, "      sub_scope"
    );

    // The number of PUs this member received from the split.
    int my_npus;
    rc = qv_hw_obj_count(sub_scope, QV_HW_OBJ_PU, &my_npus);
    if (rc != QV_SUCCESS) {
        ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Every member should have received a non-empty piece of the parent.
    ctu_assert(
        my_npus > 0,
        "QV_SPLIT_ENTIRE produced an empty scope (my_npus=%d)", my_npus
    );

    // Since QV_SPLIT_ENTIRE assigns each member a distinct piece and uses the
    // entire parent, the union of the members' PUs must equal the parent's PU
    // count. Members receive disjoint pieces, so summing PU counts recovers the
    // total number of parent PUs.
    int total_npus = 0;
    rc = MPI_Allreduce(
        &my_npus, &total_npus, 1, MPI_INT, MPI_SUM, comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Allreduce() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    ctu_assert(
        total_npus == parent_npus,
        "QV_SPLIT_ENTIRE did not use all parent resources "
        "(total_npus=%d, parent_npus=%d)", total_npus, parent_npus
    );

    ctu_logf(
        "QV_SPLIT_ENTIRE: group_size=%d npieces=%d parent_npus=%d "
        "my_npus=%d total_npus=%d\n",
        group_size, npieces, parent_npus, my_npus, total_npus
    );

    rc = qv_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_free(base_scope);
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

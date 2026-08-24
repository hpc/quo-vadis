/**
 * @file test-mpi-split-entire.c
 *
 * Tests the QV_SPLIT_ENTIRE automatic grouping option. Unlike the other
 * QV_SPLIT_* options, which honor the requested number of pieces verbatim,
 * QV_SPLIT_ENTIRE guarantees that all the resources in the parent scope are
 * distributed across the children. This test exercises both possibilities:
 *
 *   1. The group size is smaller than the requested number of pieces. Here the
 *      split size is reduced to match the group size so that no parent resource
 *      is left unused.
 *   2. The group size is >= the requested number of pieces. Here no clamping is
 *      needed, so the requested number of pieces is honored verbatim and the
 *      members are distributed across those pieces.
 *
 * In both cases the union of the members' resources must equal the parent's.
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
    int npieces = group_size + 2;

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

    // Now test the other possibility: the group size is >= the requested split
    // size. Here QV_SPLIT_ENTIRE has no need to clamp the split size, so it
    // honors |npieces| verbatim, carving the parent into that many pieces and
    // distributing the members across them. The entire parent must still be
    // used, so the union of the members' PUs equals the parent's PU count.
    // Multiple members may share the same piece, so we deduplicate by only
    // counting one representative member per piece.
    npieces = (group_size > 1) ? (group_size - 1) : 1;

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

    // Determine this member's rank within its piece. Members that land in the
    // same piece share an intra-piece group; the group's rank tells us who is
    // the piece representative (rank 0). Since the group size is >= the
    // requested split size, QV_SPLIT_ENTIRE must honor the requested number of
    // pieces verbatim (no clamping occurs).
    int my_piece_rank;
    rc = qv_group_rank(sub_scope, &my_piece_rank);
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Only the representative (rank 0) of each piece contributes its PU count,
    // so summing over representatives recovers the parent's total PU count
    // without double-counting shared pieces.
    int contrib_npus = (my_piece_rank == 0) ? my_npus : 0;
    total_npus = 0;
    rc = MPI_Allreduce(
        &contrib_npus, &total_npus, 1, MPI_INT, MPI_SUM, comm
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

    // Count the number of distinct pieces (one per representative) and verify
    // the split honored the requested piece count verbatim rather than
    // clamping it to the group size.
    int is_rep = (my_piece_rank == 0) ? 1 : 0;
    int num_pieces = 0;
    rc = MPI_Allreduce(
        &is_rep, &num_pieces, 1, MPI_INT, MPI_SUM, comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Allreduce() failed";
        ctu_panic("%s (rc=%d)", ers, rc);
    }

    ctu_assert(
        num_pieces == npieces,
        "QV_SPLIT_ENTIRE did not honor the requested split size "
        "(num_pieces=%d, npieces=%d)", num_pieces, npieces
    );

    ctu_logf(
        "QV_SPLIT_ENTIRE: group_size=%d npieces=%d parent_npus=%d "
        "my_npus=%d total_npus=%d num_pieces=%d\n",
        group_size, npieces, parent_npus, my_npus, total_npus, num_pieces
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

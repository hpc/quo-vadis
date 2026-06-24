/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis-mpi.h"
#include "common-test-utils.h"

int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;
    qv_hw_obj_type_t target_dev = QV_HW_OBJ_GPU;
    char const *dev_name = ctu_obj_name(target_dev);

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

    setbuf(stdout, NULL);

    // Get base scope.
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

    int ndevs;
    rc = qv_scope_hw_obj_count(base_scope, target_dev, &ndevs);
    if (rc != QV_SUCCESS) {
      ers = "qv_scope_hw_obj_count() failed";
      ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ndevs == 0) {
        if (wrank == 0) printf("Skipping: no %ss found!\n", dev_name);
        goto done;
    }
    else {
        if (wrank == 0) printf("# Number of %ss found: %d\n", dev_name, ndevs);
    }

    qv_scope_t *dev_scope;
    rc = qv_scope_split_at(
        base_scope,
        target_dev,
        QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
        &dev_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_dev_rank;
    rc = qv_scope_group_rank(
        dev_scope,
        &my_dev_rank
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_bind_push(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ndevs;
    rc = qv_scope_hw_obj_count(
        dev_scope,
        target_dev,
        &my_ndevs
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Where did I end up?
    char *binds;
    rc = qv_scope_bind_string(dev_scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf(
        "=> [%d] Split@Dev: got %d %s(s), running on %s\n",
        wrank, my_ndevs, dev_name, binds
    );
    free(binds);

    for (int i = 0; i < my_ndevs; ++i) {
        char *devid;
        qv_scope_device_id_get(
            dev_scope, target_dev, i, QV_DEVICE_ID_PCI_BUS_ID, &devid
        );
        printf("   [%d] Dev %d PCI Bus ID = %s\n", wrank, i, devid);
        free(devid);
    }

    rc = qv_scope_free(dev_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

done:
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

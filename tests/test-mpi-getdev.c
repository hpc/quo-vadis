#include "qvi-macros.h"
#include "quo-vadis-mpi.h"

#include <stdlib.h>
#include <stdio.h>

#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

typedef struct device_name_type_s {
    char const *name;
    qv_device_id_type_t type;
} device_name_type_t;

static const device_name_type_t devnts[] = {
    {QVI_STRINGIFY(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {QVI_STRINGIFY(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {QVI_STRINGIFY(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

static void
emit_gpu_info(
    qv_context_t *ctx,
    qv_scope_t *scope,
    const char *scope_name
) {
    /* Get number of GPUs */
    int ngpus;
    int rc = qv_scope_nobjs(ctx, scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        const char *ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (ngpus == 0) {
        printf("\n# No GPU Devices in %s\n", scope_name);
        return;
    }

    printf("\n# Discovered GPU Devices in %s\n", scope_name);
    const unsigned ndevids = sizeof(devnts) / sizeof(device_name_type_t);
    for (unsigned i = 0; i < ngpus; ++i) {
        for (unsigned j = 0; j < ndevids; ++j) {
            char *devids = NULL;
            int rc = qv_scope_get_device(
                ctx,
                scope,
                QV_HW_OBJ_GPU,
                i,
                devnts[j].type,
                &devids
            );
            if (rc != QV_SUCCESS) {
                const char *ers = "qv_scope_get_device() failed";
                panic("%s (rc=%s)", ers, qv_strerr(rc));
            }
            printf("# Device %u %s = %s\n", i, devnts[j].name, devids);
            free(devids);
        }
    }
    printf("# -----------------------------------------------------------\n");
}

int main(int argc, char **argv)
{
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

    /* Create a QV context */
    qv_context_t *ctx;
    rc = qv_mpi_context_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get base scope: RM-given resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(ctx, QV_SCOPE_USER, &base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (wrank == 0) {
        emit_gpu_info(ctx, base_scope, "Base Scope");
    }

    /* Get number of GPUs */
    int ngpus;
    rc = qv_scope_nobjs(ctx, base_scope, QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (wrank == 0) {
        printf("[%d]: Base scope has %d GPUs\n", wrank, ngpus);
    }

    /* Split the base scope evenly across workers */
    qv_scope_t *rank_scope;
    rc = qv_scope_split(
            ctx,
            base_scope,
            wsize,        // Number of workers
            wrank,        // My group color
            &rank_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Move to my subscope */
    rc = qv_bind_push(ctx, rank_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get num GPUs */
    int rank_ngpus;
    rc = qv_scope_nobjs(ctx, rank_scope, QV_HW_OBJ_GPU, &rank_ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d]: Local scope has %d GPUs\n", wrank, rank_ngpus);

    int total_ngpus;
    MPI_Reduce(&rank_ngpus, &total_ngpus, 1, MPI_INT, MPI_SUM, 0, comm);

    if (wrank == 0) {
        if (ngpus == total_ngpus) {
            printf("PASS: Number of GPUs match\n");
        }
        else {
            printf("FAIL: Base GPUs=%d do not match aggregate GPUs=%d\n",
                    ngpus, total_ngpus);
        }
    }

    qv_mpi_context_free(ctx);
    MPI_Finalize();

    return 0;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
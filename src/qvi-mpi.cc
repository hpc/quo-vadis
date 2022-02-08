/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-mpi.cc
 */

#include "qvi-common.h"

#include "qvi-mpi.h"
#include "qvi-utils.h"

#include "mpi.h"

using qvi_mpi_group_tab_t =
std::unordered_map<qvi_mpi_group_id_t, qvi_mpi_group_t>;

struct qvi_mpi_group_s {
    /** ID used for table lookups */
    qvi_mpi_group_id_t tabid = 0;
    /** ID (rank) in group */
    int group_id = 0;
    /** Size of group */
    int group_size = 0;
    /* Underlying MPI group */
    MPI_Group mpi_group = MPI_GROUP_NULL;
    /* Underlying MPI communicator from MPI group */
    MPI_Comm mpi_comm = MPI_COMM_NULL;
};

struct qvi_mpi_s {
    /** Task associated with this MPI process */
    qvi_task_t *task = nullptr;
    /** Node size */
    int node_size = 0;
    /** World size */
    int world_size = 0;
    /** Duplicate of MPI_COMM_SELF */
    MPI_Comm self_comm = MPI_COMM_NULL;
    /** Duplicate of initializing communicator */
    MPI_Comm world_comm = MPI_COMM_NULL;
    /** Node communicator */
    MPI_Comm node_comm = MPI_COMM_NULL;
    /** Maintains the next available group ID value */
    qvi_mpi_group_id_t group_next_id = QVI_MPI_GROUP_INTRINSIC_END;
    /** Group table (ID to internal structure mapping) */
    qvi_mpi_group_tab_t *group_tab = nullptr;
};

/**
 * Copies contents of internal structure from src to dst.
 */
static void
cp_mpi_group(
    const qvi_mpi_group_t *src,
    qvi_mpi_group_t *dst
) {
    memmove(dst, src, sizeof(*src));
}

/**
 * Returns the next available group ID.
 */
static int
next_group_tab_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t *gid
) {
    if (mpi->group_next_id == UINT64_MAX) {
        qvi_log_error("qvi_mpi_group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = mpi->group_next_id++;
    return QV_SUCCESS;
}

/**
 * Creates 'node' communicators from an arbitrary MPI communicator.
 */
static int
mpi_comm_to_new_node_comm(
    MPI_Comm comm,
    MPI_Comm *node_comm
) {
    int rc = MPI_Comm_split_type(
        comm,
        MPI_COMM_TYPE_SHARED,
        0,
        MPI_INFO_NULL,
        node_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_split_type(MPI_COMM_TYPE_SHARED) failed");
    }
    return rc;
}

/**
 * Creates a QV group from an MPI communicator.
 */
static int
group_create_from_mpi_comm(
    MPI_Comm comm,
    qvi_mpi_group_t *new_group
) {
    cstr ers = nullptr;

    new_group->mpi_comm = comm;

    int rc = MPI_Comm_group(
        new_group->mpi_comm,
        &new_group->mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_group() failed";
        goto out;
    }

    rc = MPI_Group_rank(
        new_group->mpi_group,
        &new_group->group_id
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_rank() failed";
        goto out;
    }

    rc = MPI_Group_size(
        new_group->mpi_group,
        &new_group->group_size
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_size() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

/**
 * Creates a QV group from an MPI group.
 */
static int
group_create_from_mpi_group(
    qvi_mpi_t *mpi,
    MPI_Group group,
    qvi_mpi_group_t **maybe_group
) {
    cstr ers = nullptr;

    int qvrc = QV_SUCCESS;

    MPI_Comm group_comm;
    int rc = MPI_Comm_create_group(
        mpi->node_comm,
        group,
        0,
        &group_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_create_group() failed";
        qvrc = QV_ERR_MPI;
        goto out;
    }
    // Not in the group, so return NULL and bail.
    if (group_comm == MPI_COMM_NULL) {
        *maybe_group = nullptr;
        return QV_SUCCESS;
    }
    // In the group.
    qvrc = qvi_mpi_group_new(maybe_group);
    if (qvrc != QV_SUCCESS) {
        ers = "qvi_mpi_group_new() failed";
        goto out;
    }
    qvrc = group_create_from_mpi_comm(
        group_comm,
        *maybe_group
    );
    if (qvrc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return qvrc;
}

/**
 *
 */
static int
group_add(
    qvi_mpi_t *mpi,
    qvi_mpi_group_t *group,
    qvi_mpi_group_id_t given_id = QVI_MPI_GROUP_NULL
) {
    qvi_mpi_group_id_t gtid;
    // Marker used to differentiate between intrinsic and automatic IDs.
    if (given_id != QVI_MPI_GROUP_NULL) {
        gtid = given_id;
    }
    else {
        int rc = next_group_tab_id(mpi, &gtid);
        if (rc != QV_SUCCESS) return rc;
    }
    group->tabid = gtid;
    mpi->group_tab->insert({gtid, *group});
    return QV_SUCCESS;
}

int
qvi_mpi_new(
    qvi_mpi_t **mpi
) {
    int rc = QV_SUCCESS;

    qvi_mpi_t *impi = qvi_new qvi_mpi_t;
    if (!impi) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Task
    // We don't own task memory. Set in qvi_mpi_init().
    // Groups
    impi->group_tab = qvi_new qvi_mpi_group_tab_t;
    if (!impi->group_tab) {
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_mpi_free(&impi);
    }
    *mpi = impi;
    return rc;
}

void
qvi_mpi_free(
    qvi_mpi_t **mpi
) {
    if (!mpi) return;
    qvi_mpi_t *impi = *mpi;
    if (!impi) goto out;
    if (impi->group_tab) {
        for (auto &i : *impi->group_tab) {
            auto &mpi_group = i.second.mpi_group;
            if (mpi_group != MPI_GROUP_NULL) {
                MPI_Group_free(&mpi_group);
            }
            auto &mpi_comm = i.second.mpi_comm;
            if (mpi_comm != MPI_COMM_NULL) {
                MPI_Comm_free(&mpi_comm);
            }
        }
        delete impi->group_tab;
    }
    if (impi->world_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&impi->world_comm);
    }
    delete impi;
out:
    *mpi = nullptr;
}

pid_t
qvi_mpi_task_pid_get(
    qvi_mpi_t *mpi
) {
    return qvi_task_pid(mpi->task);
}

/**
 * Creates the node communicator. Returns the MPI status.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    cstr ers = nullptr;
    // MPI_COMM_SELF duplicate
    int rc = MPI_Comm_dup(
        MPI_COMM_SELF,
        &mpi->self_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_dup(MPI_COMM_SELF) failed";
        goto out;
    }
    // 'World' (aka initializing communicator) duplicate
    rc = MPI_Comm_dup(
        comm,
        &mpi->world_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_dup() failed";
        goto out;
    }
    // Node communicator
    rc = mpi_comm_to_new_node_comm(comm, &mpi->node_comm);
    if (rc != MPI_SUCCESS) {
        ers = "mpi_comm_to_new_node_comm() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    cstr ers = nullptr;

    qvi_mpi_group_t self_group, node_group;
    int rc = group_create_from_mpi_comm(
        mpi->self_comm,
        &self_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm(self_comm) failed";
        goto out;
    }
    rc = group_create_from_mpi_comm(
        mpi->node_comm,
        &node_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm(node_comm) failed";
        goto out;
    }
    rc = group_add(mpi, &self_group, QVI_MPI_GROUP_SELF);
    if (rc != QV_SUCCESS) {
        ers = "group_add(self) failed";
        goto out;
    }
    rc = group_add(mpi, &node_group, QVI_MPI_GROUP_NODE);
    if (rc != QV_SUCCESS) {
        ers = "group_add(node) failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    qvi_task_t *task,
    MPI_Comm comm
) {
    cstr ers = nullptr;
    int inited;
    // If MPI isn't initialized, then we can't continue.
    int rc = MPI_Initialized(&inited);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;
    if (!inited) {
        ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }
    // MPI is initialized.
    rc = MPI_Comm_size(comm, &mpi->world_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        goto out;
    }

    int world_id;
    rc = MPI_Comm_rank(comm, &world_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        goto out;
    }

    rc = create_intrinsic_comms(mpi, comm);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_comms() failed";
        goto out;
    }

    rc = MPI_Comm_size(mpi->node_comm, &mpi->node_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size(node_comm) failed";
        goto out;
    }

    int node_id;
    rc = MPI_Comm_rank(mpi->node_comm, &node_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank(node_comm) failed";
        goto out;
    }

    rc = create_intrinsic_groups(mpi);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_groups() failed";
        goto out;
    }
    // Task initialization
    rc = qvi_task_init(task, getpid(), world_id, node_id);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_init() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }
    mpi->task = task;
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

int
qvi_mpi_finalize(
    qvi_mpi_t *
) {
    return QV_SUCCESS;
}

int
qvi_mpi_group_new(
    qvi_mpi_group_t **group
) {
    int rc = QV_SUCCESS;

    qvi_mpi_group_t *igroup = qvi_new qvi_mpi_group_t;
    if (!igroup) rc = QV_ERR_OOR;

    *group = igroup;
    return rc;
}

void
qvi_mpi_group_free(
    qvi_mpi_group_t **group
) {
    if (!group) return;
    qvi_mpi_group_t *igroup = *group;
    if (!igroup) goto out;
    delete igroup;
out:
    *group = nullptr;
}

int
qvi_mpi_group_size(
    const qvi_mpi_group_t *group
) {
    return group->group_size;
}

int
qvi_mpi_group_id(
    const qvi_mpi_group_t *group
) {
    return group->group_id;
}

int
qvi_mpi_group_lookup_by_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t id,
    qvi_mpi_group_t *group
) {
    auto got = mpi->group_tab->find(id);
    if (got == mpi->group_tab->end()) {
        return QV_ERR_NOT_FOUND;
    }
    cp_mpi_group(&got->second, group);
    return QV_SUCCESS;
}

int
qvi_mpi_group_create_from_group_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t id,
    qvi_mpi_group_t **group
) {
    qvi_mpi_group_t *tmp_group;
    int rc = qvi_mpi_group_new(&tmp_group);
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_mpi_group_lookup_by_id(mpi, id, tmp_group);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_mpi_group_create_from_mpi_comm(
        mpi,
        tmp_group->mpi_comm,
        group
    );
out:
    qvi_mpi_group_free(&tmp_group);
    return rc;
}

int
qvi_mpi_group_create_from_ids(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *group,
    int num_group_ids,
    const int *group_ids,
    qvi_mpi_group_t **maybe_group
) {
    cstr ers = nullptr;
    int qvrc = QV_SUCCESS;

    MPI_Group new_mpi_group;
    int rc = MPI_Group_incl(
        group->mpi_group,
        num_group_ids,
        group_ids,
        &new_mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_incl() failed";
        qvrc = QV_ERR_MPI;
        goto out;
    }

    qvrc = group_create_from_mpi_group(
        mpi,
        new_mpi_group,
        maybe_group
    );
    if (qvrc != QV_SUCCESS) {
        ers = "group_create_from_mpi_group() failed";
        goto out;
    }
    // Not in the group and no errors.
    if (*maybe_group == nullptr) {
        return QV_SUCCESS;
    }
    // In the group.
    qvrc = group_add(mpi, *maybe_group);
    if (qvrc != QV_SUCCESS) {
        ers = "group_add() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return qvrc;
}

int
qvi_mpi_group_create_from_split(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *parent,
    int color,
    int key,
    qvi_mpi_group_t **child
) {
    int qvrc = QV_SUCCESS;

    MPI_Comm split_comm = MPI_COMM_NULL;
    int rc = MPI_Comm_split(
        parent->mpi_comm,
        color,
        key,
        &split_comm
    );
    if (rc != MPI_SUCCESS) {
        qvrc = QV_ERR_MPI;
        goto out;
    }

    qvrc = qvi_mpi_group_create_from_mpi_comm(
        mpi,
        split_comm,
        child
    );
out:
    if (split_comm != MPI_COMM_NULL) MPI_Comm_free(&split_comm);
    return qvrc;
}

int
qvi_mpi_group_create_from_mpi_comm(
    qvi_mpi_t *mpi,
    MPI_Comm comm,
    qvi_mpi_group_t **new_group
) {
    cstr ers = nullptr;
    MPI_Comm node_comm = MPI_COMM_NULL;

    int rc = qvi_mpi_group_new(new_group);
    if (rc != QV_SUCCESS) return rc;

    rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (rc != MPI_SUCCESS) {
        ers = "mpi_comm_to_new_node_comm() failed";
        rc = QV_ERR_MPI;
        goto out;
    }

    rc = group_create_from_mpi_comm(
        node_comm,
        *new_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm() failed";
        goto out;
    }

    rc = group_add(mpi, *new_group);
    if (rc != QV_SUCCESS) {
        ers = "group_add() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_mpi_group_free(new_group);
        if (node_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&node_comm);
        }
    }
    return rc;
}

/**
 *
 */
static int
sleepy_node_barrier(
    MPI_Comm node_comm
) {
    MPI_Request request;
    int rc = MPI_Ibarrier(node_comm, &request);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;

    int done = 0;
    do {
        rc = MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        if (rc != MPI_SUCCESS) return QV_ERR_MPI;
        usleep(50000);
    } while (!done);

    return QV_SUCCESS;
}

int
qvi_mpi_node_barrier(
    qvi_mpi_t *mpi
) {
    return sleepy_node_barrier(mpi->node_comm);
}

int
qvi_mpi_group_barrier(
    qvi_mpi_group_t *group
) {
    return sleepy_node_barrier(group->mpi_comm);
}

int
qvi_mpi_group_gather_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs
) {
    const int send_count = (int)qvi_bbuff_size(txbuff);
    const int group_id = group->group_id;
    const int group_size = group->group_size;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS;
    int *rxcounts = nullptr, *displs = nullptr;
    byte *allbytes = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;

    if (group_id == root) {
        rxcounts = qvi_new int[group_size];
        if (!rxcounts) {
            rc = QV_ERR_OOR;
            goto out;
        }
    }
    // Figure out now much data are sent by each participant.
    mpirc = MPI_Gather(
        &send_count, 1, MPI_INT,
        rxcounts, 1, MPI_INT,
        root, group->mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Root sets up relevant Gatherv data structures.
    if (group_id == root) {
        displs = qvi_new int[group_size];
        if (!displs) {
            rc = QV_ERR_OOR;
            goto out;
        };

        int displ = 0;
        for (int i = 0; i < group_size; ++i) {
            displs[i] = displ;
            displ += rxcounts[i];
        }

        int total_bytes = 0;
        for (int i = 0; i < group_size; ++i) {
            total_bytes += rxcounts[i];
        }

        allbytes = qvi_new byte[total_bytes];
        if (!allbytes) {
            rc = QV_ERR_OOR;
            goto out;
        };
    }

    mpirc = MPI_Gatherv(
        qvi_bbuff_data(txbuff), send_count, MPI_UINT8_T,
        allbytes, rxcounts, displs, MPI_UINT8_T,
        root, group->mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Root creates new buffers from data gathered from each participant.
    if (group_id == root) {
        bbuffs = qvi_new qvi_bbuff_t*[group_size];
        if (!bbuffs) {
            rc = QV_ERR_OOR;
            goto out;
        }
        std::fill(bbuffs, bbuffs + group_size, nullptr);

        byte *bytepos = allbytes;
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&bbuffs[i]);
            if (rc != QV_SUCCESS) break;
            rc = qvi_bbuff_append(bbuffs[i], bytepos, rxcounts[i]);
            if (rc != QV_SUCCESS) break;
            bytepos += rxcounts[i];
        }
    }
out:
    if (rxcounts) delete[] rxcounts;
    if (displs) delete[] displs;
    if (allbytes) delete[] allbytes;
    if (rc != QV_SUCCESS) {
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    return rc;
}

int
qvi_mpi_group_scatter_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    const int group_size = group->group_size;
    const int group_id = group->group_id;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS, rxcount = 0;
    int *txcounts = nullptr, *displs = nullptr, total_bytes = 0;
    byte *mybytes = nullptr, *txbytes = nullptr;
    qvi_bbuff_t *mybbuff = nullptr;

    if (group_id == root) {
        txcounts = qvi_new int[group_size];
        if (!txcounts) {
            rc = QV_ERR_OOR;
            goto out;
        }
        for (int i = 0; i < group_size; ++i) {
            txcounts[i] = (int)qvi_bbuff_size(txbuffs[i]);
            total_bytes += txcounts[i];
        }
    }
    // Scatter buffer sizes.
    mpirc = MPI_Scatter(
        txcounts, 1, MPI_INT,
        &rxcount, 1, MPI_INT,
        root, group->mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Root sets up relevant Scatterv data structures.
    if (group->group_id == root) {
        displs = qvi_new int[group_size];
        if (!displs) {
            rc = QV_ERR_OOR;
            goto out;
        };

        int displ = 0;
        for (int i = 0; i < group_size; ++i) {
            displs[i] = displ;
            displ += txcounts[i];
        }
        // A flattened buffer containing all the relevant information.
        txbytes = qvi_new byte[total_bytes];
        if (!txbytes) {
            rc = QV_ERR_OOR;
            goto out;
        };
        // Copy buffer data into flattened buffer.
        byte *bytepos = txbytes;
        for (int i = 0; i < group_size; ++i) {
            memmove(bytepos, txbuffs[i], txcounts[i]);
            bytepos += txcounts[i];
        }
    }
    // Everyone allocates a buffer for their data.
    mybytes = qvi_new byte[rxcount];
    if (!mybytes) {
        rc = QV_ERR_OOR;
        goto out;
    };

    mpirc = MPI_Scatterv(
        txbytes, txcounts, displs, MPI_UINT8_T,
        mybytes, rxcount, MPI_UINT8_T,
        root, group->mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Everyone creates new buffers from data received from root.
    rc = qvi_bbuff_new(&mybbuff);
    if (rc != QV_SUCCESS) goto out;
    rc = qvi_bbuff_append(mybbuff, mybytes, rxcount);
out:
    if (txcounts) delete[] txcounts;
    if (displs) delete[] displs;
    if (txbytes) delete[] txbytes;
    if (mybytes) delete[] mybytes;
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

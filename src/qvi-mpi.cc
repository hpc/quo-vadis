/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

#include "qvi-mpi.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"

/**
 * Performs a low-noise, high latency node-level barrier across the given
 * communicator.
 */
static int
sleepy_node_barrier(
    MPI_Comm node_comm
) {
    MPI_Request request;
    int rc = MPI_Ibarrier(node_comm, &request);
    if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;

    int done = 0;
    do {
        rc = MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;
        std::this_thread::sleep_for(std::chrono::microseconds(50000));
    } while (!done);

    return QV_SUCCESS;
}

std::vector<pid_t>
qvi_mpi_group::pids(void) const
{
    static_assert(
        sizeof(int) == sizeof(pid_t),
        "int and pid_t must have the same size."
    );

    const pid_t mypid = getpid();
    std::vector<pid_t> allpids(qvcomm.m_size);

    const int mpirc = MPI_Allgather(
        &mypid, 1, MPI_INT, allpids.data(),
        1, MPI_INT, qvcomm.m_mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
        throw qvi_runtime_error(QV_ERR_MPI);
    }
    return allpids;
}

int
qvi_mpi_group::barrier(void) const
{
    return sleepy_node_barrier(qvcomm.m_mpi_comm);
}

int
qvi_mpi_group::gather_bbuffs(
    const qvi_bbuff &txbuff,
    int root,
    std::vector<qvi_bbuff> &rxbuffs
) const {
    const int send_count = (int)txbuff.size();
    const int group_id = qvcomm.m_rank;
    const int group_size = qvcomm.m_size;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS;
    std::vector<int> rxcounts, displs;
    std::vector<byte_t> allbytes;

    if (group_id == root) {
        rxcounts.resize(group_size);
    }
    // Figure out now much data are sent by each participant.
    mpirc = MPI_Gather(
        &send_count, 1, MPI_INT,
        rxcounts.data(), 1, MPI_INT,
        root, qvcomm.m_mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Root sets up relevant Gatherv data structures.
    if (group_id == root) {
        displs.resize(group_size);

        int total_bytes = 0;
        for (int i = 0; i < group_size; ++i) {
            displs[i] = total_bytes;
            total_bytes += rxcounts[i];
        }
        allbytes.resize(total_bytes);
    }

    mpirc = MPI_Gatherv(
        txbuff.cdata(), send_count, MPI_UINT8_T,
        allbytes.data(), rxcounts.data(), displs.data(),
        MPI_UINT8_T, root, qvcomm.m_mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Root creates new buffers from data gathered from each participant.
    if (group_id == root) {
        rxbuffs.resize(group_size);
        byte_t *bytepos = allbytes.data();
        for (int i = 0; i < group_size; ++i) {
            rc = rxbuffs[i].append(bytepos, rxcounts[i]);
            if (rc != QV_SUCCESS) return rc;
            bytepos += rxcounts[i];
        }
    }
    return QV_SUCCESS;
}

int
qvi_mpi_group::scatter_bbuffs(
    const std::vector<qvi_bbuff> &txbuffs,
    int root,
    qvi_bbuff &rxbuff
) const {
    const int group_size = qvcomm.m_size;
    const int group_id = qvcomm.m_rank;

    int mpirc = MPI_SUCCESS, rxcount = 0;
    int total_bytes = 0;
    std::vector<int> txcounts, displs;
    std::vector<byte_t> mybytes, txbytes;
    // Root sets up relevant Scatterv data structures.
    if (group_id == root) {
        txcounts.resize(group_size);
        displs.resize(group_size);

        for (int i = 0; i < group_size; ++i) {
            txcounts[i] = (int)txbuffs[i].size();
            displs[i] = total_bytes;
            total_bytes += txcounts[i];
        }
        // A flattened buffer containing all the relevant information.
        txbytes.resize(total_bytes);
        // Copy buffer data into flattened buffer.
        byte_t *bytepos = txbytes.data();
        for (int i = 0; i < group_size; ++i) {
            memmove(bytepos, txbuffs[i].cdata(), txcounts[i]);
            bytepos += txcounts[i];
        }
    }
    // Scatter buffer sizes.
    mpirc = MPI_Scatter(
        txcounts.data(), 1, MPI_INT,
        &rxcount, 1, MPI_INT,
        root, qvcomm.m_mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Everyone allocates a buffer for their data.
    mybytes.resize(rxcount);

    mpirc = MPI_Scatterv(
        txbytes.data(), txcounts.data(), displs.data(), MPI_UINT8_T,
        mybytes.data(), rxcount, MPI_UINT8_T, root, qvcomm.m_mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;

    return rxbuff.append(mybytes.data(), rxcount);
}

int
qvi_mpi_group::comm_dup(
    MPI_Comm *comm
) const {
    const int rc = MPI_Comm_dup(qvcomm.m_mpi_comm, comm);
    if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;
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
    const int rc = MPI_Comm_split_type(
        comm, MPI_COMM_TYPE_SHARED,
        0, MPI_INFO_NULL, node_comm
    );
    if (qvi_unlikely(rc != MPI_SUCCESS)) {
        qvi_log_error("MPI_Comm_split_type(MPI_COMM_TYPE_SHARED) failed");
        *node_comm = MPI_COMM_NULL;
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

/**
 * Initializes a QV group from an MPI communicator.
 */
static int
group_init_from_mpi_comm(
    MPI_Comm comm,
    qvi_mpi_group &group
) {
    group.qvcomm = qvi_mpi_comm(comm, false);
    return QV_SUCCESS;
}

/**
 * Creates intrinsic communicators.
 */
int
qvi_mpi::m_create_intrinsic_comms(
    MPI_Comm comm
) {
    // Create node communicator.
    MPI_Comm node_comm = MPI_COMM_NULL;
    const int rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // MPI_COMM_SELF duplicate.
    m_self_comm = qvi_mpi_comm(MPI_COMM_SELF, true);
    // Node communicator, no duplicate necessary here: created above.
    m_node_comm = qvi_mpi_comm(node_comm, false);
    // 'World' (aka initializing communicator) duplicate.
    m_world_comm = qvi_mpi_comm(comm, true);
    return QV_SUCCESS;
}

int
qvi_mpi::m_create_intrinsic_groups(void)
{
    int rc = add_group(qvi_mpi_group(m_self_comm), QVI_MPI_GROUP_SELF);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = add_group(qvi_mpi_group(m_node_comm), QVI_MPI_GROUP_NODE);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return add_group(qvi_mpi_group(m_world_comm), QVI_MPI_GROUP_WORLD);
}

int
qvi_mpi::m_create_admin_comms(void)
{
    assert(m_node_comm.m_mpi_comm != MPI_COMM_NULL);
    // Create a communicator that has node rank zero members in it. We will call
    // those processes 'node representatives.'
    const int color = (m_node_comm.m_rank == 0) ? 0 : MPI_UNDEFINED;
    MPI_Comm node_rep_comm;
    int mpirc = MPI_Comm_split(
        m_node_comm.m_mpi_comm, color,
        m_world_comm.m_rank, &node_rep_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Part of the node representatives communicator.
    if (node_rep_comm != MPI_COMM_NULL) {
        m_node_rep_comm = qvi_mpi_comm(node_rep_comm, false);
        // Add the comm to the group.
        return add_group(m_node_rep_comm);
    }
    return QV_SUCCESS;
}


int
qvi_mpi::get_portno_from_env(
    const qvi_mpi_comm &comm,
    int &portno
) {
    assert(comm.m_mpi_comm != MPI_COMM_NULL);

    int rc = QV_SUCCESS;
    do {
        MPI_Comm rep_comm = comm.m_mpi_comm;
        // Is QV_PORT set on this node, in my environment?
        const bool envset = qvi_envset(QVI_ENV_PORT);
        // See if any processes have QV_PORT set.
        bool anyset = 0;
        // See if QV_PORT is set on any of the nodes, while counting them.
        int mpirc = MPI_Allreduce(
            &envset, &anyset, 1, MPI_CXX_BOOL, MPI_LOR, rep_comm
        );
        if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
            rc = QV_ERR_MPI;
            break;
        }
        if (anyset) {
            int envport = QVI_PORT_UNSET;
            // If this fails, envport is set to QVI_COMM_PORT_UNSET.
            (void)qvi_port_from_env(envport);

            std::vector<int> portnos(comm.m_size);
            mpirc = MPI_Allgather(
                &envport, 1, MPI_INT,
                portnos.data(),
                1, MPI_INT, rep_comm
            );
            if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
                rc = QV_ERR_MPI;
                break;
            }
            // Pick one from the group.
            for (const auto pn : portnos) {
                if (pn == QVI_PORT_UNSET) continue;
                portno = pn;
            }
        }
    } while (false);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        portno = QVI_PORT_UNSET;
    }
    return rc;
}

int
qvi_mpi::m_start_daemons(void)
{
    int rc = QV_SUCCESS, ret = QV_SUCCESS;
    do {
        // Default port.
        int portno = QVI_PORT_UNSET;
        // Not a node representative, so wait in barrier below.
        if (m_node_rep_comm.m_mpi_comm == MPI_COMM_NULL) break;
        // The rest do all the work.
        rc = get_portno_from_env(m_node_rep_comm, portno);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        // Try to use the requested port.
        if (portno != QVI_PORT_UNSET) {
            // If there is a session directory using the given port, use it.
            if (qvi_session_exists(portno)) break;
            // Else start up the daemon using the requested port.
            rc = qvi_start_qvd(portno);
            // Done!
            break;
        }
        // No port requested, so try to discover one.
        rc = qvi_session_discover(10, portno);
        // Found one, so use it.
        if (rc == QV_SUCCESS) break;
        // Else, start up the daemon using our default port number.
        rc = qvi_start_qvd(57550);
    } while (false);
    // See if any errors happened above, but all barrier to avoid hangs.
    if (qvi_unlikely(rc != QV_SUCCESS)) ret = rc;
    rc = sleepy_node_barrier(m_node_comm.m_mpi_comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) ret = rc;
    return ret;
}

qvi_mpi::qvi_mpi(
    MPI_Comm comm
) {
    // If MPI isn't initialized, then we can't continue.
    int inited = 0;
    const int mpirc = MPI_Initialized(&inited);
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
        throw qvi_runtime_error(QV_ERR_MPI);
    }
    if (qvi_unlikely(!inited)) {
        const cstr_t ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        throw qvi_runtime_error(QV_ERR_MPI);
    }

    int rc = m_create_intrinsic_comms(comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        throw qvi_runtime_error(rc);
    }

    rc = m_create_intrinsic_groups();
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        throw qvi_runtime_error(rc);
    }

    rc = m_create_admin_comms();
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        throw qvi_runtime_error(rc);
    }

    rc = m_start_daemons();
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        throw qvi_runtime_error(rc);
    }
}

qvi_mpi::~qvi_mpi(void)
{
    for (auto &i : m_group_tab) {
        qvi_mpi_comm::free(i.second.qvcomm);
    }
}

int
qvi_mpi::add_group(
    const qvi_mpi_group &group,
    qvi_group_id_t given_id
) {
    qvi_group_id_t gid = given_id;
    // Marker used to differentiate between intrinsic and automatic IDs.
    if (given_id == QVI_MPI_GROUP_NULL) {
        const int rc = qvi_group::next_id(&gid);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    m_group_tab.insert({gid, group});
    return QV_SUCCESS;
}

int
qvi_mpi::group_from_group_id(
    qvi_group_id_t id,
    qvi_mpi_group &group
) {
    auto got = m_group_tab.find(id);
    if (qvi_unlikely(got == m_group_tab.end())) {
        group = {};
        return QV_ERR_NOT_FOUND;
    }
    group = got->second;
    return QV_SUCCESS;
}

int
qvi_mpi::group_from_split(
    const qvi_mpi_group &parent,
    int color,
    int key,
    qvi_mpi_group &child
) {
    int rc = QV_SUCCESS;
    MPI_Comm split_comm = MPI_COMM_NULL;

    do {
        const int mpirc = MPI_Comm_split(
            parent.qvcomm.m_mpi_comm, color, key, &split_comm
        );
        if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
            rc = QV_ERR_MPI;
            break;
        }

        rc = group_from_mpi_comm(split_comm, child);
    } while (false);

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        child = {};
    }

    if (split_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&split_comm);
    }
    return rc;
}

int
qvi_mpi::group_from_mpi_comm(
    MPI_Comm comm,
    qvi_mpi_group &group
) {
    int rc = QV_SUCCESS;
    MPI_Comm node_comm = MPI_COMM_NULL;

    do {
        rc = mpi_comm_to_new_node_comm(comm, &node_comm);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;

        rc = group_init_from_mpi_comm(node_comm, group);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;

        rc = add_group(group);
    } while (false);

    if (rc != QV_SUCCESS) {
        group = {};
        if (node_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&node_comm);
        }
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

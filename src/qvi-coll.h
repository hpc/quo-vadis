/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-coll.h
 */

#ifndef QVI_COLL_H
#define QVI_COLL_H

#include "qvi-common.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-group.h"

namespace qvi_coll {

template <typename TYPE>
int
gather(
    const qvi_group &group,
    int rootid,
    const TYPE &send,
    std::vector<TYPE> &recv
) {
    const uint_t group_size = group.size();
    // Pack the hardware pool into a buffer.
    qvi_bbuff txbuff;
    int rc = qvi_bbuff_rmi_pack(&txbuff, send);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Gather the values to the root.
    qvi_bbuff_alloc_type_t alloc_type = QVI_BBUFF_ALLOC_PRIVATE;
    qvi_bbuff **bbuffs = nullptr;
    rc = group.gather(&txbuff, rootid, &alloc_type, &bbuffs);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    if (group.rank() == rootid) {
        recv.resize(group_size);
        // Unpack the data.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_rmi_unpack(
                bbuffs[i]->data(), &recv[i]
            );
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
    }
out:
    if ((QVI_BBUFF_ALLOC_PRIVATE == alloc_type) ||
        ((QVI_BBUFF_ALLOC_SHARED == alloc_type) &&
          (group.rank() == rootid))) {
        if (bbuffs) {
            for (uint_t i = 0; i < group_size; ++i) {
                qvi_delete(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
    }
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        // If something went wrong, just zero-initialize the pools.
        recv = {};
    }
    return rc;
}

template <typename TYPE>
int
scatter(
    const qvi_group &group,
    int rootid,
    const std::vector<TYPE> &send,
    TYPE &recv
) {
    static_assert(
        std::is_trivially_copyable<TYPE>::value &&
        !std::is_pointer<TYPE>::value, ""
    );

    int rc = QV_SUCCESS;
    qvi_bbuff *rxbuff = nullptr;

    std::vector<qvi_bbuff *> txbuffs(0);
    if (group.rank() == rootid) {
        const uint_t group_size = group.size();
        txbuffs.resize(group_size);
        // Pack the values.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_new(&txbuffs[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;

            rc = txbuffs[i]->append(&send[i], sizeof(TYPE));
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    }

    rc = group.scatter(txbuffs.data(), rootid, &rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        goto out;
    }

    recv = *(TYPE *)rxbuff->data();
out:
    for (auto &buff : txbuffs) {
        qvi_delete(&buff);
    }
    qvi_delete(&rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        // If something went wrong, just zero-initialize the value.
        recv = {};
    }
    return rc;
}

template <class CLASS>
int
scatter(
    const qvi_group &group,
    int rootid,
    const std::vector<CLASS> &send,
    CLASS **recv
) {
    static_assert(!std::is_pointer<CLASS>::value, "");

    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff *> txbuffs(0);
    qvi_bbuff *rxbuff = nullptr;

    if (group.rank() == rootid) {
        const uint_t group_size = group.size();
        txbuffs.resize(group_size);
        // Pack the class data.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_new(&txbuffs[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;

            rc = send[i].packinto(txbuffs[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    }

    rc = group.scatter(txbuffs.data(), rootid, &rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

    rc = qvi_bbuff_rmi_unpack(rxbuff->data(), recv);
out:
    for (auto &buff : txbuffs) {
        qvi_delete(&buff);
    }
    qvi_delete(&rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(recv);
    }
    return rc;
}

template <typename TYPE>
int
bcast(
    const qvi_group &group,
    int rootid,
    TYPE &value
) {
    static_assert(
        std::is_trivially_copyable<TYPE>::value &&
        !std::is_pointer<TYPE>::value, ""
    );

    std::vector<TYPE> values;
    if (group.rank() == rootid) {
        values.resize(group.size());
        std::fill(values.begin(), values.end(), value);
    }
    return qvi_coll::scatter(group, rootid, values, value);
}

} // qvi_coll namespace

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

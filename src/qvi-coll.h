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
#include "qvi-bbuff.h"
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
    // Pack the send type into a buffer.
    qvi_bbuff txbuff;
    int rc = txbuff.pack(send);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    std::vector<qvi_bbuff> bbuffs;
    rc = group.gather(txbuff, rootid, bbuffs);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    if (group.rank() == rootid) {
        recv.resize(group_size);
        // Unpack the data.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = qvi_bbuff::unpack(bbuffs[i].data(), recv[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        }
    }
    return QV_SUCCESS;
}

template <typename TYPE>
int
scatter(
    const qvi_group &group,
    int rootid,
    const std::vector<TYPE> &send,
    TYPE &recv
) {
    static_assert(!std::is_pointer<TYPE>::value, "");

    int rc = QV_SUCCESS;
    std::vector<qvi_bbuff> txbuffs;

    if (group.rank() == rootid) {
        const uint_t group_size = group.size();
        txbuffs.resize(group_size);
        // Pack the data.
        for (uint_t i = 0; i < group_size; ++i) {
            rc = txbuffs[i].pack(send[i]);
            if (qvi_unlikely(rc != QV_SUCCESS)) break;
        }
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }

    qvi_bbuff rxbuff;
    rc = group.scatter(txbuffs, rootid, rxbuff);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Unpack the results.
    return qvi_bbuff::unpack(rxbuff.data(), recv);
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

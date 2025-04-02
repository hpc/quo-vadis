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
#include "qvi-hwpool.h"
#include "qvi-bbuff-rmi.h"


/**
 * Collective hardware split: a collection of data and operations relevant to
 * split operations requiring aggregated resource knowledge AND coordination
 * between tasks in the parent scope to perform a split.
 */
// TODO(skg) Make group const
struct qvi_coll {
    /** Constructor. */
    qvi_coll(void) = delete;

    template <typename TYPE>
    static int
    scatter_values(
        qvi_group &group,
        int rootid,
        const std::vector<TYPE> &values,
        TYPE *value
    ) {
        static_assert(std::is_trivially_copyable<TYPE>::value, "");

        int rc = QV_SUCCESS;
        qvi_bbuff *rxbuff = nullptr;

        std::vector<qvi_bbuff *> txbuffs(0);
        if (group.rank() == rootid) {
            const uint_t group_size = group.size();
            txbuffs.resize(group_size);
            // Pack the values.
            for (uint_t i = 0; i < group_size; ++i) {
                rc = qvi_bbuff_new(&txbuffs[i]);
                if (qvi_unlikely(rc != QV_SUCCESS)) break;

                rc = txbuffs[i]->append(&values[i], sizeof(TYPE));
                if (qvi_unlikely(rc != QV_SUCCESS)) break;
            }
            if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
        }

        rc = group.scatter(txbuffs.data(), rootid, &rxbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            goto out;
        }

        *value = *(TYPE *)rxbuff->data();
    out:
        for (auto &buff : txbuffs) {
            qvi_bbuff_delete(&buff);
        }
        qvi_bbuff_delete(&rxbuff);
        if (rc != QV_SUCCESS) {
            // If something went wrong, just zero-initialize the value.
            *value = {};
        }
        return rc;
    }

    template <typename TYPE>
    static int
    bcast_value(
        qvi_group &group,
        int rootid,
        TYPE *value
    ) {
        static_assert(std::is_trivially_copyable<TYPE>::value, "");

        std::vector<TYPE> values;
        if (group.rank() == rootid) {
            values.resize(group.size());
            std::fill(values.begin(), values.end(), *value);
        }
        return scatter_values(group, rootid, values, value);
    }

    template <typename TYPE>
    static int
    gather_values(
        qvi_group &group,
        int rootid,
        TYPE invalue,
        std::vector<TYPE> &outvals
    ) {
        static_assert(std::is_trivially_copyable<TYPE>::value, "");

        const uint_t group_size = group.size();

        qvi_bbuff *txbuff = nullptr;
        int rc = qvi_bbuff_new(&txbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

        rc = txbuff->append(&invalue, sizeof(TYPE));
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_bbuff_delete(&txbuff);
            return rc;
        }
        // Gather the values to the root.
        qvi_bbuff_alloc_type_t alloc_type = QVI_BBUFF_ALLOC_PRIVATE;
        qvi_bbuff **bbuffs = nullptr;
        rc = group.gather(txbuff, rootid, &alloc_type, &bbuffs);
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
        // The root fills in the output.
        if (group.rank() == rootid) {
            outvals.resize(group_size);
            // Unpack the values.
            for (uint_t i = 0; i < group_size; ++i) {
                outvals[i] = *(TYPE *)bbuffs[i]->data();
            }
        }
    out:
        if ((QVI_BBUFF_ALLOC_PRIVATE == alloc_type) ||
            ((QVI_BBUFF_ALLOC_SHARED == alloc_type) && (group.rank() == rootid))) {
            if (bbuffs) {
               for (uint_t i = 0; i < group_size; ++i) {
                    qvi_bbuff_delete(&bbuffs[i]);
                }
                delete[] bbuffs;
            }
        }

        qvi_bbuff_delete(&txbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            // If something went wrong, just zero-initialize the values.
            outvals = {};
        }
        return rc;
    }

    static int
    gather_hwpools(
        qvi_group &group,
        int rootid,
        const qvi_hwpool &txpool,
        std::vector<qvi_hwpool *> &rxpools
    ) {
        const uint_t group_size = group.size();
        // Pack the hardware pool into a buffer.
        qvi_bbuff txbuff;
        int rc = txpool.packinto(&txbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        // Gather the values to the root.
        qvi_bbuff_alloc_type_t alloc_type = QVI_BBUFF_ALLOC_PRIVATE;
        qvi_bbuff **bbuffs = nullptr;
        rc = group.gather(&txbuff, rootid, &alloc_type, &bbuffs);
        if (rc != QV_SUCCESS) goto out;

        if (group.rank() == rootid) {
            rxpools.resize(group_size);
            // Unpack the hwpools.
            for (uint_t i = 0; i < group_size; ++i) {
                rc = qvi_bbuff_rmi_unpack(
                    bbuffs[i]->data(), &rxpools[i]
                );
                if (qvi_unlikely(rc != QV_SUCCESS)) break;
            }
        }
    out:
        if ((QVI_BBUFF_ALLOC_PRIVATE == alloc_type) ||
            ((QVI_BBUFF_ALLOC_SHARED == alloc_type) && (group.rank() == rootid))) {
            if (bbuffs) {
                for (uint_t i = 0; i < group_size; ++i) {
                    qvi_bbuff_delete(&bbuffs[i]);
                }
                delete[] bbuffs;
            }
        }

        if (rc != QV_SUCCESS) {
            // If something went wrong, just zero-initialize the pools.
            rxpools = {};
        }
        return rc;
    }


    static int
    scatter_hwpools(
        qvi_group &group,
        int rootid,
        const std::vector<qvi_hwpool *> &pools,
        qvi_hwpool **pool
    ) {
        int rc = QV_SUCCESS;
        std::vector<qvi_bbuff *> txbuffs(0);
        qvi_bbuff *rxbuff = nullptr;

        if (group.rank() == rootid) {
            const uint_t group_size = group.size();
            txbuffs.resize(group_size);
            // Pack the hwpools.
            for (uint_t i = 0; i < group_size; ++i) {
                rc = qvi_bbuff_new(&txbuffs[i]);
                if (qvi_unlikely(rc != QV_SUCCESS)) break;

                rc = pools[i]->packinto(txbuffs[i]);
                if (qvi_unlikely(rc != QV_SUCCESS)) break;
            }
            if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
        }

        rc = group.scatter(txbuffs.data(), rootid, &rxbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;

        rc = qvi_bbuff_rmi_unpack(rxbuff->data(), pool);
    out:
        for (auto &buff : txbuffs) {
            qvi_bbuff_delete(&buff);
        }
        qvi_bbuff_delete(&rxbuff);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_delete(pool);
        }
        return rc;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

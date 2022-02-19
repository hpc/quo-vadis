/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-devinfo.cc
 *
 * Device information.
 */

#ifndef QVI_DEVINFO_H
#define QVI_DEVINFO_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

/** Device information. */
struct qvi_devinfo_t {
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device ID. */
    int id = 0;
    /** The bitmap encoding CPU affinity. */
    hwloc_bitmap_t affinity = nullptr;
    /** Constructor */
    qvi_devinfo_t(
        qv_hw_obj_type_t t,
        int i,
        hwloc_const_cpuset_t c
    ) : type(t)
      , id(i)
      , affinity(nullptr)
    {
        (void)qvi_hwloc_bitmap_calloc(&affinity);
        (void)qvi_hwloc_bitmap_copy(c, affinity);
    }
    /** Destructor */
    ~qvi_devinfo_t(void)
    {
        qvi_hwloc_bitmap_free(&affinity);
    }
    /** Equality operator. */
    bool
    operator==(const qvi_devinfo_t &x) const
    {
        return id == x.id && type == x.type;
    }
};

/**
 * Extend namespace std so we can easily add qvi_devinfo_ts to
 * unordered_sets.
 */
namespace std {
    template <>
    struct hash<qvi_devinfo_t>
    {
        size_t
        operator()(const qvi_devinfo_t &x) const
        {
            const int a = x.id;
            const int b = (int)x.type;
            // Cantor pairing function.
            const int64_t c = (a + b) * (a + b + 1) / 2 + b;
            return hash<int64_t>()(c);
        }
    };
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */

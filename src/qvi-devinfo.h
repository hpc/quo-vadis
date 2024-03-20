/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
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
#include "qvi-utils.h"

/** Device information. */
struct qvi_devinfo_t {
    int qvim_rc = QV_ERR_INTERNAL;
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device ID. */
    int id = 0;
    /** The PCI bus ID. */
    char *pci_bus_id = nullptr;
    /** UUID */
    char *uuid = nullptr;
    /** The bitmap encoding CPU affinity. */
    hwloc_bitmap_t affinity = nullptr;
    /** Constructor */
    qvi_devinfo_t(
        qv_hw_obj_type_t t,
        int i,
        cstr_t pci_bus_id,
        cstr_t uuid,
        hwloc_const_cpuset_t c
    ) : type(t)
      , id(i)
    {
        int nw = asprintf(&this->pci_bus_id, "%s", pci_bus_id);
        if (nw == -1) {
            qvim_rc = QV_ERR_OOR;
            return;
        }

        nw = asprintf(&this->uuid, "%s", uuid);
        if (nw == -1) {
            qvim_rc = QV_ERR_OOR;
            return;
        }

        qvim_rc = qvi_hwloc_bitmap_dup(c, &affinity);
    }
    /** Destructor */
    ~qvi_devinfo_t(void)
    {
        qvi_hwloc_bitmap_free(&affinity);
        free(pci_bus_id);
        free(uuid);
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
            const int64_t c = qvi_cantor_pairing(a, b);
            return hash<int64_t>()(c);
        }
    };
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

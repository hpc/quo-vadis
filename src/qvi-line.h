/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-line.h
 *
 * Line types and functions for sending and receiving data over the network.
 * More generally, they are types that can be easily serialized.
 */

#ifndef QVI_LINE_H
#define QVI_LINE_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-bbuff.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qvi_line_config_s {
    /** Not sent, initialized elsewhere. */
    qvi_hwloc_t *hwloc = nullptr;
    /** Connection URL. */
    char *url = nullptr;
    /** Path to hardware topology file. */
    char *hwtopo_path = nullptr;
} qvi_line_config_t;

/** Device information struct for line transmission. */
typedef struct qvi_line_devinfo_s {
    /** The bitmap encoding device affinity. */
    qvi_hwloc_bitmap_s affinity;
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device ID. */
    int id = 0;
    /** PCI bus ID. */
    std::string pci_bus_id;
    /** UUID */
    std::string uuid;
} qvi_line_devinfo_t;

/** Hardware pool data structure for line transmission. */
typedef struct qvi_line_hwpool_s {
    /** The cpuset of this resource pool. */
    qvi_hwloc_bitmap_s cpuset;
    /** Number of devinfos. */
    int ndevinfos = 0;
    /** Vector of device infos. */
    std::vector<qvi_line_devinfo_t> devinfos;
} qvi_line_hwpool_t;

/**
 *
 */
int
qvi_line_config_new(
    qvi_line_config_t **config
);

/**
 *
 */
void
qvi_line_config_free(
    qvi_line_config_t **config
);

/**
 *
 */
int
qvi_line_config_cp(
    qvi_line_config_t *from,
    qvi_line_config_t *to
);

/**
 *
 */
int
qvi_line_config_pack(
    qvi_line_config_t *config,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_line_config_unpack(
    void *buff,
    qvi_line_config_t *config
);

/**
 *
 */
void
qvi_line_devinfo_free(
    qvi_line_devinfo_t *devinfo
);

/**
 *
 */
int
qvi_line_hwpool_new(
    qvi_line_hwpool_t **hws
);

/**
 *
 */
void
qvi_line_hwpool_free(
    qvi_line_hwpool_t **hws
);

/**
 *
 */
int
qvi_line_hwpool_pack(
    qvi_line_hwpool_t *hws,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_line_hwpool_unpack(
    void *buff,
    qvi_line_hwpool_t **hws
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

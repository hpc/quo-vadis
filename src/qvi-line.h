/*
 * Copyright (c)      2022 Triad National Security, LLC
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

#include "qvi-common.h"
#include "qvi-bbuff.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Keep in sync with the qvi_line_config_t structure.
#define QVI_LINE_CONFIG_PICTURE "ss"
#define QVI_LINE_HWPOOL_PICTURE "h"

/** Sentinel value last value for device ID lists. */
const qvi_device_id_t qvi_line_hwpool_devid_last = -1;

typedef struct qvi_line_config_s {
    /** Not sent, initialized elsewhere. */
    qvi_hwloc_t *hwloc;
    /** Connection URL. */
    char *url;
    /** Path to hardware topology file. */
    char *hwtopo_path;
} qvi_line_config_t;

typedef struct qvi_line_hwpool_s {
    /** The cpuset of this resource pool. */
    hwloc_bitmap_t cpuset;
    /** qv_hw_obj_type_t to array of qvi_device_ids_ts. */
    int **device_tab;
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
 * Returns the number of device IDs present, including the sentinel value.
 */
int
qvi_line_hwpool_ndevids(
    qvi_line_hwpool_t *hwp,
    int devid_index
);

/**
 *
 */
int
qvi_line_hwpool_cp(
    qvi_line_hwpool_t *from,
    qvi_line_hwpool_t *to
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
    qvi_line_hwpool_t *hws
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

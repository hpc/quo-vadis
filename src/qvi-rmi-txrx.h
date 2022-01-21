/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rmi-txrx.h
 *
 * RMI line types and functions for sending and receiving data.
 */

#ifndef QVI_RMI_TXRX_H
#define QVI_RMI_TXRX_H

#include "qvi-common.h"
#include "qvi-bbuff.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Keep in sync with qvi_rmi_config_t structure.
#define QVI_RMI_CONFIG_PICTURE "ss"
#define QVI_RMI_HWRESOURCES_PICTURE "h"

typedef struct qvi_rmi_config_s {
    qvi_hwloc_t *hwloc;
    char *url;
    char *hwtopo_path;
} qvi_rmi_config_t;

typedef struct qvi_rmi_hwres_s {
    /** The resource bitmap. */
    hwloc_bitmap_t rmap;
    /** The resource type. */
    qv_hw_obj_type_t type;
} qvi_rmi_hwres_t;

typedef struct qvi_rmi_hwresources_s {
    /** Supported resource types. */
    enum {
        CPU = 0,
        GPU,
        LAST
    } type;
    /** Packed array of qvi_rmi_hwres_ts. */
    qvi_rmi_hwres_t rtab[LAST];
} qvi_rmi_hwresources_t;

/**
 *
 */
int
qvi_rmi_config_new(
    qvi_rmi_config_t **config
);

/**
 *
 */
void
qvi_rmi_config_free(
    qvi_rmi_config_t **config
);

/**
 *
 */
int
qvi_rmi_config_cp(
    qvi_rmi_config_t *from,
    qvi_rmi_config_t *to
);

/**
 *
 */
int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t *config
);

/**
 *
 */
int
qvi_rmi_hwresources_new(
    qvi_rmi_hwresources_t **hwres
);

/**
 *
 */
void
qvi_rmi_hwresources_free(
    qvi_rmi_hwresources_t **hwres
);

/**
 *
 */
int
qvi_rmi_hwresources_cp(
    qvi_rmi_hwresources_t *from,
    qvi_rmi_hwresources_t *to
);

/**
 *
 */
int
qvi_rmi_hwresources_pack(
    qvi_rmi_hwresources_t *hwres,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_rmi_hwresources_unpack(
    void *buff,
    qvi_rmi_hwresources_t *hwres
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

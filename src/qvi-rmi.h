/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rmi.h
 *
 * Resource Management and Inquiry
 */

#ifndef QVI_RMI_H
#define QVI_RMI_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"

#ifdef __cplusplus

struct qvi_rmi_config_s {
    /** Not sent, initialized elsewhere. */
    qvi_hwloc_t *hwloc = nullptr;
    /** Connection URL. */
    std::string url;
    /** Path to hardware topology file. */
    std::string hwtopo_path;
};

extern "C" {
#endif

// Forward declarations
struct qvi_rmi_server_s;
typedef struct qvi_rmi_server_s qvi_rmi_server_t;

/**
 *
 */
int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
);

/**
 *
 */
void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
);

/**
 *
 */
int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_s *config
);

/**
 *
 */
int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    bool block
);

/**
 *
 */
int
qvi_rmi_client_new(
    qvi_rmi_client_t **client
);

/**
 *
 */
void
qvi_rmi_client_free(
    qvi_rmi_client_t **client
);

/**
 *
 */
qvi_hwloc_t *
qvi_rmi_client_hwloc_get(
    qvi_rmi_client_t *client
);

/**
 *
 */
int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t task_id,
    hwloc_cpuset_t *cpuset
);

/**
 *
 */
int
qvi_rmi_task_set_cpubind_from_cpuset(
    qvi_rmi_client_t *client,
    pid_t task_id,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
int
qvi_rmi_scope_get_intrinsic_hwpool(
    qvi_rmi_client_t *client,
    pid_t task_id,
    qv_scope_intrinsic_t iscope,
    qvi_hwpool_s **hwpool
);

/**
 *
 */
int
qvi_rmi_obj_type_depth(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t type,
    int *depth
);

/**
 *
 */
int
qvi_rmi_get_nobjs_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
);

/**
 *
 */
int
qvi_rmi_get_device_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t dev_obj,
    int dev_i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
);

int
qvi_rmi_get_cpuset_for_nobjs(
    qvi_rmi_client_t *client,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
    hwloc_cpuset_t *result
);

#ifdef __cplusplus
}

/**
 *
 */
int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const std::string &url
);
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

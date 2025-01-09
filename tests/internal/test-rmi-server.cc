/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2023 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-rmi.cc
 */


#include "quo-vadis.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"
#include "common-test-utils.h" // IWYU pragma: keep

static int
server(
    const char *url
) {
    printf("# [%d] Starting Server (%s)\n", getpid(), url);

    char const *ers = NULL;
    const char *basedir = qvi_tmpdir();
    char *path = nullptr;
    double start = qvi_time(), end;
    qvi_rmi_config_s config;

    qvi_rmi_server_t *server = NULL;
    int rc = qvi_rmi_server_new(&server);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_new() failed";
        goto out;
    }

    qvi_hwloc_t *hwloc;
    rc = qvi_hwloc_new(&hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_init(hwloc, NULL);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    config.url = std::string(url);
    config.hwloc = hwloc;

    rc = qvi_hwloc_topology_export(
        hwloc, basedir, &path
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_export() failed";
        goto out;
    }

    config.hwtopo_path = std::string(path);
    free(path);

    rc = qvi_rmi_server_config(server, &config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_config() failed";
        goto out;
    }

    rc = qvi_rmi_server_start(server, false);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    end = qvi_time();
    printf("# [%d] Server Start Time %lf seconds\n", getpid(), end - start);
out:
    sleep(4);
    qvi_rmi_server_delete(&server);
    qvi_hwloc_delete(&hwloc);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void
usage(const char *appn)
{
    fprintf(stderr, "Usage: %s URL\n", appn);
}

int
main(
    int argc,
    char **argv
) {
    setbuf(stdout, NULL);

    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return server(argv[1]);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

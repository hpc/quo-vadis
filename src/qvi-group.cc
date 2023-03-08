/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022      Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group.cc
 */

#include "qvi-common.h"
#include "qvi-group.h"
#include "qvi-log.h"

/**
 * Group ID. Note that we pad its initial value so that other infrastructure
 * (e.g., QVI_MPI_GROUP_INTRINSIC_END) will never equal or exceed this value.
 */
static qvi_group_id_t group_id = 64;

int
qvi_group_next_id(
    qvi_group_id_t *gid
) {
    if (group_id == UINT64_MAX) {
        qvi_log_error("Group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = group_id++;
    return QV_SUCCESS;
}

void
qvi_group_free(
    qvi_group_t **group
) {
    if (!group) return;
    qvi_group_t *igroup = *group;
    if (!igroup) goto out;
    delete igroup;
out:
    *group = nullptr;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

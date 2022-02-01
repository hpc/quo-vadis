/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group.h
 */

#ifndef QVI_GROUP_H
#define QVI_GROUP_H

#include "qvi-common.h"

/**
 * Virtual base group class.
 */
struct qvi_group_s {
    qvi_group_s(void) = default;
    virtual ~qvi_group_s(void) = default;
    virtual int initialize(void) = 0;
    virtual int id(void) = 0;
    virtual int size(void) = 0;
    virtual int barrier(void) = 0;
};
typedef qvi_group_s qvi_group_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
inline void
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

/**
 * Returns the caller's group ID.
 */
inline int
qvi_group_id(
    qvi_group_t *group
) {
    return group->id();
}

/**
 * Returns the group's size.
 */
inline int
qvi_group_size(
    qvi_group_t *group
) {
    return group->size();
}

/**
 *
 */
inline int
qvi_group_barrier(
    qvi_group_t *group
) {
    return group->barrier();
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

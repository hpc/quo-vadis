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
 * @file quo-vadis.cc
 */

#include "qvi-scope.h"

int
qv_version(
    int *major,
    int *minor,
    int *patch
) {
    if (qvi_unlikely(!major || !minor || !patch)) {
        return QV_ERR_INVLD_ARG;
    }

    *major = PACKAGE_VERSION_MAJOR;
    *minor = PACKAGE_VERSION_MINOR;
    *patch = PACKAGE_VERSION_PATCH;

    return QV_SUCCESS;
}

int
qv_scope_bind_push(
    qv_scope_t *scope
) {
    if (qvi_unlikely(!scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_bind_push(scope);
    }
    qvi_catch_and_return();
}

int
qv_scope_bind_pop(
    qv_scope_t *scope
) {
    if (qvi_unlikely(!scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_bind_pop(scope);
    }
    qvi_catch_and_return();
}

int
qv_scope_bind_string(
    qv_scope_t *scope,
    qv_bind_string_format_t format,
    char **str
) {
    if (qvi_unlikely(!scope || !str)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_bind_string(scope, format, str);
    }
    qvi_catch_and_return();
}

int
qv_scope_free(
    qv_scope_t *scope
) {
    if (qvi_unlikely(!scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        qvi_scope_free(&scope);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

int
qv_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *nobjs
) {
    if (qvi_unlikely(!scope || !nobjs)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_nobjs(scope, obj, nobjs);
    }
    qvi_catch_and_return();
}

// TODO(skg) Rename to qv_scope_group_rank.
int
qv_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    if (qvi_unlikely(!scope || !taskid)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_group_rank(scope, taskid);
    }
    qvi_catch_and_return();
}

// TODO(skg) Rename to qv_scope_group_size.
int
qv_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    if (qvi_unlikely(!scope || !ntasks)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_group_size(scope, ntasks);
    }
    qvi_catch_and_return();
}

int
qv_scope_barrier(
    qv_scope_t *scope
) {
    if (qvi_unlikely(!scope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_barrier(scope);
    }
    qvi_catch_and_return();
}

// TODO(skg) Add Fortran interface.
int
qv_scope_create(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t hints,
    qv_scope_t **subscope
) {
    if (qvi_unlikely(!scope || (nobjs < 0) || !subscope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_create(
            scope, type, nobjs, hints, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_split(
    qv_scope_t *scope,
    int npieces,
    int color,
    qv_scope_t **subscope
) {
    if (qvi_unlikely(!scope || (npieces <= 0) | !subscope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        // We use the sentinel value QV_HW_OBJ_LAST to differentiate between
        // calls from split() and split_at(). Since this call doesn't have a
        // hardware type argument, we use QV_HW_OBJ_LAST as the hardware type.
        return qvi_scope_split(
            scope, npieces, color, QV_HW_OBJ_LAST, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_split_at(
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **subscope
) {
    if (qvi_unlikely(!scope || !subscope)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_split_at(
            scope, type, group_id, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_get_device_id(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int dev_index,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    if (qvi_unlikely(!scope || (dev_index < 0) || !dev_id)) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        return qvi_scope_device_id(
            scope, dev_obj, dev_index, id_type, dev_id
        );
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */

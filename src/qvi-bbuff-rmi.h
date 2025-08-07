/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff-rmi.h
 */

#ifndef QVI_BBUFF_RMI_H
#define QVI_BBUFF_RMI_H

#include "qvi-common.h"
#include "qvi-bbuff.h" // IWYU pragma: keep
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"

// 'Null' cpuset representation as a string.
static constexpr cstr_t QV_BUFF_RMI_NULL_CPUSET = "";

// Defines a zero message type.
typedef enum qvi_bbuff_rmi_zero_msg_e {
    QVI_BBUFF_RMI_ZERO_MSG = 0
} qvi_bbuff_rmi_zero_msg_t;

////////////////////////////////////////////////////////////////////////////////
// Pack
////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *,
    T
);

/**
 * Packs size_t.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    size_t data
) {
    return buff->append(&data, sizeof(data));
}

/**
 * Packs int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    int data
) {
    return buff->append(&data, sizeof(data));
}

/**
 * Packs qv_scope_create_hints_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qv_scope_create_hints_t data
) {
    const int dai = (int)data;
    return buff->append(&dai, sizeof(dai));
}

/**
 * Packs qv_hw_obj_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qv_hw_obj_type_t data
) {
    const int dai = (int)data;
    return buff->append(&dai, sizeof(dai));
}

/**
 * Packs qv_device_id_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qv_device_id_type_t data
) {
    const int dai = (int)data;
    return buff->append(&dai, sizeof(dai));
}

/**
 * Packs qv_scope_intrinsic_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qv_scope_intrinsic_t data
) {
    const int dai = (int)data;
    return buff->append(&dai, sizeof(dai));
}

/**
 * Packs cstr_t
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff *buff,
    cstr_t data
) {
    return buff->append(data, strlen(data) + 1);
}

/**
 * Packs std::string
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    const std::string &data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data.c_str());
}

/**
 * Packs std::vector<pid_t>
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    const std::vector<pid_t> &data
) {
    const size_t nelems = data.size();
    const int rc = buff->append(&nelems, sizeof(size_t));
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return buff->append(data.data(), data.size() * sizeof(pid_t));
}

/**
 * Packs const char *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    cstr_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs char *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    char *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_bbuff_rmi_zero_msg_t
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *,
    qvi_bbuff_rmi_zero_msg_t
) {
    return QV_SUCCESS;
}

/**
 * Packs hwloc_const_cpuset_t.
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff *buff,
    hwloc_const_cpuset_t data
) {
    // Protect against null data.
    if (qvi_unlikely(!data)) {
        return buff->append(
            QV_BUFF_RMI_NULL_CPUSET,
            strlen(QV_BUFF_RMI_NULL_CPUSET) + 1
        );
    }
    // Non-null data.
    char *datas = nullptr;
    int rc = qvi_hwloc::bitmap_asprintf(data, &datas);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // We are sending the string representation of the cpuset.
    rc = buff->append(datas, strlen(datas) + 1);
    free(datas);
    return rc;
}

/**
 * Packs hwloc_cpuset_t.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    hwloc_cpuset_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_hwloc_bitmap
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    const qvi_hwloc_bitmap &bitmap
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, bitmap.cdata());
}

/**
 * Packs hwloc_const_cpuset_t
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    hwloc_const_cpuset_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_hwpool_cpu
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    const qvi_hwpool_cpu &data
) {
    return data.packinto(buff);
}

/**
 * Packs qvi_hwpool_dev *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qvi_hwpool_dev *data
) {
    return data->packinto(buff);
}

/**
 * Packs qvi_hwpool *
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff *buff,
    const qvi_hwpool *data
) {
    return data->packinto(buff);
}

/**
 * Packs qvi_hwpool *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    qvi_hwpool *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_hwpool &
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff *buff,
    const qvi_hwpool &data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, &data);
}

inline int
qvi_bbuff_rmi_pack(
    qvi_bbuff *
) {
    // Base case
    return QV_SUCCESS;
}

template<typename T, typename... Types>
inline int
qvi_bbuff_rmi_pack(
    qvi_bbuff *buff,
    T&& arg,
    Types &&...args
) {
    const int rc = qvi_bbuff_rmi_pack_item(buff, std::forward<T>(arg));
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    return qvi_bbuff_rmi_pack(buff, std::forward<Types>(args)...);
}


////////////////////////////////////////////////////////////////////////////////
// Unpack
////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline int
qvi_bbuff_rmi_unpack_item(
    T,
    byte_t *buffpos,
    size_t *bytes_written
);

/**
 * Unpacks size_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    size_t *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

/**
 * Unpacks int.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    int *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

/**
 * Unpacks qv_scope_create_hints_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qv_scope_create_hints_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_scope_create_hints_t)oai;
    return QV_SUCCESS;
}

/**
 * Unpacks char *.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    char **s,
    byte_t *buffpos,
    size_t *bytes_written
) {
    const int nw = asprintf(s, "%s", buffpos);
    if (qvi_unlikely(nw == -1)) {
        *s = nullptr;
        *bytes_written = 0;
        return QV_ERR_OOR;
    }
    *bytes_written = nw + 1;
    return QV_SUCCESS;
}

/**
 * Unpacks std::string.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    std::string &str,
    byte_t *buffpos,
    size_t *bytes_written
) {
    str = std::string((const char *)buffpos);
    *bytes_written = strlen((char *)buffpos) + 1;
    return QV_SUCCESS;
}

/**
 * Unpacks qv_scope_intrinsic_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qv_scope_intrinsic_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_scope_intrinsic_t)oai;
    return QV_SUCCESS;
}

/**
 * Unpacks qv_device_id_type_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qv_device_id_type_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_device_id_type_t)oai;
    return QV_SUCCESS;
}

/**
 * Unpacks qv_hw_obj_type_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qv_hw_obj_type_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_hw_obj_type_t)oai;
    return QV_SUCCESS;
}

/**
 * Unpacks hwloc_cpuset_t *.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    hwloc_cpuset_t *cpuset,
    byte_t *buffpos,
    size_t *bytes_written
) {
    int rc = qvi_hwloc::bitmap_calloc(cpuset);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    char *const cpusets = (char *)buffpos;
    // Protect against empty data.
    if (strcmp(QV_BUFF_RMI_NULL_CPUSET, cpusets) != 0) {
        rc = qvi_hwloc::bitmap_sscanf(*cpuset, cpusets);
        if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    }
    *bytes_written = strlen(cpusets) + 1;
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_hwloc::bitmap_delete(cpuset);
    }
    return rc;
}

/**
 * Unpacks qvi_hwloc_bitmap.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwloc_bitmap &bitmap,
    byte_t *buffpos,
    size_t *bytes_written
) {
    hwloc_cpuset_t raw_cpuset = nullptr;
    int rc = qvi_bbuff_rmi_unpack_item(
        &raw_cpuset, buffpos, bytes_written
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *bytes_written = 0;
        return rc;
    }
    //
    rc = bitmap.set(raw_cpuset);
    qvi_hwloc::bitmap_delete(&raw_cpuset);
    return rc;
}

/**
 * Unpacks qvi_hwloc_bitmap *.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwloc_bitmap *bitmap,
    byte_t *buffpos,
    size_t *bytes_written
) {
    return qvi_bbuff_rmi_unpack_item(*bitmap, buffpos, bytes_written);
}

/**
 * Unpacks qvi_bbuff_rmi_zero_msg_t.
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_bbuff_rmi_zero_msg_t,
    byte_t *,
    size_t *bytes_written
) {
    *bytes_written = 0;
    return QV_SUCCESS;
}

/**
 * Unpacks qvi_hwpool_cpu
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool_cpu &cpu,
    byte_t *buffpos,
    size_t *bytes_written
) {
    return qvi_hwpool_cpu::unpack(buffpos, bytes_written, cpu);
}

/**
 * Unpacks qvi_hwpool_dev &
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool_dev &dev,
    byte_t *buffpos,
    size_t *bytes_written
) {
    return qvi_hwpool_dev::unpack(buffpos, bytes_written, dev);
}

/**
 * Unpacks qvi_hwpool *
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool *hwp,
    byte_t *buffpos,
    size_t *bytes_written
) {
    return qvi_hwpool::unpack(buffpos, bytes_written, *hwp);
}

/**
 * Unpacks std::vector<pid_t>
 */
inline int
qvi_bbuff_rmi_unpack_item(
    std::vector<pid_t> &pids,
    byte_t *buffpos,
    size_t *bytes_written
) {
    size_t bw = 0, total_bw = 0;

    size_t nelems = 0;
    int rc = qvi_bbuff_rmi_unpack_item(&nelems, buffpos, &bw);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    total_bw += bw;
    buffpos += bw;

    pid_t *pidsp = (pid_t *)buffpos;
    pids = std::vector<pid_t>(pidsp, pidsp + nelems);

    total_bw += nelems * sizeof(pid_t);

    *bytes_written = total_bw;

    return QV_SUCCESS;
}

/**
 * Base case for qvi_bbuff_rmi_unpack.
 */
inline int
qvi_bbuff_rmi_unpack(
    void *
) {
    return QV_SUCCESS;
}

template<typename T, typename... Types>
inline int
qvi_bbuff_rmi_unpack(
    void *data,
    T&& arg,
    Types &&...args
) {
    byte_t *pos = (byte_t *)data;
    size_t bytes_written = 0;
    const int rc = qvi_bbuff_rmi_unpack_item(
        std::forward<T>(arg),
        (byte_t *)data,
        &bytes_written
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    pos += bytes_written;
    return qvi_bbuff_rmi_unpack(pos, std::forward<Types>(args)...);
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
